#include "CookerDriver.hpp"
#include "CookerOptions.hpp"
#include "OutputSink.hpp"
#include <print>
#include <string_view>
#include <vector>

int main(int argc, char** argv)
{
    std::vector<std::string_view> arguments;
    arguments.reserve(static_cast<size_t>(argc));
    for (int i = 1; i < argc; ++i)
    {
        arguments.emplace_back(argv[i]);
    }

    const velox::cooker::CookResult<velox::cooker::CookerOptions> options =
        velox::cooker::ParseCommandLine(arguments);
    if (!options)
    {
        std::println(stderr,
                     "[shader_cooker] {}\n{}",
                     velox::cooker::ToString(options.error()),
                     velox::cooker::GetUsageText());
        return 1;
    }

    velox::cooker::FileOutputSink sink{ options.value().OutputPath };
    const velox::cooker::CookResult<velox::cooker::CookStatistics> statistics =
        velox::cooker::RunCook(options.value(), sink);

    if (!statistics)
    {
        std::println(stderr, "[shader_cooker] cook failed: {}", velox::cooker::ToString(statistics.error()));
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
