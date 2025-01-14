#include "include/fib_extension.hpp"
#define DUCKDB_EXTENSION_MAIN

#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include "fib_extension.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

namespace duckdb {

unique_ptr<FunctionData> FibBind(ClientContext &context,
                                 TableFunctionBindInput &input,
                                 vector<LogicalType> &return_types,
                                 vector<string> &names) {
    // get arg
    auto max = IntegerValue::Get(input.inputs[0]);
    if (max < 0 ||
        max > 93) { // max >= 0 && max <= 93, at 94 and above overflows
        throw BinderException(
            "Invalid input: n must be between 0 and 93 (inclusive). "
            "Values less than 0 are not allowed, and values greater than 93 "
            "exceed the maximum limit for 64-bit unsigned integers.");
    }

    auto bind_data = make_uniq<FibBindData>();
    bind_data->max = max;

    // schema
    return_types.push_back(LogicalType::UBIGINT);
    names.emplace_back("i");

    return_types.push_back(LogicalType::UBIGINT);
    names.emplace_back("f");

    return std::move(bind_data);
}

static unique_ptr<GlobalTableFunctionState>
FibInitGlobal(ClientContext &context, TableFunctionInitInput &input) {
    auto &bind_data = input.bind_data->Cast<FibBindData>();
    return make_uniq<FibGlobalState>();
}

unique_ptr<LocalTableFunctionState>
FibInitLocal(ExecutionContext &context, TableFunctionInitInput &input,
             GlobalTableFunctionState *global_state_p) {
    return make_uniq<FibLocalState>();
}

static void FibFunction(ClientContext &context, TableFunctionInput &data_p,
                        DataChunk &output) {
    // get bind data and local state
    auto &bind_data = data_p.bind_data->Cast<FibBindData>();
    auto &global_state = data_p.global_state->Cast<FibGlobalState>();
    auto &local_state = data_p.local_state->Cast<FibLocalState>();

    // determine how many rows to insert
    if (local_state.curr_index == bind_data.max) {
        output.SetCardinality(0);
        return;
    }
    auto remaining = bind_data.max - local_state.curr_index;
    auto size = MinValue<idx_t>(remaining, STANDARD_VECTOR_SIZE);

    // set cardinality of output
    output.SetCardinality(size);

    // get chunk
    auto ith_vals = FlatVector::GetData<uint64_t>(output.data[0]);
    auto fib_vals = FlatVector::GetData<uint64_t>(output.data[1]);

    idx_t i = 0;

    // first fibonacci value
    if (local_state.curr_index == 0) {
        ith_vals[0] = 0;
        fib_vals[0] = local_state.a;
        local_state.curr_index++;
        i++;
    }

    // second fibonacci value
    if (local_state.curr_index == 1 && i < size) {
        ith_vals[1] = 1;
        fib_vals[1] = local_state.b;
        local_state.curr_index++;
        i++;
    }

    // fill chunk
    for (; i < size; i++) {
        uint64_t next = local_state.a + local_state.b;
        local_state.a = local_state.b;
        local_state.b = next;
        fib_vals[i] = next;
        ith_vals[i] = local_state.curr_index;
        local_state.curr_index++;
    }
}

void FibExtension::Load(DuckDB &db) {
    TableFunction table_function("fibonacci", {LogicalType::INTEGER},
                                 FibFunction, FibBind, FibInitGlobal,
                                 FibInitLocal);
    ExtensionUtil::RegisterFunction(*db.instance, table_function); // Register
}

std::string FibExtension::Name() { return "fib"; }

std::string FibExtension::Version() const { return ""; }

} // namespace duckdb

extern "C" {

DUCKDB_EXTENSION_API void fib_init(duckdb::DatabaseInstance &db) {
    duckdb::DuckDB db_wrapper(db);
    db_wrapper.LoadExtension<duckdb::FibExtension>();
}

DUCKDB_EXTENSION_API const char *fib_version() {
    return duckdb::DuckDB::LibraryVersion();
}
}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif
