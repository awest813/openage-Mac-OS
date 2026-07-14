// Copyright 2026-2026 the openage authors. See copying.md for legal info.

import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
	id: root

	property color bronze: "#C4A35A"
	property color bronzeDim: "#8F7638"
	property color ink: "#0B1C24"
	property color parchment: "#E8DFC8"

	implicitWidth: Math.max(220, contentItem.implicitWidth + 40)
	implicitHeight: 44
	anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined

	background: Rectangle {
		radius: 2
		color: root.down ? root.bronzeDim : (root.hovered ? root.bronze : "#1F2F36")
		border.color: root.bronze
		border.width: 1

		Behavior on color { ColorAnimation { duration: 120 } }
	}

	contentItem: Text {
		text: root.text
		horizontalAlignment: Text.AlignHCenter
		verticalAlignment: Text.AlignVCenter
		color: root.down || root.hovered ? root.ink : root.parchment
		font.family: "Georgia"
		font.pixelSize: 16
		font.letterSpacing: 1
	}

	scale: root.down ? 0.98 : 1.0
	Behavior on scale { NumberAnimation { duration: 90 } }
}
