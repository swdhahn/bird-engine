#include "AnimationSystem.h"

#include <cmath>
#include <stdexcept>
#include <utility>

#include "Tween.h"

namespace bird {

namespace {

template <typename T>
AnimationHandle playTween(AnimationSystem& system, const T& from, const T& to,
						  float duration, Ease ease,
						  std::function<void(T)> setter) {
	if (!setter) {
		throw std::invalid_argument("AnimationSystem: setter must not be empty");
	}
	auto tween = _own_mk<Tween<T>>(
		from, to, duration, ease,
		[setter = std::move(setter)](const T& value) { setter(value); });
	return system.play(std::move(tween));
}

}  // namespace

AnimationSystem::~AnimationSystem() {
	clear();
}

AnimationHandle AnimationSystem::play(_own<AnimationPlayback> playback,
									  const void* owner) {
	if (!playback) {
		throw std::invalid_argument("AnimationSystem::play: playback is null");
	}
	uint32_t index;
	if (!m_freeSlots.empty()) {
		index = m_freeSlots.back();
		m_freeSlots.pop_back();
	} else {
		index = static_cast<uint32_t>(m_slots.size());
		m_slots.emplace_back();
	}
	Slot& slot = m_slots[index];
	slot.playback = std::move(playback);
	slot.owner = owner;
	++m_activeCount;
	return AnimationHandle{index, slot.generation};
}

AnimationSystem::Slot* AnimationSystem::resolve(AnimationHandle handle) {
	if (handle.index >= m_slots.size()) return nullptr;
	Slot& slot = m_slots[handle.index];
	if (slot.generation != handle.generation || !slot.playback) return nullptr;
	return &slot;
}

const AnimationSystem::Slot* AnimationSystem::resolve(
	AnimationHandle handle) const {
	return const_cast<AnimationSystem*>(this)->resolve(handle);
}

void AnimationSystem::release(uint32_t index) {
	Slot& slot = m_slots[index];
	++slot.generation;
	slot.owner = nullptr;
	--m_activeCount;
	if (m_ticking) {
		// The playback may be mid-step(); keep it alive until tick ends.
		m_graveyard.push_back(std::move(slot.playback));
		m_pendingFree.push_back(index);
	} else {
		slot.playback.reset();
		m_freeSlots.push_back(index);
	}
}

bool AnimationSystem::cancel(AnimationHandle handle) {
	if (!resolve(handle)) return false;
	release(handle.index);
	return true;
}

size_t AnimationSystem::cancelOwner(const void* owner) {
	if (!owner) return 0;
	size_t cancelled = 0;
	for (uint32_t i = 0; i < m_slots.size(); ++i) {
		if (m_slots[i].playback && m_slots[i].owner == owner) {
			release(i);
			++cancelled;
		}
	}
	return cancelled;
}

size_t AnimationSystem::cancelOwnersIf(
	const std::function<bool(const void*)>& predicate) {
	if (!predicate) return 0;
	size_t cancelled = 0;
	for (uint32_t i = 0; i < m_slots.size(); ++i) {
		const Slot& slot = m_slots[i];
		if (slot.playback && slot.owner && predicate(slot.owner)) {
			release(i);
			++cancelled;
		}
	}
	return cancelled;
}

void AnimationSystem::clear() {
	for (uint32_t i = 0; i < m_slots.size(); ++i) {
		if (m_slots[i].playback) release(i);
	}
}

bool AnimationSystem::isActive(AnimationHandle handle) const {
	return resolve(handle) != nullptr;
}

AnimationPlayback* AnimationSystem::get(AnimationHandle handle) {
	Slot* slot = resolve(handle);
	return slot ? slot->playback.get() : nullptr;
}

bool AnimationSystem::pause(AnimationHandle handle) {
	AnimationPlayback* playback = get(handle);
	if (!playback) return false;
	playback->clock().pause();
	return true;
}

bool AnimationSystem::resume(AnimationHandle handle) {
	AnimationPlayback* playback = get(handle);
	if (!playback) return false;
	playback->clock().resume();
	return true;
}

bool AnimationSystem::setSpeed(AnimationHandle handle, float speed) {
	AnimationPlayback* playback = get(handle);
	if (!playback) return false;
	playback->clock().setSpeed(speed);
	return true;
}

void AnimationSystem::tick(float delta) {
	if (!std::isfinite(delta) || delta < 0.0f) {
		throw std::invalid_argument(
			"AnimationSystem::tick: delta must be finite and >= 0");
	}
	if (m_ticking) {
		throw std::logic_error("AnimationSystem::tick: re-entrant tick");
	}
	m_ticking = true;
	// Restores the idle state even when a callback throws.
	struct TickScope {
		AnimationSystem& system;
		~TickScope() {
			system.m_ticking = false;
			system.m_graveyard.clear();
			system.m_freeSlots.insert(system.m_freeSlots.end(),
									  system.m_pendingFree.begin(),
									  system.m_pendingFree.end());
			system.m_pendingFree.clear();
		}
	} scope{*this};
	// Snapshot the slot count: animations started by callbacks this tick
	// begin on the next one.
	const size_t count = m_slots.size();
	for (uint32_t i = 0; i < count; ++i) {
		// Index each time: callbacks may grow (reallocate) m_slots.
		AnimationPlayback* playback = m_slots[i].playback.get();
		if (!playback) continue;
		const uint32_t generation = m_slots[i].generation;
		const bool running = playback->step(delta);
		// The step's callbacks may already have cancelled this slot.
		if (!running && m_slots[i].generation == generation &&
			m_slots[i].playback) {
			release(i);
		}
	}
}

// ------------------------------------------------------------------ tweens

AnimationHandle AnimationSystem::tweenFloat(float from, float to,
											float duration, Ease ease,
											std::function<void(float)> setter) {
	return playTween<float>(*this, from, to, duration, ease, std::move(setter));
}

AnimationHandle AnimationSystem::tweenVector2(
	Vector2 from, Vector2 to, float duration, Ease ease,
	std::function<void(Vector2)> setter) {
	return playTween<Vector2>(*this, from, to, duration, ease,
							  std::move(setter));
}

AnimationHandle AnimationSystem::tweenVector3(
	Vector3 from, Vector3 to, float duration, Ease ease,
	std::function<void(Vector3)> setter) {
	return playTween<Vector3>(*this, from, to, duration, ease,
							  std::move(setter));
}

AnimationHandle AnimationSystem::tweenVector4(
	Vector4 from, Vector4 to, float duration, Ease ease,
	std::function<void(Vector4)> setter) {
	return playTween<Vector4>(*this, from, to, duration, ease,
							  std::move(setter));
}

AnimationHandle AnimationSystem::tweenQuaternion(
	Quaternion from, Quaternion to, float duration, Ease ease,
	std::function<void(Quaternion)> setter) {
	return playTween<Quaternion>(*this, from, to, duration, ease,
								 std::move(setter));
}

}  // namespace bird
