#include <iostream>
#include "test_runner.hpp"

constexpr const char* RED    = "\033[31m";
constexpr const char* GREEN  = "\033[32m";
constexpr const char* RESET  = "\033[0m";

inline int RunTests()
{
    for (auto& test : Tests) {
        CurrentTest = test.Name;

        size_t before = Failures.size();

        test.Function();
        TestsRun++;

        if (Failures.size() == before) {
            TestsPassed++;
            std::cout << GREEN << "[PASS] " << RESET << test.Name << "\n";
        } else {
            TestsFailed++;
            std::cout << RED << "[FAIL] " << RESET << test.Name << "\n";
        }
    }

    std::cout << "\n";

    for (const auto& f : Failures) {
        std::cout
            << f.Test << "\n"
            << "  "
            << f.File
            << ":"
            << f.Line
            << "\n"
            << f.Message
            << "\n\n";
    }

    std::cout << TestsPassed << " passed, " << TestsFailed << " failed\n";
    std::cout << AssertionsPassed << " assertions passed, " << AssertionsFailed << " failed\n";

    return TestsFailed != 0;
}

int main()
{
    return RunTests();
}
