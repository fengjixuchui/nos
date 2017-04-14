#include "trace.h"
#include "panic.h"
#include "debug.h"
#include "atomic.h"
#include "idt.h"
#include "test.h"
#include "exception.h"
#include "asm.h"
#include "cpu.h"
#include "cmd.h"
#include "interrupt.h"
#include "icxxabi.h"
#include "preempt.h"

#include <boot/grub.h>

#include <lib/error.h>
#include <lib/stdlib.h>

#include <mm/new.h>
#include <mm/page_allocator.h>
#include <mm/memory_map.h>

#include <drivers/8042.h>
#include <drivers/vga.h>
#include <drivers/serial.h>
#include <drivers/pic.h>
#include <drivers/pit.h>
#include <drivers/acpi.h>
#include <drivers/lapic.h>
#include <drivers/ioapic.h>

using namespace Kernel;
using namespace Shared;

void TraceCpuState(ulong cpu)
{
    Trace(0, "Cpu %u cr0 0x%p cr2 0x%p cr3 0x%p cr4 0x%p",
        cpu, GetCr0(), GetCr2(), GetCr3(), GetCr4());

    Trace(0, "Cpu %u rflags 0x%p rsp 0x%p rip 0x%p",
        cpu, GetRflags(), GetRsp(), GetRip());

    Trace(0, "Cpu %u ss 0x%p cs 0x%p ds 0x%p gs 0x%p fs 0x%p es 0x%p",
        cpu, (ulong)GetSs(), (ulong)GetCs(), (ulong)GetDs(),
        (ulong)GetGs(), (ulong)GetFs(), (ulong)GetEs());
}

void ApStartup(void *ctx)
{
    (void)ctx;

    auto& cpu = CpuTable::GetInstance().GetCurrentCpu();

    cpu.SetRunning();

    Trace(0, "Cpu %u running rflags 0x%p task 0x%p",
        cpu.GetIndex(), GetRflags(), Task::GetCurrentTask());

    TraceCpuState(cpu.GetIndex());

    Idt::GetInstance().Save();

    InterruptEnable();

    for (;;)
    {
        cpu.Idle();
    }
}

extern "C" void ApMain()
{
    Lapic::Enable();

    auto& cpu = CpuTable::GetInstance().GetCurrentCpu();

    if (!cpu.Run(ApStartup, nullptr))
    {
        Trace(0, "Can't run cpu %u task", cpu.GetIndex());
        return;
    }
}

void Exit()
{
    PreemptDisable();

    VgaTerm::GetInstance().Printf("Going to exit!\n");
    Trace(0, "Exit begin");

    CpuTable::GetInstance().ExitAllExceptSelf();

    VgaTerm::GetInstance().Printf("Bye!\n");
    Trace(0, "Exit end");

    PreemptOff();

    __cxa_finalize(0);
    InterruptDisable();
    Hlt();
}

void TestTaskFunc(void *ctx)
{
    (void)ctx;

    for (size_t i = 0; i < 3; i++)
    {
        Trace(0, "Hello from task 0x%p", Task::GetCurrentTask());
        GetCpu().Sleep(100000000);
    }
}

bool TestTask()
{
    Task *task[3] = {0};
    for (size_t i = 0; i < ArraySize(task); i++)
    {
        task[i] = new Task();
        if (task[i] == nullptr)
        {
            for (size_t j = 0; j < i; j++)
            {
                delete task[j];
            }
            return false;
        }
    }

    bool result;

    for (size_t i = 0; i < ArraySize(task); i++)
    {
        if (!task[i]->Start(TestTaskFunc, nullptr))
        {
            for (size_t j = 0; j < i; j++)
            {
                task[j]->Wait();
            }
            result = false;
            goto delTasks;
        }
    }

    for (size_t i = 0; i < ArraySize(task); i++)
    {
        task[i]->Wait();
    }

    result = true;

delTasks:
    for (size_t i = 0; i < ArraySize(task); i++)
    {
        delete task[i];
    }

    return result;
}

