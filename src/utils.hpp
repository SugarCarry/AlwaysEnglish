#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QStringList>
#include "singleton.h"

class Utils : public QObject {
    Q_OBJECT

public:
    SINGLETON(Utils)

    explicit Utils(QObject *parent = nullptr);

    Q_INVOKABLE bool setAutoStart(bool enable) {
        const auto appName = autoStartName();
        removeRegistryAutoStart(appName);

        if (enable) {
            return createStartupTask(appName);
        }

        return removeStartupTask(appName);
    }

    Q_INVOKABLE bool isAutoStartLaunch() const {
        const auto arguments = QCoreApplication::arguments();
        return arguments.contains("--autostart")
               || arguments.contains("--startup")
               || arguments.contains("--silent");
    }

private:
    static QString autoStartName() {
        auto appName = QCoreApplication::applicationName();
        if (appName.isEmpty()) {
            appName = "AlwaysEnglish";
        }
        return appName;
    }

    static void removeRegistryAutoStart(const QString &appName) {
        QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
        settings.remove(appName);
    }

    static bool runSchtasks(const QStringList &arguments, QString *output = nullptr) {
        QProcess process;
        process.start("schtasks.exe", arguments);

        if (!process.waitForFinished(30000)) {
            process.kill();
            process.waitForFinished(3000);
            return false;
        }

        const auto text = QString::fromLocal8Bit(process.readAllStandardOutput())
                          + QString::fromLocal8Bit(process.readAllStandardError());
        if (output) {
            *output = text;
        }

        return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    }

    static bool createStartupTask(const QString &taskName) {
        const auto appPath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        const auto startupCommand = QString("\"%1\" --autostart").arg(appPath);

        return runSchtasks({
            "/Create",
            "/TN", taskName,
            "/SC", "ONLOGON",
            "/TR", startupCommand,
            "/RL", "HIGHEST",
            "/F"
        });
    }

    static bool removeStartupTask(const QString &taskName) {
        QString output;
        if (runSchtasks({"/Delete", "/TN", taskName, "/F"}, &output)) {
            return true;
        }

        return output.contains("does not exist", Qt::CaseInsensitive)
               || output.contains("cannot find", Qt::CaseInsensitive)
               || output.contains("找不到")
               || output.contains("不存在");
    }
};

inline Utils::Utils(QObject* parent) : QObject(parent) {}
