#include "nvvk/resource_allocator.hpp"
#include "Resource.h"
namespace Play{
    class PlayAllocator: public nvvk::ResourceAllocator
    {
    public:
        PlayAllocator() = default;
        ~PlayAllocator(){}
        void* map(Buffer& buffer);
        void* unmap(Buffer& buffer);
    };

}// namespace Play