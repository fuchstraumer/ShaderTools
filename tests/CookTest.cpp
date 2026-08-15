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

    
    const lodestone::CookResult<lodestone::CookerOptions> options = lodestone::ParseCommandLine(arguments);

    // the library logs unconditionally to stderr; redirect it to silence the lock contention across threads
    std::freopen("NUL", "w", stderr);

    const size_t numThreads = std::thread::hardware_concurrency();
    std::vector<std::thread> jobThreads;
    // have each thread take it's own copy of the arguments, and write to it's own output file.
    for (size_t threadIndex = 0; threadIndex < numThreads; ++threadIndex)
    {
        jobThreads.emplace_back([options, threadIndex]()
            {
                for (size_t i = 0; i < 100; ++i)
                {
                    lodestone::CookerOptions threadOptions = options.value();
                    // give each thread it's own output dir, too
                    std::filesystem::path outputDir =
                        std::filesystem::current_path() / std::format("thread_{}", threadIndex);
                    if (!std::filesystem::exists(outputDir))
                    {
                        std::filesystem::create_directories(outputDir);
                    }
                    threadOptions.OutputPath = outputDir / std::format("output_thread_{}.dump", threadIndex);
                    // disable multithreading, because we're doing it manually
                    threadOptions.MultithreadEntryPointCodegen = false;
                    lodestone::FileOutputSink sink{ threadOptions.OutputPath };
                    const lodestone::CookResult<lodestone::CookStatistics> statistics =
                        lodestone::RunCook(threadOptions, sink);
                    if (!statistics)
                    {
                        std::println(stderr,
                                     "[shader_cooker] cook failed on thread {} : {}",
                                     threadIndex,
                                     lodestone::ToString(statistics.error()));
                    }
                }
            });
    }

    for (std::thread& jobThread : jobThreads)
    {
        jobThread.join();
    }

    return 0;
}
