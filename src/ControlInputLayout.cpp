#include "ControlInputLayout.h"
#include "GetActiveWindowPath.h"
#include <QJSEngine>
#include <QJSValue>
#include <QVariantMap>
#include <QtGlobal>
#include <imm.h>

// WM_IME_CONTROL 的子命令常量在部分 Windows SDK 的 imm.h 中未公开定义,
// 这些是 Windows 上通用且稳定的值, 手动补充以便跨进程查询 IME 转换模式
#ifndef IMC_GETCONVERSIONMODE
#define IMC_GETCONVERSIONMODE 0x0001
#endif
#ifndef IMC_GETOPENSTATUS
#define IMC_GETOPENSTATUS 0x0005
#endif

namespace {
constexpr LANGID ENGLISH_LANG_ID = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
constexpr LANGID CHINESE_LANG_ID = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
constexpr LANGID KOREAN_LANG_ID = MAKELANGID(LANG_KOREAN, SUBLANG_KOREAN);

QString languageNameFromLangId(LANGID langId) {
    switch (PRIMARYLANGID(langId)) {
        case LANG_ENGLISH:
            return "ENG";
        case LANG_CHINESE:
            return "中";
        case LANG_KOREAN:
            return "한";
        default:
            return "--";
    }
}

bool layoutMatchesLanguage(HKL layout, const QString &language) {
    const auto langId = LOWORD(reinterpret_cast<quintptr>(layout));
    if (language == "ENG") {
        return langId == ENGLISH_LANG_ID;
    }
    if (language == "中") {
        return PRIMARYLANGID(langId) == LANG_CHINESE;
    }
    if (language == "한") {
        return PRIMARYLANGID(langId) == LANG_KOREAN;
    }
    return false;
}
}

ControlInputLayout::ControlInputLayout(QObject *parent)
        : QObject(parent), m_isCapLock(true), m_isTurnOn(false), m_currentLanguage("--"), m_currentTargetLanguage("ENG") {
    m_timer = new QTimer(this);
    m_timer_always = new QTimer(this);

    connect(m_timer, &QTimer::timeout, this, &ControlInputLayout::onTimerTimeout);
    connect(m_timer_always, &QTimer::timeout, this, &ControlInputLayout::onTimerTimeout_always);
}

ControlInputLayout::~ControlInputLayout() {
    stopTask();
    alwaysStoptTask();
}

bool ControlInputLayout::isEnglishInputInstalled() {
    return isInputInstalled("ENG");
}

bool ControlInputLayout::isTargetInputInstalled(const QString &language) {
    return isInputInstalled(language);
}

QString ControlInputLayout::currentLanguage() const {
    return m_currentLanguage;
}

QVariantMap ControlInputLayout::caretRect() const {
    return m_caretRect;
}

QString ControlInputLayout::refreshCurrentLanguage() {
    updateCurrentLanguage();
    updateCaretRect();
    return m_currentLanguage;
}

void ControlInputLayout::gotoLanguageSettings() {
    ShellExecute(NULL, L"open", L"ms-settings:regionlanguage", NULL, NULL, SW_SHOWNORMAL);
}

void ControlInputLayout::startTask() {
    m_timer->start(100);
}

void ControlInputLayout::alwaysStartTask() {
    m_timer_always->start(100);
}

void ControlInputLayout::stopTask() {
    m_timer->stop();
}

void ControlInputLayout::alwaysStoptTask() {
    m_timer_always->stop();
}

bool ControlInputLayout::isCapLock() {
    loadCurrentWindowSettings();
    return m_isCapLock;
}

void ControlInputLayout::loadCurrentWindowSettings() {
    m_isTurnOn = false;
    m_isCapLock = false;
    m_currentTargetLanguage = "ENG";

    QString exeInfos_str = m_settings->getExeInfos();

    QJSEngine engine;

    QJSValue jsObject = engine.evaluate("(" + exeInfos_str + ")");

    if (!jsObject.isObject()) {
        return;
    }

    QVariantMap variantMap = jsObject.toVariant().toMap();

    for (const auto &key: variantMap.keys()) {
        if (key.contains(gw->exeName)) {
            const auto exeInfo = variantMap[key].toMap();
            m_isTurnOn = exeInfo["isTurnOn"].toBool();
            m_isCapLock = exeInfo["isCapLock"].toBool();
            m_currentTargetLanguage = normalizeLanguage(exeInfo.value("targetLanguage", "ENG").toString());
            break;
        }
    }
}

