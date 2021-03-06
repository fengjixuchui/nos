#include "watchdog.h"
#include "asm.h"
#include "preempt.h"
#include "time.h"
#include "panic.h"
#include "trace.h"

namespace Kernel
{

Watchdog::Watchdog()
{
}

Watchdog::~Watchdog()
{
}

void Watchdog::Check()
{
    Stdlib::Time now = GetBootTime();
    Stdlib::Time timeout(25 * Const::NanoSecsInMs);

    for (size_t i = 0; i < Stdlib::ArraySize(SpinLockList); i++)
    {
        auto& listLock = SpinLockListLock[i];
        auto& list = SpinLockList[i];

        if (list.IsEmpty())
            continue;

        ulong flags = listLock.LockIrqSave();
        for (Stdlib::ListEntry* entry = list.Flink;
            entry != &list;
            entry = entry->Flink)
        {
            SpinLock* lock = CONTAINING_RECORD(entry, SpinLock, WatchdogListEntry);
            CheckCounter.Inc();
            Stdlib::Time lockTime(lock->WatchdogLockTime.Get());
            if (lockTime.GetValue() != 0)
            {
                Stdlib::Time delta = now - lockTime;
                if (delta > timeout)
                {
                    Trace(0, "Spinlock 0x%p is held too long %u", lock, delta.GetValue());
                }
            }
        }
        listLock.UnlockIrqRestore(flags);
    }
}

void Watchdog::RegisterSpinLock(SpinLock& lock)
{
    size_t i = Stdlib::HashPtr(&lock) % Stdlib::ArraySize(SpinLockList);
    auto& listLock = SpinLockListLock[i];
    auto& list = SpinLockList[i];

    ulong flags = listLock.LockIrqSave();
    list.InsertTail(&lock.WatchdogListEntry);
    SpinLockCounter.Inc();
    listLock.UnlockIrqRestore(flags);
}

void Watchdog::UnregisterSpinLock(SpinLock& lock)
{
    size_t i = Stdlib::HashPtr(&lock) % Stdlib::ArraySize(SpinLockList);
    auto& listLock = SpinLockListLock[i];

    ulong flags = listLock.LockIrqSave();
    if (!lock.WatchdogListEntry.IsEmpty())
    {
        lock.WatchdogListEntry.Remove();
        SpinLockCounter.Dec();
    }
    listLock.UnlockIrqRestore(flags);
}

void Watchdog::Dump(Stdlib::Printer& printer)
{
    printer.Printf("%u %u\n", SpinLockCounter.Get(), CheckCounter.Get());
}

}