#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QStringList>
#include <QWindow>
#include "singleton.h"
#include <windows.h>

// WDA_EXCLUDEFROMCAPTURE 需要 Windows 10 2004 (build 19041) 及以上
// 若编译时使用的 SDK 头文件较旧未定义该常量, 这里手动补充
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

class Utils : public QObject {
    Q_OBJECT

public:
    SINGLETON(Utils)

    explicit Utils(QObject *parent = nullptr);

    Q_INVOKABLE bool setAutoStart(bool enable) {
        const auto appName = autoStartName();
        removeRegistryAutoStart(appName);
        removeRegistryAutoStart(legacyAutoStartName());
        removeStartupTask(legacyAutoStartName());

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

    // 将指定窗口排除出屏幕捕获 (截图/录屏看不到该窗口, 但本机肉眼可见)
    // 适配 Snipaste、微信、QQ、Win+Shift+S 等所有基于系统捕获的截图工具
    Q_INVOKABLE bool setExcludeFromCapture(QWindow *window, bool exclude) {
        if (!window) {
            return false;
        }
        const HWND hwnd = reinterpret_cast<HWND>(window->winId());
        if (!hwnd) {
            return false;
        }
        return SetWindowDisplayAffinity(hwnd, exclude ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
    }

private:
    static QString autoStartName() {
        auto appName = QCoreApplication::applicationName();
        if (appName.isEmpty()) {
            appName = "AlwaysLang";
        }
        return appName;
    }

    static QString legacyAutoStartName() {
        return "AlwaysEnglish";
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