void ControlInputLayout::onTimerTimeout() {
    updateCurrentLanguage();

    if (!gw->isTargetWindow()) {
        return;
    }

    loadCurrentWindowSettings();

    if (!m_isTurnOn) {
        return;
    }

    switchToLanguage(m_currentTargetLanguage);
    updateCurrentLanguage();

    if (m_currentTargetLanguage == "ENG" && m_isCapLock) {
        capLock();
    }
}

void ControlInputLayout::onTimerTimeout_always() {
    switchToLanguage("ENG");
    updateCurrentLanguage();

    if (m_settings->getCapLock()) {
        capLock();
    }
}

void ControlInputLayout::switchToEnglish() {
    switchToLanguage("ENG");
}

void ControlInputLayout::switchToLanguage(const QString &language) {
    const QString normalizedLanguage = normalizeLanguage(language);
    const HKL targetLayout = findInstalledLayout(normalizedLanguage);
    if (!targetLayout) {
        return;
    }

    const HWND foregroundWindow = GetForegroundWindow();
    if (!foregroundWindow) {
        return;
    }

    const DWORD threadId = GetWindowThreadProcessId(foregroundWindow, NULL);
    HKL hkl = GetKeyboardLayout(threadId);

    if (!layoutMatchesLanguage(hkl, normalizedLanguage)) {
        PostMessage(foregroundWindow, WM_INPUTLANGCHANGEREQUEST, 0,
                    reinterpret_cast<LPARAM>(targetLayout));
    }
}

void ControlInputLayout::capLock() {
    // 检查大小写键是否按下, 如果没有则模拟按下并释放大小写键
    if (!(GetKeyState(VK_CAPITAL) & 0x0001)) {
        keybd_event(VK_CAPITAL, 0x3a, KEYEVENTF_EXTENDEDKEY | 0, 0);
        keybd_event(VK_CAPITAL, 0x3a, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP, 0);
    }
}

QString ControlInputLayout::getCurrentInputLanguage() const {
    const HWND foregroundWindow = GetForegroundWindow();
    if (!foregroundWindow) {
        return "--";
    }

    const DWORD threadId = GetWindowThreadProcessId(foregroundWindow, NULL);
    HKL hkl = GetKeyboardLayout(threadId);
    const auto langId = LOWORD(reinterpret_cast<quintptr>(hkl));
    const QString layoutLanguage = languageNameFromLangId(langId);

    // 纯英文键盘布局: 直接是英文, 无需再查子模式
    if (layoutLanguage == "ENG" || layoutLanguage == "--") {
        return layoutLanguage;
    }

    // 中文/韩文等 IME: 键盘布局语言无法反映"中/英"子模式
    // 需查询 IME 的转换模式 (IME_CMODE_NATIVE): 置位为本地语言(中/한), 清除为英文
    // 跨进程查询用 WM_IME_CONTROL + IMC_GETCONVERSIONMODE 发送到目标窗口的默认 IME 窗口
    bool nativeMode = true;
    const HWND hwndIme = ImmGetDefaultIMEWnd(foregroundWindow);
    if (hwndIme) {
        DWORD_PTR conversionMode = 0;
        const LRESULT ok = SendMessageTimeoutW(hwndIme, WM_IME_CONTROL,
                                               IMC_GETCONVERSIONMODE, 0,
                                               SMTO_ABORTIFHUNG | SMTO_BLOCK, 30,
                                               &conversionMode);
        if (ok) {
            nativeMode = (conversionMode & IME_CMODE_NATIVE) != 0;
        }
    }

    // Caps Lock 打开时, 中文 IME 实际输入的是英文字母
    const bool capsOn = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
    const bool englishMode = !nativeMode || capsOn;

    if (layoutLanguage == "中") {
        return englishMode ? "英" : "中";
    }
    if (layoutLanguage == "한") {
        return englishMode ? "ENG" : "한";
    }
    return layoutLanguage;
}

