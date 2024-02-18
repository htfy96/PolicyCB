#include "PolicyCB.hpp"
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

namespace {

using namespace PolicyCB;
using namespace std;
// Roughly equivalent to std::function<int(string, string)>
// 24 byte
using DynamicCB = Callback<int(string, string),
                           MovePolicy::DYNAMIC,
                           CopyPolicy::DYNAMIC,
                           DestroyPolicy::DYNAMIC,
                           SBOPolicy::DYNAMIC_GROWTH,
                           16>;
// Fixed size variant of the above. 16 bytes.
using FixedDynamicCB = Callback<int(string, string),
                                MovePolicy::DYNAMIC,
                                CopyPolicy::DYNAMIC,
                                DestroyPolicy::DYNAMIC,
                                SBOPolicy::FIXED_SIZE,
                                16>;

// Only allows trivially-copyable invocables to optimize
// calls. Faster to call than the above variant at the cost
// of slightly more memory.
// 32 bytes
using TrivialCB = Callback<int(string, string),
                           MovePolicy::TRIVIAL_ONLY,
                           CopyPolicy::TRIVIAL_ONLY,
                           DestroyPolicy::TRIVIAL_ONLY,
                           SBOPolicy::DYNAMIC_GROWTH,
                           16>;

// This is probably the most useful specilization I used
// in work - the 8 byte storage allows us to store a
// lambda with captured {this}
// 16 bytes
using FixedTrivialCB = Callback<int(string, string),
                                MovePolicy::TRIVIAL_ONLY,
                                CopyPolicy::TRIVIAL_ONLY,
                                DestroyPolicy::TRIVIAL_ONLY,
                                SBOPolicy::FIXED_SIZE,
                                8>;

// The std::function_ref equivalent. Basically just a function pointer
using FunctionRef = Callback<int(string, string),
                             MovePolicy::TRIVIAL_ONLY,
                             CopyPolicy::TRIVIAL_ONLY,
                             DestroyPolicy::TRIVIAL_ONLY,
                             SBOPolicy::NO_STORAGE,
                             0>;

using StdFunction = std::function<int(string, string)>;
template<typename CBType, typename ObjVecT>
int
runBenchmark(const ObjVecT& objVec)
{
    int temp = 2;

    BENCHMARK("Construction and destruction of 100000 callbacks")
    {
        std::vector<CBType> cbVec;
        for (int i = 0; i < 100000; ++i) {
            cbVec.emplace_back(objVec[i % objVec.size()]);
            ++temp;
        }
        return temp;
        cbVec.clear();
    };

    std::vector<CBType> cbVec;
    for (int i = 0; i < 100000; ++i) {
        cbVec.emplace_back(objVec[i % objVec.size()]);
        ++temp;
    }

    BENCHMARK("Random calls on 400 callbacks")
    {
        for (int i = 0; i < 1000000; ++i) {
            cbVec[std::rand() % 400]("hello"s, "world!"s);
            ++temp;
        }
    };

    BENCHMARK("RandomCopy and calls on 400 callbacks")
    {
        for (int i = 0; i < 1000000; ++i) {
            int nowIdx = std::rand() % 400;
            cbVec[nowIdx] = cbVec[std::rand() % cbVec.size()];
            cbVec[nowIdx]("hello2"s, "world!"s);
            ++temp;
        }
    };

    return temp;
}

TEST_CASE("Small obj benchmarks")
{
    vector<int (*)(string, string)> objVec{ [](string a, string b) -> int { return a.size() + b.size(); },
                                            [](string a, string b) -> int { return a.size() * b.size(); },
                                            [](string a, string b) -> int { return rand(); },
                                            [](string a, string b) -> int { return a.substr(1)[0] + b.size(); },
                                            [](string a, string b) -> int { return rand() + b.size(); } };

    SECTION("Dynamic CB")
    {
        runBenchmark<DynamicCB>(objVec);
    }
    SECTION("Fixed Dynamic CB")
    {
        runBenchmark<FixedDynamicCB>(objVec);
    }
    SECTION("Trivial CB")
    {
        runBenchmark<TrivialCB>(objVec);
    }
    SECTION("Fixed Trivial CB")
    {
        runBenchmark<FixedTrivialCB>(objVec);
    }
    SECTION("Function Ref")
    {
        runBenchmark<FunctionRef>(objVec);
    }
    SECTION("Std Function")
    {
        runBenchmark<StdFunction>(objVec);
    }
}

TEST_CASE("Mid obj benchmarks")
{
    int cnts[5] = { 0, 0, 5, 2, 3 };
    struct Mid
    {
        int* cnt;
        int operator()(const string& a, const string& b)
        {
            return (*cnt)++;
        }
    };

    vector<Mid> mids{ { cnts }, { cnts + 1 }, { cnts + 2 }, { cnts + 3 }, { cnts + 4 } };
    SECTION("Dynamic CB")
    {
        runBenchmark<DynamicCB>(mids);
    }
    SECTION("Fixed Dynamic CB")
    {
        runBenchmark<FixedDynamicCB>(mids);
    }
    SECTION("Trivial CB")
    {
        runBenchmark<TrivialCB>(mids);
    }
    SECTION("Fixed Trivial CB")
    {
        runBenchmark<FixedTrivialCB>(mids);
    }
    SECTION("Std Function")
    {
        runBenchmark<StdFunction>(mids);
    }
}
}