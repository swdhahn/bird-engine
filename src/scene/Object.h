#ifndef H_BIRD_OBJECT
#define H_BIRD_OBJECT

#include "../util/Memory.hpp"

namespace bird {

/**
 * Discriminator enum identifying concrete Object sub-types in the scene graph.
 */
enum OBJ_TYPES {
	WORLDOBJECT,
	SCREENOBJECT,
	CAMERA,
	LIGHT,
	SCENE,
	TYPE_LAST = SCENE
};

/**
 * Helper macro for concrete subclasses of Object.
 * Injects static type constant and implements the virtual getType() accessor.
 */
#define BIRD_OBJECT(TypeName)                     \
   public:                                        \
	static constexpr OBJ_TYPES s_type = TypeName; \
	virtual OBJ_TYPES getType() const override {  \
		return s_type;                            \
	}

/**
 * Base polymorphic class for all entities participating in scene hierarchies.
 */
class Object {
   public:
	virtual ~Object() = default;

	/** Returns the concrete OBJ_TYPES discriminator for this instance. */
	virtual OBJ_TYPES getType() const = 0;
};

}  // namespace bird

#endif	// H_BIRD_OBJECT
