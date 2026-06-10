// Copyright 2015-2017 the openage authors. See copying.md for legal info.

import QtQuick 2.4
import QtQuick.Controls 1.1
import QtQuick.Layouts 1.1

import yay.sfttech.openage 1.0 as OA

Item {
	id: root

	property var actionMode
	property string playerName
	property int civIndex

	readonly property int topStripSubid: 0
	readonly property int midStripSubid: 1
	readonly property int leftRectSubid: 2
	readonly property int rightRectSubid: 3
	readonly property int resBaseSubid: 4

	readonly property string srcPrefix: "image://by-filename/converted/interface/hud"
	readonly property string pad: "0000"
	readonly property string srcSuffix: ".slp.png"
	property string hudImageSource: srcPrefix + (pad + civIndex).slice(-pad.length) + srcSuffix

	readonly property string iconsPrefix: "image://by-filename/converted/interface/"
	readonly property string iconsBorder: "image://by-filename/converted/interface/53003.slp.png.1"
	readonly property real ownerFieldWidthRatio: 0.35

	function prettifyLabel(label) {
		if (!label)
			return ""

		var withSpaces = ("" + label).replace(/_/g, " ")
		var words = withSpaces.split(" ")
		for (var i = 0; i < words.length; ++i) {
			if (words[i].length > 0) {
				words[i] = words[i].charAt(0).toUpperCase() + words[i].slice(1)
			}
		}
		return words.join(" ")
	}

	width: 1289
	height: 960

	Item {
		anchors.left: parent.left
		anchors.right: parent.right
		anchors.top: parent.top

		height: metricsUnit * 1.5 * 2.5

		Image {
			anchors.fill: parent
			source: hudImageSource + "." + root.topStripSubid

			sourceSize.height: parent.height
			fillMode: Image.Tile
		}

		RowLayout {
			anchors.fill: parent
			anchors.leftMargin: metricsUnit
			anchors.rightMargin: metricsUnit
			anchors.verticalCenter: parent.verticalCenter
			spacing: metricsUnit * 0.7

			Row {
				spacing: metricsUnit * 0.7

				Component {
					id: resourceIndicator

					Rectangle {
						property string amount
						property int iconIndex
						property bool warning

						width: metricsUnit * 1.5 * 6.5
						height: metricsUnit * 1.5 * 1.7

						color: "#7FFFFFFF"

						Rectangle {
							anchors.fill: parent
							anchors.rightMargin: metricsUnit * 0.3
							anchors.bottomMargin: metricsUnit * 0.3

							color: "black"

							Image {
								sourceSize.height: parent.height

								source: hudImageSource + "." + (root.resBaseSubid + iconIndex)
								fillMode: Image.PreserveAspectFit
							}

							Text {
								anchors.right: parent.right
								anchors.rightMargin: metricsUnit / 2
								anchors.verticalCenter: parent.verticalCenter
								text: amount

								color: "white"
							}
						}

						Rectangle {
							anchors.fill: parent
							anchors.rightMargin: metricsUnit * 0.3
							anchors.bottomMargin: metricsUnit * 0.3

							visible: warning

							color: "#80FFC100"

							SequentialAnimation on opacity {
								loops: Animation.Infinite
								PropertyAnimation { from: 0; to: 1; duration: 250 }
								PropertyAnimation { from: 1; to: 0; duration: 250 }
							}
						}
					}
				}

				Repeater {
					model: OA.Resources {
						actionMode: root.actionMode
					}

					delegate: Loader {
						sourceComponent: resourceIndicator

						onLoaded: {
							item.amount = Qt.binding(function() { return display })
							item.iconIndex = Qt.binding(function() { return index })
						}
					}
				}

				Loader {
					sourceComponent: resourceIndicator

					onLoaded: {
						item.amount = Qt.binding(function() { return root.actionMode.population })
						item.iconIndex = 4
						item.warning = Qt.binding(function() { return root.actionMode.population_warn })
					}
				}
			}

			Item {
				Layout.fillWidth: true
				Layout.minimumWidth: epoch.implicitWidth

				Rectangle {
					anchors.centerIn: parent
					width: 200
					height: metricsUnit * 2.5

					color: "black"

					Text {
						id: epoch
						anchors.centerIn: parent

						color: "white"
						text: root.playerName
					}
				}
			}

			Repeater {
				model: 5

				Rectangle {
					width: metricsUnit * 1.5 * 5
					height: metricsUnit * 1.5 * 1.5

					color: "transparent"
					border.width: 1
					border.color: "white"
				}
			}
		}
	}

	Item {
		anchors.left: parent.left
		anchors.right: parent.right
		anchors.bottom: parent.bottom

		height: metricsUnit * 1.5 * 16

		Image {
			id: leftRect

			anchors.left: parent.left
			anchors.top: parent.top
			anchors.bottom: parent.bottom

			width: height * 1.63

			source: hudImageSource + "." + root.leftRectSubid
			fillMode: Image.Stretch

			ActionsGrid {
				id: actionsGrid

				anchors.fill: parent
				anchors.topMargin: parent.height * 40 / 218
				anchors.leftMargin: parent.height * 40 / 218 + metricsUnit * 1.4
				anchors.rightMargin: parent.height * 40 / 218 + metricsUnit * 0.5
				anchors.bottomMargin: parent.height * 40 / 218 - metricsUnit * 1.4

				columns: 5
				rows: 3

				Actions {
					id: actions

					actionMode: root.actionMode
				}

				buttonActions: actions.actionObjects
			}
		}

		ColumnLayout {
			anchors.left: leftRect.right
			anchors.right: rightRect.left
			anchors.top: parent.top
			anchors.bottom: parent.bottom

			spacing: 0

			Image {
				id: borderStrip

				Layout.fillWidth: true

				sourceSize.height: metricsUnit * 1.4

				source: hudImageSource + "." + root.midStripSubid
				fillMode: Image.Tile
			}

			Rectangle {
				Layout.fillHeight: true
				Layout.fillWidth: true

				color: "#41110d"

				Paper {
					anchors.fill: parent
					tornBottom: false
				}

				Item {
					anchors.fill: parent
					id: selection_single_panel
					visible: root.actionMode.selection_size == 1

					Rectangle {
						anchors.fill: parent
						anchors.margins: metricsUnit * 0.8
						color: "#66FFF6DD"
						border.width: 1
						border.color: "#66422C19"
					}

					Image {
						anchors.top: parent.top
						anchors.topMargin: metricsUnit * 2
						anchors.left: parent.left
						anchors.leftMargin: metricsUnit * 2

						width: 50
						height: 50

						id: selected_icon

						source: root.actionMode.selection_icon ? iconsPrefix + root.actionMode.selection_icon : iconsBorder
					}
					Image {
						anchors.centerIn: selected_icon

						width: 50
						height: 50

						source: iconsBorder
					}

					Text {
						anchors.top: selected_icon.top
						anchors.left: selected_icon.right
						anchors.leftMargin: metricsUnit * 1.2

						id: selected_name
						property real ownerFieldReserveWidth: (parent.width * root.ownerFieldWidthRatio) + (metricsUnit * 3)

						color: "black"
						text: root.actionMode.selection_name
						font.pointSize: 15
						font.bold: true
						anchors.right: parent.right
						anchors.rightMargin: ownerFieldReserveWidth
						elide: Text.ElideRight
					}

					Text {
						anchors.top: selected_name.bottom
						anchors.left: selected_name.left
						anchors.topMargin: metricsUnit * 0.5

						id: selected_type

						color: "black"
						opacity: 0.8
						text: root.actionMode.selection_type
						font.pointSize: 11
						width: selected_name.width
						elide: Text.ElideRight
					}

					Text {
						anchors.top: selected_type.bottom
						anchors.left: selected_name.left
						anchors.topMargin: metricsUnit * 0.8

						id: selected_hp

						color: "black"
						text: root.actionMode.selection_hp
						font.bold: true
					}

					Text {
						anchors.top: selected_hp.bottom
						anchors.left: selected_icon.left
						anchors.topMargin: metricsUnit * 1.5

						color: "black"
						text: root.actionMode.selection_attrs
						width: parent.width - metricsUnit * 4
						wrapMode: Text.WordWrap
						font.pointSize: 11
					}

					Text {
						anchors.top: parent.top
						anchors.right: parent.right
						anchors.topMargin: metricsUnit * 2
						anchors.rightMargin: metricsUnit * 2

						color: "black"
						text: root.actionMode.selection_owner
						horizontalAlignment: Text.AlignRight
						width: parent.width * root.ownerFieldWidthRatio
						wrapMode: Text.WordWrap
						font.pointSize: 11
					}

				}

				Item {
					anchors.fill: parent
					id: selection_group_panel
					visible: root.actionMode.selection_size > 1

					Text {
						anchors.top: parent.top
						anchors.topMargin: metricsUnit * 2
						anchors.left: parent.left
						anchors.leftMargin: metricsUnit * 2

						color: "black"
						text: root.actionMode.selection_name
						font.pointSize: 14
						font.bold: true
					}
				}

				Rectangle {
					anchors.left: parent.left
					anchors.right: parent.right
					anchors.bottom: parent.bottom
					anchors.bottomMargin: metricsUnit * 3
					visible: actionMode.ability.length
					height: metricsUnit * 3
					color: "#66000000"
					border.color: "#44FFFFFF"
					border.width: 1

					Text {
						anchors.centerIn: parent

						text: "Ability: " + root.prettifyLabel(actionMode.ability)
						font.pointSize: 12
						color: "white"
					}
				}
			}
		}

		Image {
			id: rightRect

			anchors.right: parent.right
			anchors.top: parent.top
			anchors.bottom: parent.bottom

			width: height * 2.01

			source: hudImageSource + "." + root.rightRectSubid
			fillMode: Image.Stretch

			Rectangle {
				anchors.top: parent.top
				anchors.left: parent.left
				anchors.right: parent.right
				anchors.margins: metricsUnit * 2
				anchors.topMargin: metricsUnit * 2.8
				height: parent.height * 0.58

				color: "#30000000"
				border.color: "#AAFFFFFF"
				border.width: 1

				Rectangle {
					anchors.fill: parent
					anchors.margins: metricsUnit
					color: "#22000000"
					border.color: "#55FFFFFF"
					border.width: 1

					Text {
						anchors.top: parent.top
						anchors.horizontalCenter: parent.horizontalCenter
						anchors.topMargin: metricsUnit * 0.8
						text: "Minimap"
						font.bold: true
						color: "white"
					}

					Text {
						anchors.centerIn: parent
						width: parent.width - metricsUnit * 2
						horizontalAlignment: Text.AlignHCenter
						wrapMode: Text.WordWrap
						color: "#CCFFFFFF"
						text: "Minimap data is built each tick in the engine (fog shading plus unit and building markers). A live texture overlay in this panel is pending QML wiring."
					}
				}
			}
		}
	}

	/*
	 * Metric propagation.
	 */
	FontMetrics {
		id: fontMetrics
	}

	property int metricsUnit: metrics ? metrics.unit : fontMetrics.averageCharacterWidth
}
