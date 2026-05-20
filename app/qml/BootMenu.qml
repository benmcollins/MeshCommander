// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2026 Ben Collins <ben@ironrocketsmc.org>

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QuMesh

/// One source of truth for the "Boot to…" menu items shared by
/// SessionWindow and MachineDetailsWindow. Pre-#299 each window held a
/// hand-maintained copy of the same fourteen MenuItems (BIOS / PXE /
/// IDE-R CDROM / IDE-R Floppy × power-on/reset, plus Secure Erase /
/// Platform Erase / HTTPS / OCR). The MachineDetails copy had drifted
/// ahead — SessionWindow was missing the cap-gated items entirely.
///
/// Caller wires in:
///
/// - `controller` — a MachineDetailsController (or anything quacking
///   like one: exposes the `bootTo*` slots and the `cap*` properties).
/// - `targetHost` — the host string the confirm dialog quotes.
/// - `confirmDialog` — a ConfirmDialog with `askFor(t, b, verb, fn)`.
/// - Optional `secureErasePrompt` / `platformErasePrompt` /
///   `httpsBootPrompt` / `ocrPrompt`. If any are null the matching
///   MenuItem hides itself even when the AMT capability bit is on —
///   SessionWindow uses that to opt out of the extra prompts without
///   carrying their full QML weight.
Menu {
    id: root

    required property var controller
    required property string targetHost
    required property var confirmDialog

    /// Set these from MachineDetailsWindow where the prompts live.
    /// Leave null in SessionWindow (the items hide).
    property var secureErasePrompt: null
    property var platformErasePrompt: null
    property var httpsBootPrompt: null
    property var ocrPrompt: null

    title: qsTr("Boot to…")

    MenuItem {
        text: qsTr("Power on to BIOS Setup")
        onTriggered: root.controller.bootToBios(false)
    }
    MenuItem {
        text: qsTr("Reset to BIOS Setup")
        onTriggered: root.confirmDialog.askFor(
            qsTr("Reset \"%1\" to BIOS Setup?").arg(root.targetHost),
            qsTr("Hard-resets the target into the BIOS Setup screen."),
            qsTr("Reset"),
            () => root.controller.bootToBios(true))
    }
    MenuSeparator {}
    MenuItem {
        text: qsTr("Power on to PXE")
        onTriggered: root.controller.bootToPxe(false)
    }
    MenuItem {
        text: qsTr("Reset to PXE")
        onTriggered: root.confirmDialog.askFor(
            qsTr("Reset \"%1\" to PXE?").arg(root.targetHost),
            qsTr("Hard-resets the target into PXE network boot."),
            qsTr("Reset"),
            () => root.controller.bootToPxe(true))
    }
    MenuSeparator {}
    MenuItem {
        text: qsTr("Power on to IDE-R CDROM")
        onTriggered: root.controller.bootToIderCdrom(false)
    }
    MenuItem {
        text: qsTr("Reset to IDE-R CDROM")
        onTriggered: root.confirmDialog.askFor(
            qsTr("Reset \"%1\" to IDE-R CDROM?").arg(root.targetHost),
            qsTr("Hard-resets the target into the redirected CD/DVD."),
            qsTr("Reset"),
            () => root.controller.bootToIderCdrom(true))
    }
    MenuItem {
        text: qsTr("Power on to IDE-R Floppy")
        onTriggered: root.controller.bootToIderFloppy(false)
    }
    MenuItem {
        text: qsTr("Reset to IDE-R Floppy")
        onTriggered: root.confirmDialog.askFor(
            qsTr("Reset \"%1\" to IDE-R Floppy?").arg(root.targetHost),
            qsTr("Hard-resets the target into the redirected floppy."),
            qsTr("Reset"),
            () => root.controller.bootToIderFloppy(true))
    }

    // Secure Erase — requires AMT capability bit + a host-side prompt
    // that captures the password and confirms intent (#170).
    MenuSeparator {
        visible: root.controller.capSecureErase && root.secureErasePrompt
        height: visible ? implicitHeight : 0
    }
    MenuItem {
        visible: root.controller.capSecureErase && root.secureErasePrompt
        height: visible ? implicitHeight : 0
        text: qsTr("Power on to Secure Erase…")
        onTriggered: root.secureErasePrompt.openFor(false)
    }
    MenuItem {
        visible: root.controller.capSecureErase && root.secureErasePrompt
        height: visible ? implicitHeight : 0
        text: qsTr("Reset to Secure Erase…")
        onTriggered: root.secureErasePrompt.openFor(true)
    }

    MenuSeparator {
        visible: root.controller.capPlatformErase && root.platformErasePrompt
        height: visible ? implicitHeight : 0
    }
    MenuItem {
        visible: root.controller.capPlatformErase && root.platformErasePrompt
        height: visible ? implicitHeight : 0
        text: qsTr("Power on to Platform Erase…")
        onTriggered: root.platformErasePrompt.openFor(false)
    }
    MenuItem {
        visible: root.controller.capPlatformErase && root.platformErasePrompt
        height: visible ? implicitHeight : 0
        text: qsTr("Reset to Platform Erase…")
        onTriggered: root.platformErasePrompt.openFor(true)
    }

    MenuSeparator {
        visible: root.controller.capForceUefiHttpsBoot && root.httpsBootPrompt
        height: visible ? implicitHeight : 0
    }
    MenuItem {
        visible: root.controller.capForceUefiHttpsBoot && root.httpsBootPrompt
        height: visible ? implicitHeight : 0
        text: qsTr("Power on to HTTPS URL…")
        onTriggered: root.httpsBootPrompt.openFor(false)
    }
    MenuItem {
        visible: root.controller.capForceUefiHttpsBoot && root.httpsBootPrompt
        height: visible ? implicitHeight : 0
        text: qsTr("Reset to HTTPS URL…")
        onTriggered: root.httpsBootPrompt.openFor(true)
    }

    // One-Click Recovery (#170) — the prompt picks which of WinRE /
    // Local PBA / HTTPS-with-pinning to fire based on the per-flavor
    // capability bits, so we expose it as one entry.
    MenuSeparator {
        visible: (root.controller.capForceWinReBoot
                  || root.controller.capForceUefiLocalPbaBoot
                  || root.controller.capForceUefiHttpsBoot)
              && root.ocrPrompt
        height: visible ? implicitHeight : 0
    }
    MenuItem {
        visible: (root.controller.capForceWinReBoot
                  || root.controller.capForceUefiLocalPbaBoot
                  || root.controller.capForceUefiHttpsBoot)
              && root.ocrPrompt
        height: visible ? implicitHeight : 0
        text: qsTr("Power on to recovery…")
        onTriggered: root.ocrPrompt.openFor(false)
    }
    MenuItem {
        visible: (root.controller.capForceWinReBoot
                  || root.controller.capForceUefiLocalPbaBoot
                  || root.controller.capForceUefiHttpsBoot)
              && root.ocrPrompt
        height: visible ? implicitHeight : 0
        text: qsTr("Reset to recovery…")
        onTriggered: root.ocrPrompt.openFor(true)
    }
}
