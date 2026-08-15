#include "CookerDriver.hpp"
#include "CookerErrors.hpp"
#include "CookerOptions.hpp"
#include "OutputSink.hpp"

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <print>
#include <string_view>
#include <thread>
#include <vector>

int main(int argc, char** argv)
{
    std::vector<std::string_view> arguments;
    arguments.reserve(static_cast<size_t>(argc));
    for (int i = 1; i < argc; ++i)
    {
        arguments.emplace_back(argv[i]);
    }

    const lodestone::CookResult<lodestone::CookerOptions> options =
        lodestone::ParseCommandLine(arguments);
    if (!options)
    {
        std::println(stderr,
                     "[shader_cooker] {}\n{}",
                     lodestone::ToString(options.error()),
                     lodestone::GetUsageText());
        return 1;
    }

    lodestone::FileOutputSink sink{ options.value().OutputPath };
    const lodestone::CookResult<lodestone::CookStatistics> statistics =
        lodestone::RunCook(options.value(), sink);

    if (!statistics)
    {
        std::println(stderr, "[shader_cooker] cook failed: {}", lodestone::ToString(statistics.error()));
        return 1;
    }

    std::println(stderr,
                 "[shader_cooker] cooked {} modules, {} variants, {} entrypoints, {} KiB of WGSL in {:.1f}ms "
                 "-> {}",
                 statistics.value().ModulesCooked,
                 statistics.value().VariantsCompiled,
                 statistics.value().EntryPointsCompiled,
                 statistics.value().TotalWgslBytes / 1024u,
                 statistics.value().ElapsedMilliseconds,
                 sink.Describe());

    return 0;
}
