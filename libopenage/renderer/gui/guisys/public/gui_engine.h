// Copyright 2015-2026 the openage authors. See copying.md for legal info.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <QObject>

QT_FORWARD_DECLARE_CLASS(QObject)

// QT_FORWARD_DECLARE_CLASS(QQuickWindow)

namespace qtgui {

class GuiQmlEngineImpl;
class GuiRenderer;

/**
 * Represents one QML execution environment.
 */
class GuiQmlEngine {
public:
	explicit GuiQmlEngine(std::shared_ptr<GuiRenderer> renderer);
	~GuiQmlEngine();

	/**
	 * Expose a C++ object to QML as a named context property.
	 *
	 * Must be called before the root QML component is created.
	 *
	 * @param name Context property name (e.g. "menuController").
	 * @param object Object ownership remains with the caller.
	 */
	void set_context_property(const std::string &name, QObject *object);

private:
	friend class GuiQmlEngineImpl;
	std::unique_ptr<GuiQmlEngineImpl> impl;
};

} // namespace qtgui
