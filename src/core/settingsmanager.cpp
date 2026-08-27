#include "settingsmanager.h"
#include <QSettings>
#include <QCoreApplication>
#include <QDir>

settingsmanager::settingsmanager() {}

void settingsmanager::SetAutostart(bool enable)
{
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                       QSettings::NativeFormat);
    if (enable) {
        QString appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        settings.setValue("AirTask", QString("\"%1\"").arg(appPath));
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

