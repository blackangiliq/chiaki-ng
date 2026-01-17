import QtQuick

// ═══════════════════════════════════════════════════════════════
// 🔒 APP CONFIGURATION - CHANGE APP NAME HERE ONLY
// ═══════════════════════════════════════════════════════════════
// The app name is defined here ONCE and used everywhere.
// To change the name, modify ONLY this file.
// ═══════════════════════════════════════════════════════════════

QtObject {
    // ════════════════════════════════════════════════════════════
    // 🎮 APP NAME - CHANGE THIS VALUE TO RENAME THE APP
    // ════════════════════════════════════════════════════════════
    readonly property string appName: "Urscript"
    
    // App version (synced with Qt.application.version)
    readonly property string appVersion: Qt.application.version
    
    // Full display name
    readonly property string displayName: appName + " v" + appVersion
}
