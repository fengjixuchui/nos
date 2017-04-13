#pragma once

#include <include/types.h>

void* operator new(size_t size) noexcept;
void* operator new[](size_t size) noexcept;

void operator delete(void* ptr) noexcept;
void operator delete[](void* ptr) noexcept;
