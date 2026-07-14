// Copyright 2026-2026 the openage authors. See copying.md for legal info.

import QtQuick 2.15
import yay.sfttech.livereload 1.0

/**
 * Root shell for the presenter GUI: main menu, in-game HUD placeholder,
 * pause overlay, and after-game statistics.
 *
 * Expects context property `menuController` (presenter::MenuController).
 */
Item {
	id: root
	focus: true

	Keys.onPressed: function(event) {
		if (event.key === Qt.Key_Escape) {
			menuController.togglePause()
			event.accepted = true
		}
	}

	MainMenu {
		anchors.fill: parent
		visible: menuController.screen === "main"
		opacity: visible ? 1 : 0
		Behavior on opacity { NumberAnimation { duration: 180 } }
	}

	// Transparent pass-through while playing so world input reaches the window.
	Item {
		anchors.fill: parent
		visible: menuController.screen === "game"
		enabled: false
	}

	PauseMenu {
		anchors.fill: parent
		visible: menuController.screen === "pause"
		opacity: visible ? 1 : 0
		Behavior on opacity { NumberAnimation { duration: 140 } }
	}

	GameOverMenu {
		anchors.fill: parent
		visible: menuController.screen === "gameover"
		opacity: visible ? 1 : 0
		Behavior on opacity { NumberAnimation { duration: 200 } }
	}

	Component.onCompleted: forceActiveFocus()
}
