#ifndef UTILS_HPP
#define UTILS_HPP

#define CUSTOM_NAME_VK(DEBUGER,_x ) DEBUGER.setObjectName(_x, (std::string(CLASS_NAME) + std::string("::") + std::string(#_x " (") + NAME_FILE_LOCATION).c_str())

#endif // UTILS_HPP