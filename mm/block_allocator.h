#pragma once

#include <kernel/spin_lock.h>
#include <lib/stdlib.h>
#include <lib/list_entry.h>

namespace Kernel
{

namespace Mm
{

class BlockAllocatorImpl
{
public:
    BlockAllocatorImpl();
    ~BlockAllocatorImpl();

    bool Setup(ulong startAddress, ulong endAddress, ulong blockSize);

    void* Alloc();

    bool IsOwner(void *block);

    void Free(void *block);

private:
    BlockAllocatorImpl(const BlockAllocatorImpl& other) = delete;
    BlockAllocatorImpl(BlockAllocatorImpl&& other) = delete;
    BlockAllocatorImpl& operator=(const BlockAllocatorImpl& other) = delete;
    BlockAllocatorImpl& operator=(BlockAllocatorImpl&& other) = delete;

    using ListEntry = Stdlib::ListEntry;

    static const ulong Magic = 0xbeadbeadbeadbead;

    struct BlockEntry
    {
        ulong Magic;
        ListEntry ListLink;
        ulong Frames[10];
        size_t NumFrames;
    };

    ListEntry FreeBlockList;
    ListEntry ActiveBlockList;

    ulong Usage;
    ulong Total;
    ulong StartAddress;
    ulong EndAddress;
    ulong BlockSize;
    SpinLock Lock;
};

}
}