void BpStartup(void* ctx)
{
    (void)ctx;

    auto& idt = Idt::GetInstance();
    auto& excTable = ExceptionTable::GetInstance();
    auto& pit = Pit::GetInstance();
    auto& kbd = IO8042::GetInstance();
    auto& serial = Serial::GetInstance();
    auto& cmd = Cmd::GetInstance();
    auto& ioApic = IoApic::GetInstance();
    auto& cpus = CpuTable::GetInstance();
    auto& cpu = cpus.GetCurrentCpu();

    Trace(0, "Cpu %u running rflags 0x%p task 0x%p",
        cpu.GetIndex(), GetRflags(), Task::GetCurrentTask());

    TraceCpuState(cpu.GetIndex());

    ioApic.Enable();

    excTable.RegisterExceptionHandlers();

    //TODO: irq -> gsi remap by ACPI MADT
    Interrupt::Register(pit, 0x2, 0x20);
    Interrupt::Register(kbd, 0x1, 0x21);
    Interrupt::Register(serial, 0x4, 0x24);

    idt.SetDescriptor(CpuTable::IPIVector, IdtDescriptor::Encode(IPInterruptStub));

    idt.Save();
    pit.Setup();
    InterruptEnable();

    if (!cpus.StartAll())
    {
        Panic("Can't start all cpus");
        return;
    }

    PreemptOn();

    VgaTerm::GetInstance().Printf("IPI test...\n");

    ulong cpuMask = cpus.GetRunningCpus();
    for (ulong i = 0; i < 8 * sizeof(ulong); i++)
    {
        if (cpuMask & ((ulong)1 << i))
        {
            cpus.SendIPI(i);
        }
    }

    VgaTerm::GetInstance().Printf("Task test...\n");

    if (!TestTask())
    {
        Panic("Task test failed");
        return;
    }

    VgaTerm::GetInstance().Printf("Idle looping...\n");

    cmd.Start();
    for (;;)
    {
        cpu.Idle();
        cmd.Run();
        if (cmd.IsExit())
        {
            Trace(0, "Exit requested");
            break;
        }
    }

    Exit();
}

extern "C" void Main(Grub::MultiBootInfoHeader *MbInfo)
{
    do {

    Tracer::GetInstance().SetLevel(1);

    auto& mmap = MemoryMap::GetInstance();
    Trace(0, "Enter rflags 0x%p KernelSpace 0x%p KernelEnd 0x%p",
        GetRflags(), mmap.GetKernelSpaceBase(), mmap.GetKernelEnd());

    VgaTerm::GetInstance().Printf("Hello!\n");

    Grub::ParseMultiBootInfo(MbInfo);

    ulong memStart, memEnd;
    if (mmap.GetKernelEnd() <= (mmap.GetKernelSpaceBase() + MB))
    {
        Panic("Kernel end is lower than kernel space base");
        break;
    }

    //boot64.asm only setup paging for first 4GB
    if (!mmap.FindRegion(mmap.GetKernelEnd() - mmap.GetKernelSpaceBase(), 4 * GB, memStart, memEnd))
    {
        Panic("Can't get available memory region");
        break;
    }

    Trace(0, "Memory region 0x%p 0x%p", memStart, memEnd);
    if (!PageAllocatorImpl::GetInstance().Setup(mmap.GetKernelSpaceBase() + memStart,
                                                mmap.GetKernelSpaceBase() + memEnd))
    {
        Panic("Can't setup page allocator");
        break;
    }

    VgaTerm::GetInstance().Printf("Self test begin, please wait...\n");

    auto& acpi = Acpi::GetInstance();
    auto err = acpi.Parse();
    if (!err.Ok())
    {
        TraceError(err, "Can't parse ACPI");
        Panic("Can't parse ACPI");
        break;
    }

    Trace(0, "Before test");

    err = Test();
    if (!err.Ok())
    {
        TraceError(err, "Test failed");
        Panic("Self test failed");
        break;
    }

    Trace(0, "After test");
    VgaTerm::GetInstance().Printf("Self test complete, error %u\n", (ulong)err.GetCode());

    auto& kbd = IO8042::GetInstance();
    auto& cmd = Cmd::GetInstance();
    auto& pic = Pic::GetInstance();
    auto& cpus = CpuTable::GetInstance();
    if (!kbd.RegisterObserver(cmd))
    {
        Panic("Can't register cmd in kbd");
        break;
    }

    pic.Remap();
    pic.Disable();

    Lapic::Enable();

    auto& cpu = cpus.GetCurrentCpu();
    if (!cpus.SetBspIndex(cpu.GetIndex()))
    {
        Panic("Can't set boot processor index");
        break;
    }

    Trace(0, "Before cpu run");

    if (!cpu.Run(BpStartup, nullptr))
    {
        Panic("Can't run cpu %u task", cpu.GetIndex());
        break;
    }

    } while (false);

    Exit();
}
