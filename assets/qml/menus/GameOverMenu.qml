// Copyright 2026-2026 the openage authors. See copying.md for legal info.

import QtQuick 2.15

Item {
	anchors.fill: parent

	Rectangle {
		anchors.fill: parent
		color: "#E0071015"
	}

	MenuFrame {
		anchors.fill: parent
		compact: true
		title: menuController.hasWinner ? (menuController.winnerLabel + " wins") : "Game Over"
		body: "Units killed: " + menuController.unitsKilled
			  + "\nUnits lost: " + menuController.unitsLost
			  + "\nResources gathered: " + menuController.resourcesGathered
			  + "\nAPM: " + Math.round(menuController.apm)

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
