// Copyright 2022-2026 the openage authors. See copying.md for legal info.

#include "texture_manager.h"

#include "error/error.h"
#include "log/log.h"
#include "log/message.h"
#include "renderer/renderer.h"
#include "renderer/resources/texture_data.h"


namespace openage::renderer::resources {

TextureManager::TextureManager(const std::shared_ptr<Renderer> &renderer) :
	renderer{renderer},
	loaded{} {
}

const std::shared_ptr<Texture2d> &TextureManager::request(const util::Path &path) {
	if (not this->loaded.contains(path)) {
		try {
			auto tex_data = resources::Texture2dData(path);
			this->loaded.insert({path, this->renderer->add_texture(tex_data)});
		}
		catch (const Error &err) {
			if (this->placeholder) {
				log::log(MSG(warn) << "Failed to load texture image from: " << path
				                   << " - using placeholder instead.");
				this->loaded.insert({path, (*this->placeholder).second});
			}
			else {
				throw;
			}
		}
	}
	return this->loaded.at(path);
}

void TextureManager::add(const util::Path &path) {
	if (not this->loaded.contains(path)) {
		try {
			auto tex_data = resources::Texture2dData(path);
			this->loaded.insert({path, this->renderer->add_texture(tex_data)});
		}
		catch (const Error &err) {
			if (this->placeholder) {
				log::log(MSG(warn) << "Failed to load texture image from: " << path
				                   << " - using placeholder instead.");
				this->loaded.insert({path, (*this->placeholder).second});
			}
			else {
				throw;
			}
		}
	}
}

void TextureManager::add(const util::Path &path,
                         const std::shared_ptr<Texture2d> &texture) {
	this->loaded.insert({path, texture});
}

void TextureManager::remove(const util::Path &path) {
	this->loaded.erase(path);
}

void TextureManager::set_placeholder(const util::Path &path) {
	auto tex_data = resources::Texture2dData(path);
	this->placeholder = std::make_pair(path, this->renderer->add_texture(tex_data));
}

const TextureManager::placeholder_t &TextureManager::get_placeholder() const {
	return this->placeholder;
}

} // namespace openage::renderer::resources
