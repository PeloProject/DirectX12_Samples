#pragma once

#include <QString>

class RuntimeBridge;

QString GetEditorBaseDirectory();
int RunStandaloneGameMode(RuntimeBridge& runtime, const QString& baseDir);
