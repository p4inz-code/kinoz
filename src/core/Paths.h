#pragma once
#include <QString>

// Everything the app writes lives next to the exe, in ./data  (portable, never C:).
namespace Paths {
QString appDir();
QString dataDir();
QString cacheDir();
QString thumbsDir();
QString tmpDir();
QString exportsDir();
QString logsDir();
void ensure();
}
