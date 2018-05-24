#pragma once

#include "stdlib.h"
#include "error.h"

#include <kernel/panic.h>
#include <mm/new.h>

namespace Stdlib
{

template<class T>
class Vector
{
public:
    Vector()
        : Arr(nullptr), Size(0), Capacity(0)
    {
    }

    size_t GetSize() const
    {
        return Size;
    }

    size_t GetCapacity() const
    {
        return Capacity;
    }

    T& operator[](size_t index)
    {
        BugOn(index < 0 || index >= Size);
        return Arr[index];
    }

    bool Reserve(size_t capacity)
    {
        if (capacity <= Capacity)
            return true;

        T* newArr = new T[capacity];
        if (!newArr)
            return false;

        if (Arr)
        {
            for (size_t i = 0; i < Size; i++)
            {
                newArr[i] = Stdlib::Move(Arr[i]);
            }
            delete[] Arr;
        }
        Arr = newArr;
        Capacity = capacity;
        return true;
    }

    bool ReserveAndUse(size_t capacity)
    {
        if (!Reserve(capacity))
            return false;
        Size = Capacity;
        return true;
    }

    bool Truncate(size_t size)
    {
        if (size > Size)
            return false;

        Size = size;
        return true;
    }

    bool PushBack(T&& e)
    {
        if (Size == Capacity)
        {
            if (!Reserve(2*Size + 1))
                return false;
        }
        Arr[Size++] = Stdlib::Move(e);
        return true;
    }

    bool PushBack(const T& e)
    {
        if (Size == Capacity)
        {
            if (!Reserve(2*Size + 1))
                return false;
        }
        Arr[Size++] = e;
        return true;
    }

    const T* GetConstBuf() const
    {
        return Arr;
    }

    T* GetBuf()
    {
        return Arr;
    }

    virtual ~Vector()
    {
        Release();
    }

    void Clear()
    {
        Release();
    }

    Vector(Vector&& other)
        : Vector()
    {
        Arr = other.Arr;
        Size = other.Size;
        Capacity = other.Capacity;
        other.Arr = nullptr;
        other.Size = 0;
        other.Capacity = 0;
    }

    Vector& operator=(Vector&& other)
    {
        Release();
        Arr = other.Arr;
        Size = other.Size;
        Capacity = other.Capacity;
        other.Arr = nullptr;
        other.Size = 0;
        other.Capacity = 0;
        return *this;
    }

    Vector(const Vector& other, Stdlib::Error& err)
        : Vector()
    {
        if (!err.Ok())
            return;

        Arr = new T[other.Capacity];
        if (!Arr)
        {
            err = MakeError(Stdlib::Error::NoMemory);
            return;
        }

        for (size_t i = 0; i < other.Size; i++)
        {
            Arr[i] = other.Arr[i];
        }
        Size = other.Size;
        Capacity = other.Capacity;
    }

    void Shuffle()
    {
        Panic("Not implemented yet!");
    }

    Vector(const T* arr, size_t size, Stdlib::Error& err)
    {
        if (!err.Ok())
            return;

        Arr = new T[size];
        if (!Arr)
        {
            err = MakeError(Stdlib::Error::NoMemory);
            return;
        }

        for (size_t i = 0; i < size; i++)
        {
            Arr[i] = arr[i];
        }

        Size = size;
        Capacity = Size;
    }

private:
    Vector(const Vector& other) = delete;
    Vector& operator=(const Vector& other) = delete;

    void Release()
    {
        if (Arr)
        {
            delete[] Arr;
            Arr = nullptr;
        }
        Size = 0;
        Capacity = 0;
    }

    T* Arr;
    size_t Size;
    size_t Capacity;
};

}
