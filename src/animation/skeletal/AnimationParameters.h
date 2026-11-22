#ifndef BIRD_ANIMATION_ANIMATIONPARAMETERS_H
#define BIRD_ANIMATION_ANIMATIONPARAMETERS_H

#include <cstdint>
#include <string>
#include <unordered_map>

#include "../../util/Constants.h"

namespace bird {

/**
 * Value kind stored in an AnimationParameters blackboard.
 */
enum class AnimationParameterType : uint8_t {
	/** Continuous floating-point value (e.g. locomotion speed, aim pitch). */
	Float = 0,
	/** Boolean flag (e.g. isGrounded, isCrouching). */
	Bool = 1,
	/** Signed 32-bit integer (e.g. weaponType, comboIndex). */
	Int = 2,
	/** One-shot boolean pulse automatically consumed by a state transition. */
	Trigger = 3
};

/**
 * Per-entity parameter blackboard read by AnimationStateMachine transitions
 * and BlendTree nodes each frame.
 *
 * Gameplay C++ or Dart scripts write parameters every frame (e.g. speed,
 * moveX, moveY, isGrounded, jump trigger) without coupling directly to
 * animation clip names or state machine internals.
 *
 * @code
 * auto& params = animComponent->getParameters();
 * params.setFloat("speed", velocity.length());
 * params.setBool("grounded", controller->isOnGround());
 * if (attackPressed) params.setTrigger("attack");
 * @endcode
 */
class AnimationParameters {
   public:
	/**
	 * Sets (or creates) a float parameter.
	 * @param name Unique parameter name.
	 * @param value New floating-point value.
	 */
	void setFloat(const std::string& name, float value);

	/**
	 * Reads a float parameter.
	 * @param name Parameter name.
	 * @param defaultValue Value returned when absent or of a different type.
	 * @return The stored float value, or defaultValue.
	 */
	float getFloat(const std::string& name, float defaultValue = 0.0f) const;

	/**
	 * Sets (or creates) a boolean parameter.
	 * @param name Unique parameter name.
	 * @param value New boolean value.
	 */
	void setBool(const std::string& name, bool value);

	/**
	 * Reads a boolean parameter.
	 * @param name Parameter name.
	 * @param defaultValue Value returned when absent or of a different type.
	 * @return The stored bool value, or defaultValue.
	 */
	bool getBool(const std::string& name, bool defaultValue = false) const;

	/**
	 * Sets (or creates) an integer parameter.
	 * @param name Unique parameter name.
	 * @param value New 32-bit integer value.
	 */
	void setInt(const std::string& name, int32_t value);

	/**
	 * Reads an integer parameter.
	 * @param name Parameter name.
	 * @param defaultValue Value returned when absent or of a different type.
	 * @return The stored int32_t value, or defaultValue.
	 */
	int32_t getInt(const std::string& name, int32_t defaultValue = 0) const;

	/**
	 * Arms a one-shot trigger parameter. Remains armed until consumed by a
	 * matching state transition or cleared via resetTrigger().
	 * @param name Unique trigger name.
	 */
	void setTrigger(const std::string& name);

	/**
	 * Clears an armed trigger without firing any transition.
	 * @param name Trigger name.
	 */
	void resetTrigger(const std::string& name);

	/**
	 * Checks whether a trigger is currently armed without consuming it.
	 * @param name Trigger name.
	 * @return true when the trigger exists and is armed.
	 */
	bool isTriggerSet(const std::string& name) const;

	/**
	 * Checks whether a trigger is armed and atomically resets it to false.
	 * Called by AnimationStateMachine when a transition using the trigger fires.
	 * @param name Trigger name.
	 * @return true if the trigger was armed before this call.
	 */
	bool consumeTrigger(const std::string& name);

	/** @return true when a parameter with the given name exists. */
	bool hasParameter(const std::string& name) const;

	/**
	 * Queries the stored type of a parameter.
	 * @param name Parameter name.
	 * @param outType Populated with the parameter's type when found.
	 * @return true if the parameter exists.
	 */
	BIRD_NO_EXPORT bool tryGetType(const std::string& name,
								   AnimationParameterType& outType) const;

	/** Removes all parameters from the blackboard. */
	void clear();

   private:
	struct Entry {
		AnimationParameterType type = AnimationParameterType::Float;
		float floatValue = 0.0f;
		int32_t intValue = 0;
		bool boolValue = false;
	};

	std::unordered_map<std::string, Entry> m_entries;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_ANIMATIONPARAMETERS_H
