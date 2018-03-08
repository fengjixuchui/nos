#include "idt.h"
#include "asm.h"
#include "trace.h"

#include <lib/stdlib.h>

namespace Kernel
{

Idt::Idt()
    : Base(0)
    , Limit(0)
{
    Trace(0, "Idt 0x%p", this);

    for (size_t i = 0; i < Stdlib::ArraySize(Entry); i++)
    {
        Entry[i] = IdtDescriptor::Encode(DummyInterruptStub);
    }
}

Idt::~Idt()
{
}

void Idt::Save()
{
    TableDesc desc = {
        .Base = reinterpret_cast<u64>(&Entry[0]),
        .Limit = sizeof(Entry),
    };

    LoadIdt(&desc);

    Base = desc.Base;
    Limit = desc.Limit;
}

u32 Idt::GetBase()
{
    return Base;
}

u16 Idt::GetLimit()
{
    return Limit;
}

IdtDescriptor Idt::GetDescriptor(u16 index)
{
    if (index > Stdlib::ArraySize(Entry))
	    return IdtDescriptor();

    return Entry[index];
}

void Idt::SetDescriptor(u16 index, const IdtDescriptor& desc)
{
    if (index > Stdlib::ArraySize(Entry))
	    return;

    Entry[index] = desc;
}

void Idt::DummyInterrupt()
{
    DummyInterruptCounter.Inc();
    Panic("Unknown interrupt");
}

extern "C" void DummyInterrupt()
{
    Idt::GetInstance().DummyInterrupt();
}

}
