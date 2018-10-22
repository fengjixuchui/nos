#include "sched.h"
#include "panic.h"
#include "trace.h"
#include "preempt.h"
#include "time.h"
#include "asm.h"
#include "debug.h"
#include "cpu.h"

namespace Kernel
{

TaskQueue::TaskQueue(Cpu *owner)
    : Owner(owner)
{
    Stdlib::AutoLock lock(Lock);
    TaskList.Init();

    SwitchContextCounter.Set(0);
    ScheduleCounter.Set(0);
}

void TaskQueue::SwitchComplete(Task* curr)
{
    Task* prev = curr->Prev;
    curr->Prev = nullptr;

    curr->Lock.Unlock();
    prev->Lock.Unlock();
    Lock.Unlock();

    if (prev->State.Get() != Task::StateExited)
    {
/*
        auto taskQueue = prev->SelectNextTaskQueue();
        if (taskQueue != nullptr)
        {
            prev->TaskQueue->Remove(prev);
            taskQueue->Insert(prev);
        }
*/
        prev->PreemptDisableCounter.Dec();
    } else {
        prev->PreemptDisableCounter.Dec();
        prev->Put();
    }
    prev->Put();
}

void TaskQueue::SwitchComplete(void* ctx)
{
    Task* curr = static_cast<Task*>(ctx);
    curr->TaskQueue->SwitchComplete(curr);
}

void TaskQueue::Switch(Task* next, Task* curr)
{
    SwitchContextCounter.Inc();

    BugOn(curr == next);
    BugOn(next->Prev != nullptr);
    BugOn(next->Rsp == 0);

    if (curr->State.Get() != Task::StateExited)
        curr->State.Set(Task::StateWaiting);

    curr->ContextSwitches.Inc();
    curr->UpdateRuntime();

    BugOn(next->State.Get() == Task::StateExited);
    next->State.Set(Task::StateRunning);
    next->RunStartTime = GetBootTime();
    curr->Get();
    next->Prev = curr;
    SwitchContext(next->Rsp, &curr->Rsp, &TaskQueue::SwitchComplete, next);
}

Task* TaskQueue::SelectNext(Task *curr)
{
    Task* next = nullptr;

    for (auto currEntry = TaskList.Flink;
        currEntry != &TaskList;
        currEntry = currEntry->Flink)
    {
        Task* cand = CONTAINING_RECORD(currEntry, Task, ListEntry);
        if (cand == curr)
        {
            BugOn(cand->State.Get() == Task::StateExited);
            cand->ListEntry.Remove();
            TaskList.InsertTail(&cand->ListEntry);
            break;
        }

        if (cand->PreemptDisableCounter.Get() != 0)
        {
            continue;
        }
        next = cand;
        break;
    }

    return next;
}

void TaskQueue::Schedule(Task* curr)
{
    ScheduleCounter.Inc();

    ulong flags = GetRflags();
    InterruptDisable();
    Lock.Lock();

    Task* next = nullptr;
    do {
        curr->Lock.Lock();
        BugOn(TaskList.IsEmpty());

        if (curr->State.Get() == Task::StateExited)
        {
            BugOn(curr->TaskQueue != this);
            BugOn(curr->ListEntry.IsEmpty());
            curr->TaskQueue = nullptr;
            curr->ListEntry.RemoveInit();
        }

        next = SelectNext(curr);
        if (next == nullptr)
        {
            break;
        }

        next->Lock.Lock();
        next->ListEntry.Remove();
        if (curr->TaskQueue != nullptr)
        {
            BugOn(curr->TaskQueue != this);
            curr->ListEntry.Remove();
            TaskList.InsertTail(&curr->ListEntry);
        }
        TaskList.InsertTail(&next->ListEntry);

    } while (false);

    if (next == nullptr)
    {
        curr->UpdateRuntime();
        curr->Lock.Unlock();
        Lock.Unlock();
        SetRflags(flags);
        curr->PreemptDisableCounter.Dec();
        BugOn(curr->State.Get() == Task::StateExited);
        return;
    }

    Switch(next, curr);
    SetRflags(flags);
}

void TaskQueue::Insert(Task* task)
{
    task->Get();

    Stdlib::AutoLock lock(Lock);
    Stdlib::AutoLock lock2(task->Lock);

    BugOn(task->TaskQueue != nullptr);
    BugOn(!(task->ListEntry.IsEmpty()));

    task->TaskQueue = this;
    TaskList.InsertTail(&task->ListEntry);
}

void TaskQueue::Remove(Task* task)
{
    {
        Stdlib::AutoLock lock(Lock);
        Stdlib::AutoLock lock2(task->Lock);

        BugOn(task->TaskQueue != this);
        BugOn(task->ListEntry.IsEmpty());
        task->TaskQueue = nullptr;
        task->ListEntry.RemoveInit();
    }

    task->Put();
}

void TaskQueue::Clear()
{
    Stdlib::ListEntry taskList;
    {
        Stdlib::AutoLock lock(Lock);
        taskList.MoveTailList(&TaskList);
    }

    if (taskList.IsEmpty())
        return;

    while (!taskList.IsEmpty())
    {
        Task* task = CONTAINING_RECORD(taskList.RemoveHead(), Task, ListEntry);
        Stdlib::AutoLock lock2(task->Lock);
        BugOn(task->TaskQueue != this);
        task->TaskQueue = nullptr;
        task->Put();
    }

    Trace(0, "TaskQueue 0x%p counters: sched %u switch context %u",
        this, ScheduleCounter.Get(), SwitchContextCounter.Get());
}

TaskQueue::~TaskQueue()
{
    Clear();
}

long TaskQueue::GetSwitchContextCounter()
{
    return SwitchContextCounter.Get();
}

Cpu* TaskQueue::GetCpu()
{
    return Owner;
}

void Schedule()
{
    if (unlikely(!PreemptIsOn()))
    {
        return;
    }

    Task *curr = Task::GetCurrentTask();
    curr->PreemptDisableCounter.Inc();
    if (curr->PreemptDisableCounter.Get() > 1)
    {
        Stdlib::AutoLock lock(curr->Lock);
        curr->UpdateRuntime();
        curr->PreemptDisableCounter.Dec();
        return;
    }

    curr->TaskQueue->Schedule(curr);
}

void Sleep(ulong nanoSecs)
{
    auto expired = GetBootTime() + nanoSecs;

    while (GetBootTime() < expired)
    {
        Schedule();
    }
}

}