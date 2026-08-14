#include "CookerErrors.hpp"
#include <magic_enum/magic_enum.hpp>

namespace lodestone
{

std::string_view ToString(CookError error) noexcept
{
    return magic_enum::enum_name(error);
}

} // namespace lodestone
