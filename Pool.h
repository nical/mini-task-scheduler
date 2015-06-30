#ifndef MOZILLA_GFX_POOL
#define MOZILLA_GFX_POOL


#include <string>
#include <string.h>
#include <vector>
#include <list>
#include <pthread.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

// TODO[nical]
typedef int ptrdiff_t;

namespace mozilla {
namespace gfx {

/**
 * A simple pool allocator for plain data structures (without destructor).
 */
class Pool {
public:
    Pool(size_t aStorageSize)
    {
        mStorage = (uint8_t*)malloc(aStorageSize);
        mSize = aStorageSize;
        mCursor = 0;
        mIsGrowable = false;
    }

    template<typename T>
    ptrdiff_t AddItem(const T& aValue)
    {
        if (mCursor + sizeof(T) > mSize) {
            if (!mIsGrowable) {
                return -1;
            }
            uint8_t* newStorage = (uint8_t*)malloc(mSize*2);
            memcpy(newStorage, mStorage, mSize);
            free(mStorage);
            mStorage = newStorage;
        }
        memcpy(mStorage + mCursor, (uint8_t*)&aValue, sizeof(T));
        ptrdiff_t offset = mCursor;
        mCursor += sizeof(T);
        return offset;
    }

    void* GetStorage(ptrdiff_t offset = 0) { return offset >= 0 ? mStorage + offset : nullptr; }

    ~Pool()
    {
        free(mStorage);
    }

protected:
    uint8_t* mStorage;
    uint32_t mSize;
    ptrdiff_t mCursor;
    bool mIsGrowable;
};

class GrowablePool : public Pool
{
public:
    GrowablePool(size_t aBufferSize)
    : Pool(aBufferSize)
    {
        mIsGrowable = true;
    }
};

} // namespace
} // namespace

#endif
