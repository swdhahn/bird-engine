#ifndef BIRD_CONTACTEVENT_H
#define BIRD_CONTACTEVENT_H

#include <cstdint>

#include "../util/Math.h"

namespace bird {

class RigidBodyComponent;
class CharacterControllerComponent;
class WorldObject;

/** Which notification a ContactEvent is. */
enum class ContactEventKind : uint8_t { ContactBegin, ContactEnd, TriggerEnter, TriggerExit };

/**
 * A contact or trigger notification, always delivered on the main thread
 * after the physics step that produced it.
 *
 * Begin events fire once when two bodies start touching (however many
 * sub-shapes touch) and end events once when the last touching pair
 * separates or either body is destroyed. A pair involving a sensor body
 * produces trigger events instead of contact events. Character controllers
 * take part too: a character touching a body (or another character), or
 * entering a sensor, notifies both sides.
 */
struct ContactEvent {
	/** The rigid body receiving this event (nullptr when it is a character). */
	RigidBodyComponent* self = nullptr;
	/** The other rigid body (nullptr when the other side is a character). */
	RigidBodyComponent* other = nullptr;
	/** The character receiving this event, when self is not a rigid body. */
	CharacterControllerComponent* selfCharacter = nullptr;
	/** The other character, when other is not a rigid body. */
	CharacterControllerComponent* otherCharacter = nullptr;
	/** Objects of both sides (always set). */
	WorldObject* selfObject = nullptr;
	WorldObject* otherObject = nullptr;
	/** World-space contact point (begin events only; zero for end events). */
	Vector3 point{0.0f};
	/**
	 * World-space contact normal pointing from self towards other (begin
	 * events only; zero for end events).
	 */
	Vector3 normal{0.0f};
};

}  // namespace bird

#endif	// !BIRD_CONTACTEVENT_H
