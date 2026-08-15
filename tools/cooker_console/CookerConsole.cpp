#include "ArgumentParser.hpp"
#include "CookerDriver.hpp"
#include "CookerErrors.hpp"
#include "CookerOptions.hpp"
#include "OutputSink.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <print>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

static std::atomic<bool> k_WantsExit{false};

void HandleSIGINT(int signum)
{
    // Keep this function minimal. Just set the flag.
    k_WantsExit = true;
    std::println(stderr, "\n[Received Ctrl-C. Preparing to exit gracefully...]");
}

void RunCookingPipelineWithArgs(const lodestone::CookerOptions& options)
{
    using namespace lodestone;

    FileOutputSink sink{ options.OutputPath };
    const CookResult<CookStatistics> statistics = RunCook(options, sink);

    if (!statistics)
    {
        std::println(stdout, "[shader_cooker] cook failed: {}", ToString(statistics.error()));
        return;
    }

    std::println(stdout,
                 "[shader_cooker] cooked {} modules, {} variants, {} entrypoints, {} KiB of WGSL in {:.1f}ms "
                 "-> {}",
                 statistics.value().ModulesCooked,
                 statistics.value().VariantsCompiled,
                 statistics.value().EntryPointsCompiled,
                 statistics.value().TotalWgslBytes / 1024u,
                 statistics.value().ElapsedMilliseconds,
                 sink.Describe());
}

void BusySleep()
{
    // sleep for a bit so we're not just pegging the CPU in a tight loop if the user just hits Enter repeatedly
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

}

int main(int argc, char** argv)
{
    using namespace lodestone;
    std::signal(SIGINT, HandleSIGINT);
    std::println(std::cout, "Shader Cooker Live Tool initialized. Type arguments and press Enter to cook, or '--quit' to exit.");

    while (!k_WantsExit)
    {
        std::cout << "> ";
        
        std::string line;
        // Wait for user input. If EOF (Ctrl-D) is reached, break.
        if (!std::getline(std::cin, line))
        {
            break;
        }

        // If the signal handler fired while we were waiting on input, exit safely
        if (k_WantsExit)
        {
            break;
        }

        if (line.empty())
        {
            BusySleep();
            continue;
        }

        ArgumentParser parser{ line };
        std::vector<std::string_view> stringArgs = parser.GetArgs();
        if (stringArgs.empty())
        {
            continue;
        }

        // Check for manual exit commands
        if (stringArgs[0] == "--quit" || stringArgs[0] == "exit")
        {
            k_WantsExit = true;
            break;
        }

        if (stringArgs[0] == "--help" || stringArgs[0] == "-h")
        {
            std::println(std::cout, "Usage: [options]\n{}", lodestone::GetUsageText());
            continue;
        }

        std::println(std::cout, "Parsing arguments: {}", line);

        const CookResult<CookerOptions> options = ParseCommandLine(stringArgs);
        
        if (!options)
        {
            std::println(std::cout, "Error parsing arguments: {}", ToString(options.error()));
            continue;
        }

        RunCookingPipelineWithArgs(options.value());

        std::println(std::cout, "Cooking complete. Type arguments and press Enter to cook again, or '--quit' to exit.");
    }

    std::println(std::cout, "Exiting Shader Cooker Live Tool. Goodbye!");

    return 0;
}

