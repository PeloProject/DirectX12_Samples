#pragma once

#include <Windows.h>

class ScopedHandle
{
public:
    explicit ScopedHandle(HANDLE handle = nullptr) : handle_(handle) {}
    ~ScopedHandle() { Reset(); }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& other) noexcept : handle_(other.Release()) {}
    ScopedHandle& operator=(ScopedHandle&& other) noexcept
    {
        if (this != &other)
        {
            Reset(other.Release());
        }
        return *this;
    }

    HANDLE Get() const { return handle_; }

    HANDLE Release()
    {
        HANDLE released = handle_;
        handle_ = nullptr;
        return released;
    }

    void Reset(HANDLE handle = nullptr)
    {
        if (handle_ != nullptr)
        {
            CloseHandle(handle_);
        }
        handle_ = handle;
    }

private:
    HANDLE handle_;
};

class ScopedModule
{
public:
    explicit ScopedModule(HMODULE module = nullptr) : module_(module) {}
    ~ScopedModule() { Reset(); }

    ScopedModule(const ScopedModule&) = delete;
    ScopedModule& operator=(const ScopedModule&) = delete;

    ScopedModule(ScopedModule&& other) noexcept : module_(other.Release()) {}
    ScopedModule& operator=(ScopedModule&& other) noexcept
    {
        if (this != &other)
        {
            Reset(other.Release());
        }
        return *this;
    }

    HMODULE Get() const { return module_; }

    HMODULE Release()
    {
        HMODULE released = module_;
        module_ = nullptr;
        return released;
    }

    void Reset(HMODULE module = nullptr)
    {
        if (module_ != nullptr)
        {
            FreeLibrary(module_);
        }
        module_ = module;
    }

private:
    HMODULE module_;
};
