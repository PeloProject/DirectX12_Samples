#pragma once

#include <filesystem>

bool EnsurePieGameModuleLoaded();
void UnloadPieGameModule();
bool TryHotReloadPieGameModule();
bool ReloadPieGameModuleNow(const char* successStatus, const char* failureStatus);
bool ReloadPieGameModuleFromPath(const std::filesystem::path& modulePath, const char* successStatus, const char* failureStatus);
std::filesystem::path GetCurrentModuleDirectory();
