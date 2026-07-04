#include "Utils.h"

namespace Play
{
namespace
{
char toLowerAscii(char value)
{
    if (value >= 'A' && value <= 'Z')
    {
        return static_cast<char>(value - 'A' + 'a');
    }
    return value;
}
} // namespace

bool asciiEqualsIgnoreCase(const char* lhs, const char* rhs)
{
    if (!lhs || !rhs)
    {
        return false;
    }

    while (*lhs && *rhs)
    {
        if (toLowerAscii(*lhs) != toLowerAscii(*rhs))
        {
            return false;
        }
        ++lhs;
        ++rhs;
    }

    return *lhs == *rhs;
}
} // namespace Play
