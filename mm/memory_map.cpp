#include "memory_map.h"
#include <kernel/trace.h>
#include <lib/stdlib.h>

extern "C" char KernelEnd;
extern "C" char KernelStart;

namespace Kernel
{

namespace Mm
{

MemoryMap::MemoryMap()
    : Size(0)
{
}

bool MemoryMap::AddRegion(ulong addr, ulong len, ulong type)
{
    if (Size >= Stdlib::ArraySize(Region))
        return false;

    auto& region = Region[Size];
    region.Addr = addr;
    region.Len = len;
    region.Type = type;

    Size++;

    return true;
}

bool MemoryMap::FindRegion(ulong base, ulong limit, ulong& start, ulong& end)
{
    start = 0;
    end = 0;
    for (size_t i = 0; i < Size; i++)
    {
        auto& region = Region[i];
        if (region.Type != 1)
            continue;

        if (region.Len == 0)
            continue;

        if (region.Addr + region.Len <= base)
            continue;

        ulong regionBase = (region.Addr < base) ? base : region.Addr;
        ulong regionLength = (region.Addr < base) ?
            (region.Len - (base - region.Addr)) : region.Len;

        if ((regionBase + regionLength) > limit)
        {
            if (regionBase >= limit)
                continue;

            regionLength = limit - regionBase;
        }

        if ((end - start) < regionLength)
        {
            start = regionBase;
            end = regionBase + regionLength;
        }
    }

    if (end > start && start != 0)
        return true;

    return false;
}

MemoryMap::~MemoryMap()
{
}

ulong MemoryMap::GetKernelStart()
{
    return Stdlib::RoundDown((ulong)&KernelStart, Const::PageSize);
}

ulong MemoryMap::GetKernelEnd()
{
    return Stdlib::RoundUp((ulong)&KernelEnd, Const::PageSize);
}


size_t MemoryMap::GetRegionCount()
{
    return Size;
}

bool MemoryMap::GetRegion(size_t index, ulong& addr, ulong& len, ulong& type)
{
    if (index >= Size)
        return false;

    auto& region = Region[index];
    addr = region.Addr;
    len = region.Len;
    type = region.Type;
    return true;
}

}
}