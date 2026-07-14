// Copyright 2026-2026 the openage authors. See copying.md for legal info.

import QtQuick 2.15
import QtQuick.Controls 2.15

/**
 * Shared menu chrome: brand mark, title, body, and a vertical CTA column.
 */
Item {
	id: root

	property string title: ""
	property string body: ""
	property bool compact: false
	default property alias content: buttonColumn.data

	readonly property color ink: "#0B1C24"
	readonly property color mist: "#1A3340"
	readonly property color parchment: "#E8DFC8"
	readonly property color bronze: "#C4A35A"

	Rectangle {
		anchors.fill: parent
		gradient: Gradient {
			GradientStop { position: 0.0; color: root.ink }
			GradientStop { position: 0.55; color: root.mist }
			GradientStop { position: 1.0; color: "#071015" }
		}
	}

	Canvas {
		anchors.fill: parent
		opacity: 0.08
		onPaint: {
			var ctx = getContext("2d")
			ctx.strokeStyle = root.parchment
			ctx.lineWidth = 1
			var step = 48
			for (var x = 0; x < width; x += step) {
				ctx.beginPath()
				ctx.moveTo(x, 0)
				ctx.lineTo(x, height)
				ctx.stroke()
			}
			for (var y = 0; y < height; y += step) {
				ctx.beginPath()
				ctx.moveTo(0, y)
				ctx.lineTo(width, y)
				ctx.stroke()
			}
		}
		Component.onCompleted: requestPaint()
		onWidthChanged: requestPaint()
		onHeightChanged: requestPaint()
	}

	Column {
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.verticalCenter: parent.verticalCenter
		anchors.verticalCenterOffset: root.compact ? 0 : -height * 0.05
		spacing: root.compact ? 18 : 28
		width: Math.min(parent.width * 0.72, 420)

		Text {
			id: brand
			width: parent.width
			horizontalAlignment: Text.AlignHCenter
			text: "openage"
			color: root.bronze
			font.family: "Palatino Linotype"
			font.pixelSize: root.compact ? 42 : 64
			font.letterSpacing: 4
			opacity: 0
			scale: 0.92

			SequentialAnimation on opacity {
				id: brandFade
				running: false
				NumberAnimation { to: 1; duration: 700; easing.type: Easing.OutCubic }
			}
			SequentialAnimation on scale {
				id: brandScale
				running: false
				NumberAnimation { to: 1; duration: 700; easing.type: Easing.OutCubic }
			}
		}

		Text {
			id: titleText
			width: parent.width
			horizontalAlignment: Text.AlignHCenter
			text: root.title
			color: root.parchment
			font.family: "Palatino Linotype"
			font.pixelSize: root.compact ? 22 : 28
			visible: root.title.length > 0
			opacity: 0
			SequentialAnimation on opacity {
				id: titleFade
				running: false
				PauseAnimation { duration: 120 }
				NumberAnimation { to: 1; duration: 450; easing.type: Easing.OutCubic }
			}
		}

		Text {
			id: bodyText
			width: parent.width
			horizontalAlignment: Text.AlignHCenter
			wrapMode: Text.WordWrap
			text: root.body
			color: "#B8C4C8"
			font.family: "Georgia"
			font.pixelSize: 15
			visible: root.body.length > 0
			opacity: 0
			SequentialAnimation on opacity {
				id: bodyFade
				running: false
				PauseAnimation { duration: 200 }
				NumberAnimation { to: 1; duration: 500; easing.type: Easing.OutCubic }
			}
		}

		Column {
			id: buttonColumn
			anchors.horizontalCenter: parent.horizontalCenter
			spacing: 12
			width: parent.width
		}
	}

	function playIntro() {
		brand.opacity = 0
		brand.scale = 0.92
		titleText.opacity = 0
		bodyText.opacity = 0
		brandFade.restart()
		brandScale.restart()
		titleFade.restart()
		bodyFade.restart()
		if (buttonColumn.children.length > 0) {
			buttonColumn.children[0].forceActiveFocus()
		}
	}

	onVisibleChanged: {
		if (visible) {
			playIntro()
		}
	}

	Component.onCompleted: {
		if (visible) {
			playIntro()
		}
	}
}
