// Copyright 2026-2026 the openage authors. See copying.md for legal info.

import QtQuick 2.15

Item {
	anchors.fill: parent

	Rectangle {
		anchors.fill: parent
		color: "#CC071015"
	}

	MenuFrame {
		anchors.fill: parent
		compact: true
		title: "Paused"
		body: "Simulation clock is held. Resume when you are ready."

		MenuButton {
			text: "Resume"
			onClicked: menuController.resumeGame()
		}
		MenuButton {
			text: "Main Menu"
			onClicked: menuController.quitToMenu()
		}
		MenuButton {
			text: "Quit"
			onClicked: menuController.quit()
		}
	}
}
