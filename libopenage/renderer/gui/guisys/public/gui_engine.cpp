// Copyright 2015-2026 the openage authors. See copying.md for legal info.

#include "renderer/gui/guisys/public/gui_engine.h"

#include <QQmlContext>
#include <QQmlEngine>
#include <QString>

#include "renderer/gui/guisys/private/gui_engine_impl.h"

namespace qtgui {

GuiQmlEngine::GuiQmlEngine(std::shared_ptr<GuiRenderer> renderer) :
	impl{std::make_unique<GuiQmlEngineImpl>(renderer)} {
}

GuiQmlEngine::~GuiQmlEngine() = default;

void GuiQmlEngine::set_context_property(const std::string &name, QObject *object) {
	auto engine = this->impl->get_qml_engine();
	engine->rootContext()->setContextProperty(QString::fromStdString(name), object);
}

} // namespace qtgui
