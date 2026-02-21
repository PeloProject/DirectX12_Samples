#include "pch.h"
#include "AppRuntime.h"

AppRuntime& AppRuntime::Get()
{
    static AppRuntime instance;
    return instance;
}

RuntimeState& AppRuntime::MutableState()
{
    return state_;
}

const RuntimeState& AppRuntime::State() const
{
    return state_;
}

AppRuntime& Runtime()
{
    return AppRuntime::Get();
}

RuntimeState& RuntimeStateRef()
{
    return Runtime().MutableState();
}
