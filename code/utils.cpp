#include "utils.hpp"
namespace Play{

std::string GetUniqueName()
{
    static uint64_t uniqueId = 0;
    return std::to_string(uniqueId++);
}

VkAttachmentLoadOp RTState::getVkLoadOp() const
{
    switch (_loadOp)
    {
    case loadOp::eLoad: return VK_ATTACHMENT_LOAD_OP_LOAD;
    case loadOp::eDontCare: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    case loadOp::eClear: return VK_ATTACHMENT_LOAD_OP_CLEAR;
    default: return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
}

VkAttachmentStoreOp RTState::getVkStoreOp() const
{
    switch (_storeOp)
    {
    case storeOp::eStore: return VK_ATTACHMENT_STORE_OP_STORE;
    case storeOp::eDontCare: return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    default: return VK_ATTACHMENT_STORE_OP_STORE;
    }
}
}// namespace Play