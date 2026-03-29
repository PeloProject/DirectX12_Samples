#include "LaunchOptions.h"

#include <QString>

namespace
{
    bool IsGameArg(const QString& value)
    {
        return value == QStringLiteral("--game") ||
            value == QStringLiteral("-game") ||
            value == QStringLiteral("/game");
    }
}

LaunchOptions ParseLaunchOptions(int argc, char* argv[])
{
    LaunchOptions options;
    for (int index = 1; index < argc; ++index)
    {
        const QString arg = QString::fromLocal8Bit(argv[index]).trimmed();
        if (IsGameArg(arg))
        {
            options.gameMode = true;
            continue;
        }

        if (arg == QStringLiteral("--ui=imgui"))
        {
            options.uiBackend = EditorUiBackend::ImGui;
            continue;
        }

        if (arg == QStringLiteral("--ui=qt"))
        {
            options.uiBackend = EditorUiBackend::Qt;
            continue;
        }
    }

    return options;
}
