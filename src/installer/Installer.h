#pragma once
#include <QStringList>

// kinoz's own installer / uninstaller. The app itself is the setup program: Setup.exe unpacks the files to a
// temporary folder and starts   kinoz.exe --installer   from there. Windows only (per user, no admin rights needed).
//
//   --installer [--silent] [--dest <dir>] [--no-launch]     install or update
//   --uninstall [--silent]                                  remove (run from the installed copy)
//   --installer-shots <dir>                                 render the wizard pages to PNGs (for docs)
namespace Installer {
bool wanted(const QStringList& args);            // true if the command line asks for installer mode
int run(const QStringList& args);                // runs the wizard / silent job, returns the exit code
void registerDefaultApp(const QString& exePath); // Windows: list kinoz in Settings > Default apps (no-op elsewhere)
}
