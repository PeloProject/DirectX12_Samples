#pragma once

enum class EditorUiBackend
{
    Qt,
    ImGui,
};

struct LaunchOptions
{
    bool gameMode = false;
    EditorUiBackend uiBackend = EditorUiBackend::Qt;
};

LaunchOptions ParseLaunchOptions(int argc, char* argv[]);
