#pragma once

#include "duckdb.hpp"

namespace duckdb {

class FibExtension : public Extension {
public:
	void Load(DuckDB &db) override;
	std::string Name() override;
    std::string Version() const override;
};


struct FibBindData : public TableFunctionData {
    uint64_t max;
};

struct FibGlobalState : public GlobalTableFunctionState {
	FibGlobalState() {};
	~FibGlobalState() override {}
	idx_t MaxThreads() const override { return 1; /* single threaded */ };
private:
	mutable mutex main_mutex;
};

struct FibLocalState : public LocalTableFunctionState {
public:
	uint64_t a; // prev fibonacci number
    uint64_t b; // curr fibonacci number
    uint64_t curr_index; // index

 	FibLocalState() : a(0), b(1), curr_index(0) {}
};

} // namespace duckdb
