#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QObject>
#include <QSettings>
#include <QStringList>
#include "singleton.h"

class Utils : public QObject {
    Q_OBJECT

public:
    SINGLETON(Utils)

    explicit Utils(QObject *parent = nullptr);

    Q_INVOKABLE void setAutoStart(bool enable) {
        auto appName = QCoreApplication::applicationName();
        if (appName.isEmpty()) {
            appName = "AlwaysEnglish";
        }
        auto appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        auto startupCommand = QString("\"%1\" --autostart").arg(appPath);

        QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);

        if (enable) {
            settings.setValue(appName, startupCommand);
        }
        else {
            settings.remove(appName);
        }
    }

    Q_INVOKABLE bool isAutoStartLaunch() const {
        const auto arguments = QCoreApplication::arguments();
        return arguments.contains("--autostart")
               || arguments.contains("--startup")
               || arguments.contains("--silent");
    }
};

inline Utils::Utils(QObject* parent) : QObject(parent) {}
