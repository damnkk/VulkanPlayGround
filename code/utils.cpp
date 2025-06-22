#include "utils.hpp"
namespace Play{

    std::string GetUniqueName()
    {
        static uint64_t uniqueId = 0;
        return std::to_string(uniqueId++);
    }
}// namespace Play