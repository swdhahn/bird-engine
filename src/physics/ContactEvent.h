#ifndef BIRD_CONTACTEVENT_H
#define BIRD_CONTACTEVENT_H

#include "../util/Math.h"

namespace bird {

class RigidBodyComponent;

/**
 * A contact or trigger notification, always delivered on the main thread
 * after the physics step that produced it.
 *
 * Begin events fire once when two bodies start touching (however many
 * sub-shapes touch) and end events once when the last touching pair
 * separates or either body is destroyed. A pair involving a sensor body
 * produces trigger events instead of contact events.
 */
struct ContactEvent {
	/** The body receiving this event. */
	RigidBodyComponent* self = nullptr;
	/** The other body of the pair. */
	RigidBodyComponent* other = nullptr;
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
