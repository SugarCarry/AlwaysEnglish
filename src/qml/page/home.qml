import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt.labs.platform
import FluentUI 1.0
import "../global"

FluContentPage {

    property bool isChecked: false

    // 控制表格是否能拖拽
    property bool dragEnabled: true

    id: root
    title: qsTr("App-specific language settings")

    launchMode: FluPageType.SingleInstance

    // 显示图标
    Component {
        id: com_ico
        Item {
            FluClip {
                anchors.centerIn: parent
                width: 30
                height: 30
                Image {
                    anchors.fill: parent
                    source: options && options.icon ? options.icon : ""
                    sourceSize: Qt.size(80, 80)
                }
            }
        }
    }

    // 是否启用
    Component {
        id: com_column_turn_on
        Item {
            RowLayout {
                anchors.centerIn: parent

                FluToggleSwitch {
                    checked: {
                        var rowObj = table_view.getRow(row)
                        var exePath = rowObj.path
                        return GlobalModel.exeInfos[exePath]['isTurnOn']
                    }

                    onClicked: {
                        var rowObj = table_view.getRow(row)
                        var exePath = rowObj.path
                        GlobalModel.exeInfos[exePath]['isTurnOn'] = checked

                        rowObj.turnon = table_view.customItem(com_column_turn_on)
                        table_view.setRow(row, rowObj)

                        saveAppSettings()
                    }
                }
            }
        }
    }

    // 目标语言
    Component {
        id: com_column_language
        Item {
            RowLayout {
                anchors.centerIn: parent

                FluComboBox {
                    id: language_combo
                    width: 90
                    editable: false
                    model: ["ENG", "中", "한"]

                    Component.onCompleted: {
                        var rowObj = table_view.getRow(row)
                        var exePath = rowObj.path
                        currentIndex = languageIndex(GlobalModel.exeInfos[exePath]['targetLanguage'])
                    }

                    onActivated: {
                        var rowObj = table_view.getRow(row)
                        var exePath = rowObj.path
                        var targetLanguage = model[index]

                        if (!ControlInputLayout.isTargetInputInstalled(targetLanguage)) {
                            showWarning(qsTr("The selected input language is not installed. Please install it in Windows language settings first."), 5000)
                            currentIndex = languageIndex(GlobalModel.exeInfos[exePath]['targetLanguage'])
                            return
                        }

                        GlobalModel.exeInfos[exePath]['targetLanguage'] = targetLanguage
                        rowObj.language = table_view.customItem(com_column_language)
                        table_view.setRow(row, rowObj)

                        saveAppSettings()
                    }
                }
            }
        }
    }

    // 是否开启大小写
    Component {
        id: com_column_caps
        Item {
            RowLayout {
                anchors.centerIn: parent

                FluToggleSwitch {
                    checked: {
                        var rowObj = table_view.getRow(row)
                        var exePath = rowObj.path
                        return GlobalModel.exeInfos[exePath]['isCapLock']
                    }

                    onClicked: {
                        var rowObj = table_view.getRow(row)
                        var exePath = rowObj.path
                        GlobalModel.exeInfos[exePath]['isCapLock'] = checked

                        rowObj.Caps = table_view.customItem(com_column_caps)
                        table_view.setRow(row, rowObj)

                        saveAppSettings()
                    }
                }
            }
        }
    }

    // 移除
    Component {
        id: com_action
        Item {
            RowLayout {
                anchors.centerIn: parent
                FluButton {
                    text: qsTr("Remove")
                    onClicked: {
                        // 从 Set 中移除对应行的 APP
                        var exePath = table_view.getRow(row).path
                        GlobalModel.existingFilePath.delete(exePath)

                        // 从 GlobalModel.exeInfos Object 中移除对应的 APP
                        delete GlobalModel.exeInfos[exePath]

                        table_view.closeEditor()
                        table_view.removeRow(row)

                        saveAppSettings()
                    }
                }
            }
        }
    }

    FluIconButton {
        id: myBtn

        iconDelegate: Image {
            source: isChecked ? "qrc:/res/images/start.png" : "qrc:/res/images/stop.png"
        }

        text: !isChecked ? qsTr("Start") : qsTr("Stop")

        onClicked: {
            isChecked = !isChecked
            dragEnabled = !dragEnabled

            if (isChecked) {
                myBtn.text = qsTr("Stop")
                btn_AlwaysEnglish.enabled = !btn_AlwaysEnglish.enabled
                btn_addApp.enabled = !btn_addApp.enabled

                // 启动
                ControlInputLayout.startTask()
                GlobalModel.isTaskRunning = true

                showSuccess(qsTr("Start Successfully"))
            } else {
                myBtn.text = qsTr("Start")
                btn_AlwaysEnglish.enabled = !btn_AlwaysEnglish.enabled
                btn_addApp.enabled = !btn_addApp.enabled

                // 停止
                ControlInputLayout.stopTask()
                GlobalModel.isTaskRunning = false
            }
        }
    }

    FluFrame {
        id: layout_controls
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            leftMargin: 80
            topMargin: 20
        }
        height: 50

        Row {
            spacing: 10
            anchors {
                left: parent.left
                leftMargin: 10
                verticalCenter: parent.verticalCenter
            }

            // 是否一直启动
            FluToggleButton {
                id: btn_AlwaysEnglish
                text: qsTr("Always ENG")

                normalColor: {
                    if (checked) {
                        btn_AlwaysEnglish.text = qsTr("Stop")
                        return Qt.rgba(255, 0, 0, 1)
                    } else {
                        btn_AlwaysEnglish.text = qsTr("Always ENG")
                        return FluTheme.dark ? Qt.rgba(62 / 255, 62 / 255, 62 / 255, 1) : Qt.rgba(254 / 255, 254 / 255, 254 / 255, 1)
                    }
                }

                FluTooltip {
                    visible: btn_AlwaysEnglish.hovered
                    text: qsTr("Force all apps to use the set input method, ignoring the per-app list")
                    delay: 400
                }

                onClicked: {
                    table_view.enabled = !checked
                    btn_addApp.enabled = !btn_addApp.enabled
                    myBtn.enabled = !myBtn.enabled

                    if (checked) {
                        ControlInputLayout.alwaysStartTask()
                    } else {
                        ControlInputLayout.alwaysStoptTask()
                    }
                }
            }

            // 添加
            FluButton {
                id: btn_addApp
                text: qsTr("Add an APP")
                onClicked: fileDialog.open()
            }

        }
    }

    // APP 选择对话框
    FileDialog {
        id: fileDialog
        title: qsTr("select an APP")
        fileMode: FileDialog.OpenFile
        nameFilters: ["exe Shortcuts(*.exe *.link *.lnk)"]
        onAccepted: {
            // 文件的路径
            var filePath = fileDialog.file.toString();
            addDataToRow(filePath);
        }
    }

    FluTableView {
        id: table_view
        anchors {
            left: parent.left
            right: parent.right
            top: layout_controls.bottom
            bottom: parent.bottom
        }
        anchors.topMargin: 5

        columnSource: [
            {
                title: qsTr("icon"),
                dataIndex: 'icon',
                width: 80,
                readOnly: true
            },
            {
                title: "App",
                dataIndex: 'name',
                width: 230,
                readOnly: true
            },
            {
                title: qsTr("Turn on"),
                dataIndex: 'turnon',
                width: 110,
            },
            {
                title: qsTr("Language"),
                dataIndex: 'language',
                width: 130,
            },
            {
                title: qsTr("Cap Lock"),
                dataIndex: 'Caps',
                width: 110,
            },
            {
                title: qsTr("Options"),
                dataIndex: 'action',
                width: 110,
                frozen: true
            }
        ]

        // 窗口/表格尺寸变化时, 让 "App" 列吸收多余宽度, 填满整个表格宽度
        onWidthChanged: resizeColumns()
        Component.onCompleted: resizeColumns()

        DropArea {
            anchors.fill: parent
            onDropped: function (drop) {
                if (!dragEnabled) {
                    return;
                }

                if (!drop.hasUrls) {
                    return;
                }

                for (var i = 0; i < drop.urls.length; i++) {
                    // 文件的路径
                    var filePath = drop.urls[i].toString();
                    addDataToRow(filePath);
                }
            }
        }
    }

    FluTour {
        id: tour
        steps: [
            {
                title: qsTr("1. Add Softwares"),
                description: qsTr("Drag the softwares or shortcuts in here"),
                target: () => table_view
            },
            {
                title: qsTr("1. Add Softwares"),
                description: qsTr("or click this button to add a software or shortcut"),
                target: () => btn_addApp
            },
            {title: qsTr("2. Start"), description: qsTr("click this button to start"), target: () => myBtn},
            {
                title: qsTr("2. Start"),
                description: qsTr("or you can click this button to switch to English in all APP all the time"),
                target: () => btn_AlwaysEnglish
            }
        ]
    }

    Component.onCompleted: {
        var openCount = SettingsHelper.getOpenCount()

        if (!GlobalModel.isAutoStartLaunch && openCount < 3) {
            tour.open()
        }

        if (!GlobalModel.exeInfos) {
            GlobalModel.exeInfos = ({})
        }

        for (let key in GlobalModel.exeInfos) {
            if (!GlobalModel.exeInfos[key].targetLanguage) {
                GlobalModel.exeInfos[key].targetLanguage = "ENG"
            }

            table_view.appendRow({
                icon: table_view.customItem(com_ico, {icon: GlobalModel.exeInfos[key].icon}),
                path: key,
                name: GlobalModel.exeInfos[key].name,
                turnon: table_view.customItem(com_column_turn_on),
                language: table_view.customItem(com_column_language),
                Caps: table_view.customItem(com_column_caps),
                action: table_view.customItem(com_action),
                _key: FluTools.uuid()
            })
        }

        saveAppSettings()

        if (GlobalModel.isTaskRunning
                || (!GlobalModel.hasAutoStartedTask && (GlobalModel.isAutoStart || GlobalModel.isAutoStartLaunch))) {
            isChecked = true
            dragEnabled = false
            myBtn.text = qsTr("Stop")
            btn_AlwaysEnglish.enabled = false
            btn_addApp.enabled = false
            if (!GlobalModel.isTaskRunning) {
                ControlInputLayout.startTask()
                GlobalModel.isTaskRunning = true
                GlobalModel.hasAutoStartedTask = true
            }
        }
    }

    function addDataToRow(filePath) {
        if (!dragEnabled) {
            return;
        }

        // 分割路径提取带扩展名的文件名
        var fileName = filePath.split("/").pop();

        // 获取扩展名
        const extension = fileName.split(".").pop();

        // 如果不是 .exe 或者 快捷方式则跳过
        if (extension !== "exe" && extension !== "link" && extension !== "lnk") {
            return;
        }

        // 没有扩展名的文件名
        const fileNameWithoutExtension = fileName.split(".")[0];

        // 提取不带 file:/// 的路径
        filePath = filePath.replace(/^file:\/{3}/, "");
        var exePath, fileIconBase64;

        if (extension === "link" || extension === "lnk") {
            // 根据快捷方式的路径拿到快捷方式所指向的 exe 路径
            exePath = LnkResolver.resolveLnk(filePath);
        } else {
            exePath = filePath;
        }

        // 判断表格是否已有该软件, 若已有则不重复添加
        if (GlobalModel.existingFilePath.has(exePath)) {
            return;
        }

        // 添加到 Set 中
        GlobalModel.existingFilePath.add(exePath);

        // 根据 exe 路径来找到它的图标, 拿到的是 Base64 格式
        fileIconBase64 = iconProvider.getExeIcon(exePath);

        // 用软件路径做 key 来保存其图标 是否启用 是否打开大小写键 等属性
        GlobalModel.exeInfos[exePath] = {
            name: fileNameWithoutExtension,
            icon: fileIconBase64,
            isTurnOn: true,
            targetLanguage: "ENG",
            isCapLock: true,
        }

        table_view.appendRow({
            icon: table_view.customItem(com_ico, {icon: fileIconBase64}),
            path: exePath,
            name: fileNameWithoutExtension,
            turnon: table_view.customItem(com_column_turn_on),
            language: table_view.customItem(com_column_language),
            Caps: table_view.customItem(com_column_caps),
            action: table_view.customItem(com_action),
            _key: FluTools.uuid()
        })

        saveAppSettings()
    }

    function saveAppSettings() {
        var jsonStr = JSON.stringify(GlobalModel.exeInfos)
        SettingsHelper.saveExeInfos(jsonStr)

        var array = Array.from(GlobalModel.existingFilePath)
        jsonStr = JSON.stringify(array)
        SettingsHelper.saveExistingFilePath(jsonStr)
    }

    function languageIndex(language) {
        if (language === "中") {
            return 1
        }
        if (language === "한") {
            return 2
        }
        return 0
    }

    // 让 "App" 列吸收表格的多余宽度, 使所有列填满整个表格宽度
    // (窗口最大化/缩放时表格不再留出右侧大片空白)
    function resizeColumns() {
        var cols = table_view.columnSource
        if (!cols || cols.length === 0) {
            return
        }

        // App 列(name)以外的列宽总和
        var fixedTotal = 0
        var nameIndex = -1
        for (var i = 0; i < cols.length; i++) {
            if (cols[i].dataIndex === 'name') {
                nameIndex = i
            } else {
                fixedTotal += (cols[i].width || 100)
            }
        }
        if (nameIndex === -1) {
            return
        }

        // 预留竖直滚动条/边框宽度, 避免出现横向滚动条
        var available = table_view.width - fixedTotal - 2
        var nameWidth = Math.max(230, available)

        if (cols[nameIndex].width === nameWidth) {
            return
        }

        // 构造全新数组(含新的列对象), 保证 onColumnSourceChanged 触发, 同步重建表头
        var newCols = []
        for (var j = 0; j < cols.length; j++) {
            var col = {}
            for (var prop in cols[j]) {
                col[prop] = cols[j][prop]
            }
            if (j === nameIndex) {
                col.width = nameWidth
            }
            newCols.push(col)
        }
        table_view.columnSource = newCols
        if (table_view.view) {
            table_view.view.forceLayout()
        }
    }
}
