#ifndef UTILS_HPP
#define UTILS_HPP
#include <string>
#define CUSTOM_NAME_VK(DEBUGER,_x ) DEBUGER.setObjectName(_x, (std::string(CLASS_NAME) + std::string("::") + std::string(#_x " (") + NAME_FILE_LOCATION).c_str())
namespace Play{
    template<class T>
    inline void hash_combine(std::size_t& seed, const T& v) {
        std::hash<T> hasher;
        seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }

    template<typename T,typename ...Args>
    inline size_t hash_combine(std::size_t& seed, const T& v, const Args&... args) {
        hash_combine(seed, v);
        (hash_combine(seed, args), ...);
        return seed;
    }

    std::string GetUniqueName();
} // namespace Play
#endif // UTILS_HPP