#ifndef UTILS_HPP
#define UTILS_HPP
#include <string>
#define CUSTOM_NAME_VK(DEBUGER,_x ) DEBUGER.setObjectName(_x, (std::string(CLASS_NAME) + std::string("::") + std::string(#_x " (") + NAME_FILE_LOCATION).c_str())
namespace Play{
    std::string GetUniqueName();
} // namespace Play
#endif // UTILS_HPP