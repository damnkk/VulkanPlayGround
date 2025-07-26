#ifndef RDG_HANDLE_H
#define RDG_HANDLE_H
#include "RDGResources.h"
#include "nvp/NvFoundation.h"
namespace Play::RDG{

/* Numeric constants
 *****************************************************************************/

#define MIN_uint8		((uint8_t)	0x00)
#define	MIN_uint16		((uint16_t)	0x0000)
#define	MIN_uint32		((uint32_t)	0x00000000)
#define MIN_uint64		((uint64_t)	0x0000000000000000)
#define MIN_int8		((int8_t)		-128)
#define MIN_int16		((int16_t)		-32768)
#define MIN_int32		((int32_t)		0x80000000)
#define MIN_int64		((int64_t)		0x8000000000000000)

#define MAX_uint8		((uint8_t)	0xff)
#define MAX_uint16		((uint16_t)	0xffff)
#define MAX_uint32		((uint32_t)	0xffffffff)
#define MAX_uint64		((uint64_t)	0xffffffffffffffff)
#define MAX_int8		((int8_t)		0x7f)
#define MAX_int16		((int16_t)		0x7fff)
#define MAX_int32		((int32_t)		0x7fffffff)
#define MAX_int64		((int64_t)		0x7fffffffffffffff)

#define MIN_flt			(1.175494351e-38F)			/* min positive value */
#define MAX_flt			(3.402823466e+38F)
#define MIN_dbl			(2.2250738585072014e-308)	/* min positive value */
#define MAX_dbl			(1.7976931348623158e+308)	
template <typename NumericType> struct TNumericLimits;

template<>
struct TNumericLimits<uint8_t> 
{
	typedef uint8_t NumericType;
	
	static constexpr NumericType Min()
	{
		return MIN_uint8;
	}

	static constexpr NumericType Max()
	{
		return MAX_uint8;
	}

	static constexpr NumericType Lowest()
	{
		return Min();
	}
};


template<>
struct TNumericLimits<uint16_t> 
{
	typedef uint16_t NumericType;
	
	static constexpr NumericType Min()
	{
		return MIN_uint16;
	}

	static constexpr NumericType Max()
	{
		return MAX_uint16;
	}

	static constexpr NumericType Lowest()
	{
		return Min();
	}
};

template<>
struct TNumericLimits<uint32_t> 
{
	typedef uint32_t NumericType;
	
	static constexpr NumericType Min()
	{
		return MIN_uint32;
	}

	static constexpr NumericType Max()
	{
		return MAX_uint32;
	}
	
	static constexpr NumericType Lowest()
	{
		return Min();
	}
};

template<typename LocalObjectType,typename LocalIndexType>
class RDGHandle{
public:
    using ObjectType = LocalObjectType;
    using IndexType = LocalIndexType;
    RDGHandle() = default;
    RDGHandle(IndexType index) {
        NV_ASSERT(index>=0&& index < InvalidIndex);
        this->index = (IndexType)index;
    }
    bool operator==(const RDGHandle& other) const {
        return index == other.index;
    }
    bool isValid()const{
        return index != InvalidIndex;
    }
    static const RDGHandle Null;
    static const IndexType InvalidIndex =  TNumericLimits<IndexType>::Max();
    IndexType index = InvalidIndex;

};

template<typename LocalObjectType,typename LocalIndexType>
const RDGHandle<LocalObjectType, LocalIndexType> RDGHandle<LocalObjectType, LocalIndexType>::Null ;
class RDGTexture;
class RDGBuffer;
using TextureHandle = RDGHandle<RDGTexture, uint32_t>;
using BufferHandle = RDGHandle<RDGBuffer, uint32_t>;
using TextureHandleArray = std::vector<TextureHandle>;
using BufferHandleArray = std::vector<BufferHandle>;


} // namespace Play::RDG

#endif // RDG_HANDLE_H