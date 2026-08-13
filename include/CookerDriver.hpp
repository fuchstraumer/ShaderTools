#pragma once
#ifndef VELOX_SHADER_COOKER_DRIVER_HPP
#define VELOX_SHADER_COOKER_DRIVER_HPP
#include "CookerErrors.hpp"
#include "CookerOptions.hpp"
#include "OutputSink.hpp"
#include <cstdint>

/** The execution loop, separated from `main` so the cooker can also be driven in-process by a watcher
 * or by the engine itself. */
namespace velox::cooker
{

struct CookStatistics
{
    uint32_t ModulesCooked{ 0u };
    uint32_t VariantsCompiled{ 0u };
    uint32_t EntryPointsCompiled{ 0u };
    uint32_t ReflectionMismatches{ 0u };
    size_t TotalWgslBytes{ 0u };
    size_t GeneratedSourceBytes{ 0u };
    double ElapsedMilliseconds{ 0.0 };
};

CookResult<CookStatistics> RunCook(const CookerOptions& options, OutputSink& sink);

} // namespace velox::cooker

#endif // !VELOX_SHADER_COOKER_DRIVER_HPP
