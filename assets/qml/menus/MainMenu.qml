// Copyright 2026-2026 the openage authors. See copying.md for legal info.

import QtQuick 2.15

MenuFrame {
	body: "Free and open Age of Empires engine. Configure a match, then take the field."

	MenuButton {
		text: "New Game"
		onClicked: menuController.startGame()
	}
	MenuButton {
		text: "Quit"
		onClicked: menuController.quit()
	}
}
