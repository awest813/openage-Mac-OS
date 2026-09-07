#include "api_component.h"

#include "error/error.h"
#include "log/message.h"


namespace openage::gamestate::component {

APIComponent::APIComponent(const std::shared_ptr<event::EventLoop> &loop,
                           nyan::Object &ability,
                           const time::time_t &creation_time,
                           const bool enabled) :
	ability{ability},
	enabled(loop, 0) {
	this->enabled.set_insert(creation_time, enabled);
}

APIComponent::APIComponent(const std::shared_ptr<event::EventLoop> &loop,
                           nyan::Object &ability,
                           bool enabled) :
	ability{ability},
	enabled(loop, 0, "", nullptr, enabled) {
}

APIComponent::APIComponent(const std::shared_ptr<event::EventLoop> &loop,
                           bool enabled) :
	ability{std::nullopt},
	enabled(loop, 0, "", nullptr, enabled) {
}

const nyan::Object &APIComponent::get_ability() const {
	if (not this->ability.has_value()) {
		throw Error{ERR << "APIComponent has no nyan ability object."};
	}
	return this->ability.value();
}

} // namespace openage::gamestate::component
