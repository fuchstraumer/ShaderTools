#include "CookerErrors.hpp"
#include <magic_enum/magic_enum.hpp>

namespace velox::cooker
{

std::string_view ToString(CookError error) noexcept
{
    return magic_enum::enum_name(error);
}

} // namespace velox::cooker
