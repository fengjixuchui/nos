#include "task.h"
#include "trace.h"
#include "asm.h"
#include "sched.h"

namespace Kernel
{

namespace Core
{

Task::Task()
    : TaskQueue(nullptr)
    , Stack(nullptr)
    , Function(nullptr)
    , Ctx(nullptr)
{
    RefCounter.Set(1);
    ListEntry.Init();
}

Task::~Task()
{
    Put();
    BugOn(Stack != nullptr);
    BugOn(TaskQueue != nullptr);
}

void Task::Release()
{
    BugOn(TaskQueue != nullptr);

    if (Stack != nullptr)
    {
        delete Stack;
        Stack = nullptr;
    }
}

void Task::Get()
{
    BugOn(RefCounter.Get() <= 0);
    RefCounter.Inc();
}

void Task::Put()
{
    BugOn(RefCounter.Get() <= 0);
    if (RefCounter.DecAndTest())
    {
        Release();
    }
}

bool Task::Run(Func func, void* ctx)
{
    Stack = new struct Stack(this);
    if (Stack == nullptr)
    {
        return false;
    }

    SwitchRsp((ulong)&Stack->StackTop[0]);
    Function = func;
    Ctx = ctx;

    Function(Ctx);
    return true;
}

Task* Task::GetCurrentTask()
{
    struct Stack* stack = reinterpret_cast<struct Stack *>(GetRsp() & (~(StackSize - 1)));
    if (BugOn(stack->Magic1 != StackMagic1))
        return nullptr;

    if (BugOn(stack->Magic2 != StackMagic2))
        return nullptr;

    return stack->Task;
}

}
}