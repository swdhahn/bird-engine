#ifndef BIRD_AUDIO_AUDIOINTERNAL_H
#define BIRD_AUDIO_AUDIOINTERNAL_H

// Private implementation header for src/audio/.
// Never include this header from public engine headers or other modules.

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "AudioSystem.h"
#include "third_party/miniaudio.h"

namespace bird {

struct AudioBusNode {
	std::string name;
	std::string parentName;	 // Empty only for Master
	ma_sound_group group{};
	bool initialized = false;
	float volume = 1.0f;
	float pitch = 1.0f;
	bool muted = false;
	bool paused = false;
};

struct SoundSlot {
	// Heap-allocated so moving/resizing the slot vector never invalidates
	// ma_sound's internal node-graph pointers.
	std::unique_ptr<ma_sound> sound;
	std::string busName;
	uint32_t generation = 0;
	float volume = 1.0f;
	float pitch = 1.0f;
	bool looping = false;
	bool spatialized = false;
	bool streaming = false;
	bool paused = false;
	bool oneShot = false;
	bool stoppingWithFade = false;
	bool active = false;
};

struct AudioSystem::Impl {
	ma_engine engine{};
	bool initialized = false;
	bool noDevice = false;

	// Buses stored via unique_ptr so ma_sound_group addresses stay stable
	// across insertions in the node graph.
	std::unordered_map<std::string, std::unique_ptr<AudioBusNode>> buses;
	// Creation order ensures children are always torn down before parents.
	std::vector<std::string> busOrder;

	// Generational slot map for active sounds.
	std::vector<SoundSlot> slots;
	std::vector<uint32_t> freeSlots;
	size_t activeCount = 0;

	// Cached listener state for getters.
	Vector3 listenerPos{0.0f, 0.0f, 0.0f};
	Vector3 listenerForward{0.0f, 0.0f, -1.0f};
	Vector3 listenerUp{0.0f, 1.0f, 0.0f};
	Vector3 listenerVelocity{0.0f, 0.0f, 0.0f};

	// Registered scene components synced every frame during tick().
	std::vector<AudioSourceComponent*> sources;
	std::vector<AudioListenerComponent*> listeners;
	AudioListenerComponent* activeListener = nullptr;

	SoundSlot* resolveSlot(AudioHandle handle) {
		if (!initialized || handle.isNull()) return nullptr;
		if (handle.index >= slots.size()) return nullptr;
		SoundSlot& slot = slots[handle.index];
		if (!slot.active || slot.generation != handle.generation)
			return nullptr;
		return &slot;
	}

	const SoundSlot* resolveSlot(AudioHandle handle) const {
		if (!initialized || handle.isNull()) return nullptr;
		if (handle.index >= slots.size()) return nullptr;
		const SoundSlot& slot = slots[handle.index];
		if (!slot.active || slot.generation != handle.generation)
			return nullptr;
		return &slot;
	}
};

}  // namespace bird

#endif	// BIRD_AUDIO_AUDIOINTERNAL_H
