#include "PolicyCB.hpp"
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <vector>

using namespace std;

struct Mid
{
    int* cnt;
    int f1(const string& a, const string& b)
    {
        return (*cnt)++;
    }
    int f2(const string& a, const string& b)
    {
        return (*cnt)++ + 1;
    }
    int f3(const string& a, const string& b)
    {
        return (*cnt)++ + 2;
    }
    int f4(const string& a, const string& b)
    {
        return (*cnt)++ + 3;
    }
    int f5(const string& a, const string& b)
    {
        return (*cnt)++ + 4;
    }
    int fUnd(const string& a, const string& b) __attribute__((noinline));
};

int
Mid::fUnd(const string& a, const string& b)
{
    return (*cnt)++;
}

int
f1(string a, string b)
{
    return a.size() + b.size();
}

int
f2(string a, string b)
{
    return a.size() + b.size() + 3;
}

int
f3(string a, string b)
{
    return a[0] < b[0] ? -1 : 3;
}

int
f4(string a, string b)
{
    return a.size() < b.size();
}

int
f5(string a, string b)
{
    return a[2] < b[3] + b[1] < a[0];
}
namespace {

using namespace PolicyCB;
// Roughly equivalent to std::function<int(string, string)>
// 24 byte
template<typename FT>
using DynamicCB =
  Callback<FT, MovePolicy::DYNAMIC, CopyPolicy::DYNAMIC, DestroyPolicy::DYNAMIC, SBOPolicy::DYNAMIC_GROWTH, 16>;
// Fixed size variant of the above. 16 bytes.
template<typename FT>
using FixedDynamicCB =
  Callback<FT, MovePolicy::DYNAMIC, CopyPolicy::DYNAMIC, DestroyPolicy::DYNAMIC, SBOPolicy::FIXED_SIZE, 16>;

// Only allows trivially-copyable invocables to optimize
// calls. Faster to call than the above variant at the cost
// of slightly more memory.
// 32 bytes
template<typename FT>
using TrivialCB = Callback<FT,
                           MovePolicy::TRIVIAL_ONLY,
                           CopyPolicy::TRIVIAL_ONLY,
                           DestroyPolicy::TRIVIAL_ONLY,
                           SBOPolicy::DYNAMIC_GROWTH,
                           16>;

template<typename FT>
using BigTrivialCB = Callback<FT,
                              MovePolicy::TRIVIAL_ONLY,
                              CopyPolicy::TRIVIAL_ONLY,
                              DestroyPolicy::TRIVIAL_ONLY,
                              SBOPolicy::DYNAMIC_GROWTH,
                              32>;

// This is probably the most useful specilization I used
// in work - the 8 byte storage allows us to store a
// lambda with captured {this}
// 16 bytes
template<typename FT>
using FixedTrivialCB = Callback<FT,
                                MovePolicy::TRIVIAL_ONLY,
                                CopyPolicy::TRIVIAL_ONLY,
                                DestroyPolicy::TRIVIAL_ONLY,
                                SBOPolicy::FIXED_SIZE,
                                8>;

// The std::function_ref equivalent. Basically just a function pointer
template<typename FT>
using FunctionRef = Callback<FT,
                             MovePolicy::TRIVIAL_ONLY,
                             CopyPolicy::TRIVIAL_ONLY,
                             DestroyPolicy::TRIVIAL_ONLY,
                             SBOPolicy::NO_STORAGE,
                             0>;

template<typename FT>
using StdFunction = std::function<FT>;

template<typename CBType, typename ObjVecT, typename... AdditionalArgsT>
int
runBenchmark(const ObjVecT& objVec, AdditionalArgsT&&... additionalArgs)
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
            cbVec[std::rand() % 400](std::forward<AdditionalArgsT>(additionalArgs)..., "hello"s, "world!"s);
            ++temp;
        }
    };

    BENCHMARK("RandomCopy and calls on 400 callbacks")
    {
        for (int i = 0; i < 1000000; ++i) {
            int nowIdx = std::rand() % 400;
            cbVec[nowIdx] = cbVec[std::rand() % cbVec.size()];
            cbVec[nowIdx](std::forward<AdditionalArgsT>(additionalArgs)..., "hello2"s, "world!"s);
            ++temp;
        }
    };

    return temp;
}

TEST_CASE("Small obj benchmarks")
{
    vector<int (*)(string, string)> objVec{ f1, f2, f3, f4, f5 };

    using FT = int(string, string);
    SECTION("Dynamic CB")
    {
        runBenchmark<DynamicCB<FT>>(objVec);
    }
    SECTION("Fixed Dynamic CB")
    {
        runBenchmark<FixedDynamicCB<FT>>(objVec);
    }
    SECTION("Trivial CB")
    {
        runBenchmark<TrivialCB<FT>>(objVec);
    }
    SECTION("Fixed Trivial CB")
    {
        runBenchmark<FixedTrivialCB<FT>>(objVec);
    }
    SECTION("Function Ref")
    {
        runBenchmark<FunctionRef<FT>>(objVec);
    }
    SECTION("Std Function")
    {
        runBenchmark<StdFunction<FT>>(objVec);
    }
}

TEST_CASE("Mid obj benchmarks")
{
    int cnts[5] = { 0, 0, 5, 2, 3 };
    using FT = int(string, string);

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
        runBenchmark<DynamicCB<FT>>(mids);
    }
    SECTION("Fixed Dynamic CB")
    {
        runBenchmark<FixedDynamicCB<FT>>(mids);
    }
    SECTION("Trivial CB")
    {
        runBenchmark<TrivialCB<FT>>(mids);
    }
    SECTION("Fixed Trivial CB")
    {
        runBenchmark<FixedTrivialCB<FT>>(mids);
    }
    SECTION("Std Function")
    {
        runBenchmark<StdFunction<FT>>(mids);
    }
}

TEST_CASE("Member function pointer")
{
    int cnt = 0;

    Mid mid{ &cnt };
    vector<int (Mid::*)(const string&, const string&)> memPtrs{
        &Mid::fUnd, &Mid::fUnd, &Mid::fUnd, &Mid::fUnd, &Mid::fUnd
    };
    using FT = int(Mid*, const string&, const string&);
    SECTION("Dynamic CB")
    {
        runBenchmark<DynamicCB<FT>>(memPtrs, &mid);
    }
    SECTION("Big Trivial CB")
    {
        runBenchmark<BigTrivialCB<FT>>(memPtrs, &mid);
    }
}

TEST_CASE("Member function lambda")
{
    int cnt = 0;
    Mid mid{ &cnt };
    auto obj = [](Mid* mid, const string& a, const string& b) { return mid->fUnd(a, b); };
    auto memPtrs = vector{ obj, obj, obj, obj, obj };
    using FT = int(Mid*, const string&, const string&);
    SECTION("Dynamic CB")
    {
        runBenchmark<DynamicCB<FT>>(memPtrs, &mid);
    }
    SECTION("Big Trivial CB")
    {
        runBenchmark<BigTrivialCB<FT>>(memPtrs, &mid);
    }
}
}