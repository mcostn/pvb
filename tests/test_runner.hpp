#pragma once

#include <iostream>
#include <vector>

using TestFn = void(*)();

struct Test
{
    const char* Name;
    TestFn Function;
};

struct Failure
{
    const char* Test;
    const char* File;
    int Line;
    std::string Message;
};


inline int TestsRun = 0;
inline int TestsPassed = 0;
inline int TestsFailed = 0;

inline int AssertionsPassed = 0;
inline int AssertionsFailed = 0;

inline const char* CurrentTest = "";

inline std::vector<Test> Tests;
inline std::vector<Failure> Failures;


#define TEST(name)                              \
    void name();                                \
                                                \
    struct name##_Reg                           \
    {                                           \
        name##_Reg()                            \
        {                                       \
            Tests.push_back({ #name, name });   \
        }                                       \
    };                                          \
                                                \
    inline name##_Reg name##_Instance;          \
                                                \
    void name()


#define EXPECT(cond)                                \
    do {                                            \
        if (!(cond)) {                              \
                                                    \
            ++AssertionsFailed;                     \
                                                    \
            Failures.push_back({                    \
                CurrentTest,                        \
                __FILE__,                           \
                __LINE__,                           \
                "EXPECT(" #cond ")"                 \
            });                                     \
        }                                           \
        else {                                      \
            ++AssertionsPassed;                     \
        }                                           \
    } while (0)

#define EXPECT_EQ(a, b)                             \
    do {                                            \
        auto lhs = (a);                             \
        auto rhs = (b);                             \
                                                    \
        if (!(lhs == rhs)) {                        \
            ++AssertionsFailed;                     \
                                                    \
            std::stringstream ss;                   \
            ss << "Expected:\n"                     \
               << "---START---\n"                   \
               << rhs                               \
               << "----END----\n"                   \
               << "\nActual:\n"                     \
               << "---START---\n"                   \
               << lhs                               \
               << "----END----\n";                  \
                                                    \
            Failures.push_back({                    \
                CurrentTest,                        \
                __FILE__,                           \
                __LINE__,                           \
                ss.str()                            \
            });                                     \
        }                                           \
        else {                                      \
            ++AssertionsPassed;                     \
        }                                           \
    } while (0)
