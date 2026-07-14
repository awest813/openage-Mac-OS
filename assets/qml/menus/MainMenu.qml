// Copyright 2026-2026 the openage authors. See copying.md for legal info.

import QtQuick 2.15

MenuFrame {
	body: "Free and open Age of Empires engine. Enter the current match when you are ready. Full match restart is not available yet."

	MenuButton {
		text: "Start"
		onClicked: menuController.startGame()
	}
	MenuButton {
		text: "Quit"
		onClicked: menuController.quit()
	}
}
