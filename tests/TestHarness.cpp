#include "TestHarness.hpp"
#include <cmath>
#include <print>
#include <string_view>

namespace lodestone::tests
{

TestRunner::TestRunner(std::string_view suite_name) noexcept :
    suiteName{ suite_name },
    currentSection{ },
    sectionHeadingPrinted{ false },
    checksRun{ 0u },
    failures{ 0u }
{
}

void TestRunner::BeginSection(std::string_view name) noexcept
{
    currentSection = name;
    sectionHeadingPrinted = false;
}

void TestRunner::Check(bool condition, std::string_view description) noexcept
{
    ++checksRun;
    if (condition)
    {
        return;
    }
    ++failures;
    reportFailure(description);
}

int TestRunner::Report() const noexcept
{
    if (failures == 0u)
    {
        std::println("{} : {} checks passed", suiteName, checksRun);
        return 0;
    }

    std::println("{} : {} of {} checks FAILED", suiteName, failures, checksRun);
    return 1;
}

size_t TestRunner::Failures() const noexcept
{
    return failures;
}

void TestRunner::reportFailure(std::string_view description) noexcept
{
    if (!sectionHeadingPrinted && !currentSection.empty())
    {
        std::println("  [{}]", currentSection);
        sectionHeadingPrinted = true;
    }
    std::println("    FAIL: {}", description);
}

} // namespace lodestone::tests
