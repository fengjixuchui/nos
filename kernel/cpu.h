#pragma once

#include "stdlib.h"
#include "spin_lock.h"
#include "task.h"
#include "sched.h"

namespace Kernel
{

namespace Core
{

class Cpu final
{
public:
    Cpu();
    ~Cpu();

    void Init(ulong index);

    void SetRunning();
    void SetExiting();

    ulong GetIndex();

    void Idle();

    ulong GetState();

    void IPI(Context* ctx);

    static const ulong StateInited = 0x1;
    static const ulong StateRunning = 0x2;
    static const ulong StateExiting = 0x4;
    static const ulong StateExited = 0x8;

    bool Run(Task::Func func, void *ctx);

    void SendIPISelf();

    void OnTimeChange(const Shared::Time& time);

private:
    Cpu(const Cpu& other) = delete;
    Cpu(Cpu&& other) = delete;
    Cpu& operator=(const Cpu& other) = delete;
    Cpu& operator=(Cpu&& other) = delete;

    ulong Index;
    ulong State;
    SpinLock Lock;
    TaskQueue TaskQueue;
    Task Task;
};

class CpuTable final
{
public:
    static CpuTable& GetInstance()
    {
        static CpuTable instance;
        return instance;
    }

    bool InsertCpu(ulong index);

    Cpu& GetCpu(ulong index);

    bool StartAll();

    ulong GetBspIndex();
    bool SetBspIndex(ulong index);

    ulong GetCurrentCpuId();

    Cpu& GetCurrentCpu();

    static const u8 IPIVector = 0xFE;

    void SendIPI(ulong index);

    ulong GetRunningCpus();

    void ExitAllExceptSelf();

private:
    CpuTable();
    ~CpuTable();
    CpuTable(const CpuTable& other) = delete;
    CpuTable(CpuTable&& other) = delete;
    CpuTable& operator=(const CpuTable& other) = delete;
    CpuTable& operator=(CpuTable&& other) = delete;

    ulong GetBspIndexLockHeld();

    static const ulong MaxCpu = 8;

    SpinLock Lock;
    Cpu CpuArray[MaxCpu];

    ulong BspIndex;
};

}

}