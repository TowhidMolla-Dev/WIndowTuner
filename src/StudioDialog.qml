import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Dialog {
    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.background: "#0f171d"
    padding: 20
    background: Rectangle { color: "#0f171d"; border.color: "#324451"; radius: 10 }
    Overlay.modal: Rectangle { color: "#99000000" }
}
