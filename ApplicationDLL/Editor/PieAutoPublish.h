#pragma once

#include <filesystem>
#include <vector>

bool ResolvePieManagedCsprojPath();
bool TryGetPieManagedSourceWriteTime(std::filesystem::file_time_type& outWriteTime);
bool TryStartPieManagedAutoPublish();
void TickPieManagedAutoPublish(float deltaTime);
std::vector<std::filesystem::path> BuildPieManagedReloadCandidates();
