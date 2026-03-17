import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: window

    width: 1460
    height: 920
    minimumWidth: 1180
    minimumHeight: 760
    visible: true
    title: "LusaKey"

    readonly property color shell: "#F3EDE4"
    readonly property color panel: "#FBF7F1"
    readonly property color panelSoft: "#F7F1E8"
    readonly property color borderTone: "#D7C9B8"
    readonly property color ink: "#241C15"
    readonly property color mutedInk: "#6E6255"
    readonly property color accent: "#C46D3C"
    readonly property color accentSoft: "#E7B08D"
    readonly property color danger: "#B35B46"
    property bool revealMaster: false
    property bool revealVaultPassword: false

    function strengthColor(score) {
        if (score <= 0) return "#D8C8B5"
        if (score === 1) return "#C98A5B"
        if (score === 2) return "#C8A050"
        if (score === 3) return "#73926A"
        return "#4F7A5D"
    }

    function syncFormFromSelection() {
        const entry = controller.selectedEntry
        titleField.text = entry.title || ""
        usernameField.text = entry.username || ""
        vaultPasswordField.text = entry.password || ""
        websiteField.text = entry.website || ""
        tagsField.text = entry.tagsText || ""
        notesField.text = entry.notes || ""
        favoriteBox.checked = entry.favorite || false
    }

    function saveCurrentForm() {
        return controller.saveEntry({
            id: controller.selectedEntryId,
            title: titleField.text,
            username: usernameField.text,
            password: vaultPasswordField.text,
            website: websiteField.text,
            tagsText: tagsField.text,
            notes: notesField.text,
            favorite: favoriteBox.checked,
            createdAt: controller.selectedEntry.createdAt
        })
    }

    Shortcut { sequence: StandardKey.New; enabled: controller.vaultUnlocked; onActivated: controller.beginCreateEntry() }
    Shortcut { sequence: StandardKey.Save; enabled: controller.vaultUnlocked && controller.hasSelection; onActivated: saveCurrentForm() }
    Shortcut { sequence: "Ctrl+L"; enabled: controller.vaultUnlocked; onActivated: controller.lockVault() }

    Connections {
        target: controller
        function onSelectedEntryChanged() { syncFormFromSelection() }
        function onStatusMessageChanged() { if (controller.statusMessage.length > 0) bannerTimer.restart() }
        function onErrorMessageChanged() { if (controller.errorMessage.length > 0) bannerTimer.restart() }
    }

    Timer {
        id: bannerTimer
        interval: 4200
        onTriggered: controller.clearMessages()
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#F6F0E7" }
            GradientStop { position: 1.0; color: "#EFE4D6" }
        }
    }

    Rectangle { width: 540; height: 540; radius: 270; x: width - 430; y: -120; color: "#F8DCC6"; opacity: 0.25 }
    Rectangle { width: 420; height: 420; radius: 210; x: -120; y: height - 270; color: "#E2D6C5"; opacity: 0.32 }

    Rectangle {
        visible: controller.statusMessage.length > 0 || controller.errorMessage.length > 0
        z: 40
        anchors.top: parent.top
        anchors.topMargin: 22
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(parent.width - 40, 640)
        height: bannerText.implicitHeight + 24
        radius: 18
        color: controller.errorMessage.length > 0 ? "#F6E2DB" : "#EEE4D7"
        border.color: controller.errorMessage.length > 0 ? "#D79A8C" : borderTone

        Text {
            id: bannerText
            anchors.fill: parent
            anchors.margins: 12
            wrapMode: Text.Wrap
            color: controller.errorMessage.length > 0 ? danger : ink
            text: controller.errorMessage.length > 0 ? controller.errorMessage : controller.statusMessage
            font.pixelSize: 15
        }
    }

    Item {
        anchors.fill: parent
        anchors.margins: 36
        visible: !controller.vaultUnlocked

        Rectangle {
            width: 520
            height: controller.vaultExists ? 430 : 560
            anchors.centerIn: parent
            radius: 34
            color: panel
            border.color: borderTone

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 34
                spacing: 18

                Text { text: "LusaKey"; color: ink; font.family: Qt.platform.os === "windows" ? "Georgia" : "Noto Serif"; font.pixelSize: 34; font.weight: Font.DemiBold }
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: controller.vaultExists ? "Unlock your encrypted local vault." : "Create a local vault for Windows and Linux with AES-256-GCM encryption."
                    color: mutedInk
                    font.pixelSize: 16
                }
                Rectangle { Layout.fillWidth: true; height: 1; color: borderTone }
                TextField { id: masterPasswordField; Layout.fillWidth: true; placeholderText: controller.vaultExists ? "Master password" : "Create master password"; echoMode: revealMaster ? TextInput.Normal : TextInput.Password }
                TextField { id: confirmPasswordField; Layout.fillWidth: true; visible: !controller.vaultExists; placeholderText: "Confirm master password"; echoMode: revealMaster ? TextInput.Normal : TextInput.Password }
                CheckBox { text: "Show password"; checked: revealMaster; onToggled: revealMaster = checked }
                Text { visible: !controller.vaultExists && confirmPasswordField.text.length > 0 && confirmPasswordField.text !== masterPasswordField.text; color: danger; text: "Master passwords do not match."; font.pixelSize: 14 }
                Item { Layout.fillHeight: true }
                Button {
                    Layout.fillWidth: true
                    text: controller.vaultExists ? "Unlock vault" : "Create vault"
                    enabled: controller.vaultExists ? masterPasswordField.text.length > 0 : masterPasswordField.text.length >= 10 && masterPasswordField.text === confirmPasswordField.text
                    onClicked: {
                        if (controller.vaultExists) {
                            if (controller.unlockVault(masterPasswordField.text)) masterPasswordField.text = ""
                        } else {
                            if (controller.createVault(masterPasswordField.text)) {
                                masterPasswordField.text = ""
                                confirmPasswordField.text = ""
                            }
                        }
                    }
                    background: Rectangle { radius: 16; color: parent.enabled ? accent : "#D7CFC4" }
                    contentItem: Text { text: parent.text; color: parent.enabled ? "#FFF8F2" : "#847868"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 16; font.weight: Font.DemiBold }
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 20
        visible: controller.vaultUnlocked

        Rectangle {
            Layout.preferredWidth: 360
            Layout.fillHeight: true
            radius: 32
            color: panelSoft
            border.color: borderTone

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 24
                spacing: 16

                Text { text: "LusaKey"; color: ink; font.family: Qt.platform.os === "windows" ? "Georgia" : "Noto Serif"; font.pixelSize: 30; font.weight: Font.DemiBold }
                Text { Layout.fillWidth: true; wrapMode: Text.Wrap; text: controller.storagePath; color: mutedInk; font.pixelSize: 12 }
                RowLayout {
                    Layout.fillWidth: true
                    TextField { id: searchField; Layout.fillWidth: true; placeholderText: "Search vault"; onTextChanged: controller.setSearchQuery(text) }
                    Button { text: "Lock"; onClicked: controller.lockVault() }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Text { text: controller.vaultModel.visibleCount + " of " + controller.vaultModel.totalCount + " entries"; color: mutedInk; font.pixelSize: 13 }
                    Item { Layout.fillWidth: true }
                    CheckBox { text: "Favorites"; onToggled: controller.setFavoritesOnly(checked) }
                }
                Button {
                    Layout.fillWidth: true
                    text: "New entry"
                    onClicked: controller.beginCreateEntry()
                    background: Rectangle { radius: 16; color: accent }
                    contentItem: Text { text: parent.text; color: "#FFF8F2"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 16; font.weight: Font.DemiBold }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    radius: 24
                    color: panel
                    border.color: borderTone

                    ListView {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 10
                        clip: true
                        model: controller.vaultModel

                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 96
                            radius: 22
                            color: controller.selectedEntryId === entryId ? "#F2E7D9" : "#FCF9F5"
                            border.color: controller.selectedEntryId === entryId ? accentSoft : borderTone
                            border.width: controller.selectedEntryId === entryId ? 2 : 1

                            Column {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 6

                                Row {
                                    width: parent.width
                                    spacing: 8
                                    Rectangle { width: 10; height: 10; radius: 5; color: favorite ? accent : "#D8CCBE" }
                                    Text { text: title; color: ink; font.pixelSize: 16; font.weight: Font.DemiBold; elide: Text.ElideRight; width: parent.width - 18 }
                                }

                                Text { text: subtitle; color: mutedInk; font.pixelSize: 14; elide: Text.ElideRight; width: parent.width }
                                Text { text: updatedAtLabel; color: "#8E7F70"; font.pixelSize: 12 }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: controller.selectEntryByRow(index)
                            }
                        }

                        ScrollBar.vertical: ScrollBar { }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 32
            color: panel
            border.color: borderTone

            Item {
                anchors.fill: parent
                anchors.margins: 28
                visible: !controller.hasSelection

                Column {
                    anchors.centerIn: parent
                    spacing: 16

                    Text { text: "A calm place for sensitive details"; color: ink; font.family: Qt.platform.os === "windows" ? "Georgia" : "Noto Serif"; font.pixelSize: 30; font.weight: Font.DemiBold }
                    Text { width: 420; wrapMode: Text.Wrap; text: "Create an entry, then store usernames, passwords, notes, recovery codes, and tags in one local encrypted vault."; color: mutedInk; font.pixelSize: 16 }
                    Button {
                        text: "Create first entry"
                        onClicked: controller.beginCreateEntry()
                        background: Rectangle { radius: 16; color: accent }
                        contentItem: Text { text: parent.text; color: "#FFF8F2"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 16; font.weight: Font.DemiBold }
                    }
                }
            }

            ScrollView {
                id: editorScroll
                anchors.fill: parent
                anchors.margins: 24
                visible: controller.hasSelection
                clip: true

                Column {
                    width: editorScroll.availableWidth
                    spacing: 18

                    RowLayout {
                        width: parent.width
                        ColumnLayout {
                            Layout.fillWidth: true
                            Text { text: titleField.text.length > 0 ? titleField.text : "Entry details"; color: ink; font.family: Qt.platform.os === "windows" ? "Georgia" : "Noto Serif"; font.pixelSize: 30; font.weight: Font.DemiBold }
                            Text { text: controller.selectedEntry.updatedAtLabel || "Unsaved draft"; color: mutedInk; font.pixelSize: 13 }
                        }
                        CheckBox { id: favoriteBox; text: "Favorite" }
                    }

                    TextField { id: titleField; width: parent.width; placeholderText: "Entry title"; font.pixelSize: 18 }

                    GridLayout {
                        width: parent.width
                        columns: 2
                        rowSpacing: 16
                        columnSpacing: 16
                        TextField { id: usernameField; Layout.fillWidth: true; placeholderText: "Username or email" }
                        TextField { id: websiteField; Layout.fillWidth: true; placeholderText: "Website or app URL" }
                    }

                    RowLayout {
                        width: parent.width
                        TextField { id: vaultPasswordField; Layout.fillWidth: true; placeholderText: "Password"; echoMode: revealVaultPassword ? TextInput.Normal : TextInput.Password; selectByMouse: true }
                        Button { text: revealVaultPassword ? "Hide" : "Show"; onClicked: revealVaultPassword = !revealVaultPassword }
                        Button { text: "Copy"; onClicked: controller.copyText(vaultPasswordField.text) }
                    }

                    Column {
                        width: parent.width
                        spacing: 10

                        RowLayout {
                            width: parent.width
                            Text { text: "Password strength"; color: mutedInk; font.pixelSize: 14 }
                            Item { Layout.fillWidth: true }
                            Text { text: controller.strengthLabel(vaultPasswordField.text); color: strengthColor(controller.estimateStrengthScore(vaultPasswordField.text)); font.pixelSize: 14; font.weight: Font.DemiBold }
                        }

                        Rectangle {
                            width: parent.width
                            height: 8
                            radius: 4
                            color: "#E2D6C7"
                            Rectangle {
                                width: parent.width * Math.max(0.1, (controller.estimateStrengthScore(vaultPasswordField.text) + 1) / 5)
                                height: parent.height
                                radius: 4
                                color: strengthColor(controller.estimateStrengthScore(vaultPasswordField.text))
                            }
                        }
                    }

                    Rectangle {
                        width: parent.width
                        height: 150
                        radius: 22
                        color: panelSoft
                        border.color: borderTone

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 16
                            spacing: 10

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "Password generator"; color: ink; font.pixelSize: 16; font.weight: Font.DemiBold }
                                Item { Layout.fillWidth: true }
                                Text { text: Math.round(lengthSlider.value) + " chars"; color: mutedInk; font.pixelSize: 14 }
                            }

                            Slider { id: lengthSlider; Layout.fillWidth: true; from: 10; to: 40; value: 18 }

                            RowLayout {
                                Layout.fillWidth: true
                                CheckBox { id: upperBox; text: "ABC"; checked: true }
                                CheckBox { id: lowerBox; text: "abc"; checked: true }
                                CheckBox { id: digitsBox; text: "123"; checked: true }
                                CheckBox { id: symbolsBox; text: "#$!"; checked: true }
                                Item { Layout.fillWidth: true }
                                Button {
                                    text: "Generate"
                                    onClicked: vaultPasswordField.text = controller.generatePassword(
                                        Math.round(lengthSlider.value),
                                        upperBox.checked,
                                        lowerBox.checked,
                                        digitsBox.checked,
                                        symbolsBox.checked
                                    )
                                    background: Rectangle { radius: 14; color: accent }
                                    contentItem: Text { text: parent.text; color: "#FFF8F2"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 14; font.weight: Font.DemiBold }
                                }
                            }
                        }
                    }

                    TextField { id: tagsField; width: parent.width; placeholderText: "Tags, comma separated" }
                    Rectangle {
                        id: notesArea
                        width: parent.width
                        height: 210
                        radius: 22
                        color: panelSoft
                        border.color: borderTone

                        TextArea {
                            id: notesField
                            anchors.fill: parent
                            anchors.margins: 14
                            wrapMode: TextEdit.Wrap
                            selectByMouse: true
                            placeholderText: "Notes, recovery codes, or private context"
                            background: null
                        }
                    }

                    RowLayout {
                        width: parent.width
                        Button {
                            text: "Save entry"
                            onClicked: saveCurrentForm()
                            background: Rectangle { radius: 16; color: accent }
                            contentItem: Text { text: parent.text; color: "#FFF8F2"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 16; font.weight: Font.DemiBold }
                        }
                        Button { text: "Copy username"; onClicked: controller.copyText(usernameField.text) }
                        Item { Layout.fillWidth: true }
                        Button { text: "Delete"; onClicked: controller.deleteCurrentEntry() }
                    }

                    Text {
                        width: parent.width
                        wrapMode: Text.Wrap
                        color: mutedInk
                        font.pixelSize: 13
                        text: "Everything lives in one local encrypted vault file. Use Ctrl+S to save, Ctrl+N for a new entry, and Ctrl+L to lock."
                    }
                }
            }
        }
    }
}
