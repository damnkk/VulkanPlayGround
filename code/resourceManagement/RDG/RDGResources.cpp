#include "RDGResources.h"

namespace Play::RDG{

// 基类实现
std::optional<uint32_t> RDGResourceBase::getLastProducer(uint32_t currentPassIdx) const {
    if (_producers.empty()) return std::nullopt;
    for (auto it = _producers.rbegin(); it != _producers.rend(); ++it) {
        if (it->has_value() && *it < currentPassIdx) {
            return *it;
        }
    }
    return std::nullopt;
}

std::optional<uint32_t> RDGResourceBase::getLastReader(uint32_t currentPassIdx) const {
    if (_readers.empty()) return std::nullopt;
    for (auto it = _readers.rbegin(); it != _readers.rend(); ++it) {
        if (it->has_value() && *it < currentPassIdx) {
            return *it;
        }
    }
    return std::nullopt;
}

}// namespace Play::RDG
