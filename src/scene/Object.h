#ifndef H_BIRD_OBJECT
#define H_BIRD_OBJECT

#include "../util/Memory.hpp"

namespace bird {

enum OBJ_TYPES {
	WORLDOBJECT,
	SCREENOBJECT,
	CAMERA,
	LIGHT,
	SCENE,
	TYPE_LAST = SCENE
};
#define BIRD_OBJECT(TypeName)                     \
   public:                                        \
	static constexpr OBJ_TYPES s_type = TypeName; \
	virtual OBJ_TYPES getType() const override {  \
		return s_type;                            \
	}

class Object {
   public:
	virtual ~Object() = default;
	virtual OBJ_TYPES getType() const = 0;
};
}  // namespace bird

#endif	// H_BIRD_OBJECT
