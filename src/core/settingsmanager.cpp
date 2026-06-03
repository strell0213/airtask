#include "settingsmanager.h"
#include "QSettings"
#include "QCoreApplication"

settingsmanager::settingsmanager() {}

void settingsmanager::SetAutostart(bool enable)
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                       QSettings::NativeFormat);
    if (enable) {
        settings.setValue("AirTask", QCoreApplication::applicationFilePath()
                              .replace("/", "\\"));
    } else {
        settings.remove("AirTask");
    }
}

bool settingsmanager::GetAutostart()
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                       QSettings::NativeFormat);
    return settings.contains("AirTask");
}
