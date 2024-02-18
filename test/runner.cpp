
#include "PolicyCB.hpp"
#include <iostream>
#include <string>
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

DynamicCB
getCB1()
{
    return DynamicCB{ [](string a, string b) { return a.size() + b.size(); } };
}

DynamicCB
getLargeCB1()
{
    static string largeString(1024, 'a');
    return DynamicCB{ [largeString = largeString](string a, string b) {
        return a.size() + b.size() + largeString.size();
    } };
}

FixedDynamicCB
getCB2()
{
    return FixedDynamicCB{ [](string a, string b) { return a.size() + b.size(); } };
}

/*
FixedDynamicCB
getLargeCB2()
{
    static string largeString(1024, 'a');
    return FixedDynamicCB{ [largeString = largeString](string a, string b) {
        return a.size() + b.size() + largeString.size();
    } };
}
*/

TrivialCB
getCB3()
{
    return TrivialCB{ [](string a, string b) { return a.size() + b.size(); } };
}

/*
TrivialCB
getLargeCB3()
{
    static string largeString(1024, 'a');
    return TrivialCB{ [largeString = largeString](string a, string b) {
        return a.size() + b.size() + largeString.size();
    } };
}
*/

FixedTrivialCB
getCB4()
{
    return FixedTrivialCB{ [](string a, string b) { return a.size() + b.size(); } };
}

FixedTrivialCB
getMidCB4()
{
    double d = 1.5;
    return FixedTrivialCB{ [d = d](string a, string b) { return a.size() + b.size() + d; } };
}
int
main()
{
    cout << getCB1()("hello", "world") << endl;
    cout << getCB1()("very very long string therer's even more string than you even think", "test test stete") << endl;

    cout << getLargeCB1()("hello", "world") << endl;
    static_assert(sizeof(getCB1()) == 24);

    DynamicCB anotherLargeCB1 = getLargeCB1();
    cout << anotherLargeCB1("hello", "world") << endl;

    cout << getCB2()("hello", "world") << endl;
    cout << getCB2()("very very long string therer's even more string than you even think", "test test stete") << endl;
    static_assert(sizeof(getCB2()) == 16);

    // cout << getLargeCB2()("hello", "world") << endl; // compile error

    cout << getCB3()("hello", "world") << endl;
    static_assert(sizeof(getCB3()) == 32);
    TrivialCB anotherTrivialCB = getCB3();
    TrivialCB anotherTrivialCB2{ anotherTrivialCB };
    anotherTrivialCB2("hello", "world");
    /*
    cout << getLargeCB3()("very very long string therer's even more string than you even think", "test test") << endl;
    */

    cout << getCB4()("hello", "world") << endl;
    static_assert(sizeof(getCB4()) == 16);
    cout << getMidCB4()("hello", "world") << endl;

    cout << FunctionRef([](string a, string b) -> int { return a.size() + b.size(); })("hello", "world") << endl;

    FixedTrivialCB cb5{ [](std::string_view a, std::string_view b) -> int { return a.size() + b.size(); } };
    cout << cb5("hello", "world") << endl;

    using CStyleGetStringSizeCB = Callback<size_t(const string&),
                                           MovePolicy::TRIVIAL_ONLY,
                                           CopyPolicy::TRIVIAL_ONLY,
                                           DestroyPolicy::TRIVIAL_ONLY,
                                           SBOPolicy::FIXED_SIZE,
                                           8>;
    // Compilation error! Member function is very weird and takes more than 8 bytes
    // CStyleGetStringSizeCB cb6{ &string::size }; // Please stop using member function pointers in app interface
    CStyleGetStringSizeCB cb6{ [](const string& s) { return s.size(); } };
    cout << cb6("hello") << endl;
}