QString ControlInputLayout::normalizeLanguage(const QString &language) const {
    if (language == "中" || language.compare("ZH", Qt::CaseInsensitive) == 0 ||
        language.compare("CN", Qt::CaseInsensitive) == 0) {
        return "中";
    }
    if (language == "한") {
        return "한";
    }
    return "ENG";
}

bool ControlInputLayout::isInputInstalled(const QString &language) const {
    return findInstalledLayout(language) != nullptr;
}

HKL ControlInputLayout::findInstalledLayout(const QString &language) const {
    HKL layouts[64] = {0};
    int layoutCount = GetKeyboardLayoutList(64, layouts);
    const QString normalizedLanguage = normalizeLanguage(language);

    for (int i = 0; i < layoutCount; i++) {
        if (layoutMatchesLanguage(layouts[i], normalizedLanguage)) {
            return layouts[i];
        }
    }

    return nullptr;
}

void ControlInputLayout::updateCurrentLanguage() {
    const QString language = getCurrentInputLanguage();
    if (language == m_currentLanguage) {
        return;
    }

    m_currentLanguage = language;
    emit currentLanguageChanged(m_currentLanguage);
}

QVariantMap ControlInputLayout::getCaretRect() const {
    QVariantMap rect;
    rect["x"] = 0;
    rect["y"] = 0;
    rect["width"] = 0;
    rect["height"] = 0;
    rect["valid"] = false;

    const HWND foregroundWindow = GetForegroundWindow();
    if (!foregroundWindow) {
        return rect;
    }

    auto setRectFromPoint = [&rect](const POINT &point, LONG width, LONG height) {
        rect["x"] = static_cast<qlonglong>(point.x);
        rect["y"] = static_cast<qlonglong>(point.y);
        rect["width"] = static_cast<qlonglong>(width);
        rect["height"] = static_cast<qlonglong>(height);
        rect["valid"] = true;
    };

    const DWORD threadId = GetWindowThreadProcessId(foregroundWindow, NULL);
    auto updateFromGuiThread = [&](DWORD idThread) {
        GUITHREADINFO guiThreadInfo = {0};
        guiThreadInfo.cbSize = sizeof(GUITHREADINFO);
        if (!GetGUIThreadInfo(idThread, &guiThreadInfo) || !guiThreadInfo.hwndCaret) {
            return false;
        }

        POINT point = {guiThreadInfo.rcCaret.left, guiThreadInfo.rcCaret.bottom};
        ClientToScreen(guiThreadInfo.hwndCaret, &point);
        setRectFromPoint(point,
                         guiThreadInfo.rcCaret.right - guiThreadInfo.rcCaret.left,
                         guiThreadInfo.rcCaret.bottom - guiThreadInfo.rcCaret.top);
        return true;
    };

    if (updateFromGuiThread(threadId) || updateFromGuiThread(0)) {
        return rect;
    }

    HIMC inputContext = ImmGetContext(foregroundWindow);
    if (inputContext) {
        COMPOSITIONFORM compositionForm;
        if (ImmGetCompositionWindow(inputContext, &compositionForm)) {
            POINT point = compositionForm.ptCurrentPos;
            ClientToScreen(foregroundWindow, &point);
            ImmReleaseContext(foregroundWindow, inputContext);

            setRectFromPoint(point, 1, 24);
            return rect;
        }
        ImmReleaseContext(foregroundWindow, inputContext);
    }

    POINT cursorPoint;
    if (GetCursorPos(&cursorPoint)) {
        setRectFromPoint(cursorPoint, 1, 24);
        return rect;
    }

    RECT windowRect;
    if (GetWindowRect(foregroundWindow, &windowRect)) {
        POINT point = {windowRect.left + 16, windowRect.top + 48};
        setRectFromPoint(point, 1, 24);
    }
    return rect;
}

void ControlInputLayout::updateCaretRect() {
    const QVariantMap rect = getCaretRect();
    if (rect == m_caretRect) {
        return;
    }

    m_caretRect = rect;
    emit caretRectChanged(m_caretRect);
}
