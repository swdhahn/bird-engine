#ifndef BIRD_ANIMATION_BLENDTREE_H
#define BIRD_ANIMATION_BLENDTREE_H

#include <memory>
#include <string>
#include <vector>

#include "../../util/Constants.h"
#include "../../util/Math.h"
#include "PoseNode.h"
#include "SkeletalClip.h"

namespace bird {

/**
 * 1D Blend Tree PoseNode: blends between sorted clips along a single float
 * parameter (e.g. Idle at 0.0 -> Walk at 2.0 -> Run at 6.0 driven by "speed").
 *
 * When time synchronization is enabled (the default), all clips advance on a
 * shared normalized phase computed from the weighted duration of the two active
 * clips so foot cycles stay in phase across speed changes.
 */
class BIRD_NO_EXPORT BlendTree1D : public PoseNode {
   public:
	/** One clip positioned at a scalar threshold along the 1D axis. */
	struct Entry {
		std::shared_ptr<const SkeletalClip> clip;
		float threshold = 0.0f;
	};

	/**
	 * @param parameterName Float parameter name read from PoseEvalContext::parameters.
	 * @param syncNormalizedTime When true, clips share a synchronized [0, 1] phase.
	 */
	explicit BlendTree1D(std::string parameterName,
						 bool syncNormalizedTime = true);

	/**
	 * Adds a clip at the given threshold; entries are kept sorted ascending.
	 * @param clip Non-null SkeletalClip.
	 * @param threshold Scalar position along the blend axis.
	 * @throws std::invalid_argument when clip is null or threshold is not finite.
	 */
	void addClip(std::shared_ptr<const SkeletalClip> clip, float threshold);

	/** Sets a fallback parameter value used when no blackboard is provided. */
	void setParameterValue(float value) { m_manualValue = value; }
	float getParameterValue() const { return m_lastParameterValue; }
	const std::string& getParameterName() const { return m_parameterName; }
	size_t getClipCount() const { return m_entries.size(); }

	void evaluate(const PoseEvalContext& ctx, Pose& outPose) override;
	void reset() override;
	float getNormalizedTime() const override { return m_normalizedTime; }
	float getDuration() const override { return m_effectiveDuration; }
	bool isFinished() const override { return m_finished; }
	void setSpeed(float speed) override { m_speed = speed; }
	void setLoopMode(LoopMode loop) override { m_loopMode = loop; }

   private:
	struct RuntimeEntry {
		std::shared_ptr<const SkeletalClip> clip;
		float threshold = 0.0f;
		PlaybackClock clock;
		ClipCursor cursor;
	};

	std::string m_parameterName;
	bool m_syncNormalizedTime = true;
	float m_manualValue = 0.0f;
	float m_lastParameterValue = 0.0f;
	float m_speed = 1.0f;
	LoopMode m_loopMode = LoopMode::Loop;
	float m_normalizedTime = 0.0f;
	float m_effectiveDuration = 0.0f;
	bool m_finished = false;
	bool m_hasTicked = false;

	std::vector<RuntimeEntry> m_entries;
	Pose m_poseA;
	Pose m_poseB;
	std::vector<const AnimationNotify*> m_notifyScratch;
};

/**
 * Weighting algorithm used by BlendTree2D to compute per-clip blend weights
 * from a 2D sample coordinate.
 */
enum class BlendTree2DMode : uint8_t {
	/**
	 * Inverse-Distance Weighting (Shepard's method): smooth C1 blending across
	 * arbitrary point layouts with exact interpolation at sample points.
	 */
	InverseDistance = 0,
	/**
	 * Barycentric triangulation: locates the containing triangle of sample
	 * points and blends the three vertices with exact barycentric coordinates
	 * (clamping to the nearest edge/vertex outside the convex hull).
	 */
	Barycentric = 1
};

/**
 * 2D Blend Tree PoseNode: blends directional clips arranged on a 2D parameter
 * plane (e.g. Idle (0,0), Forward (0,1), Back (0,-1), StrafeLeft (-1,0),
 * StrafeRight (1,0) driven by "moveX" and "moveY").
 *
 * Supports both Inverse-Distance Weighting and Barycentric weighting, optional
 * normalized phase synchronization, and zero per-frame allocations once warm.
 */
class BIRD_NO_EXPORT BlendTree2D : public PoseNode {
   public:
	/** One clip placed at a 2D coordinate on the blend plane. */
	struct Entry {
		std::shared_ptr<const SkeletalClip> clip;
		Vector2 position{0.0f};
	};

	/**
	 * @param paramX Float parameter name for the horizontal axis.
	 * @param paramY Float parameter name for the vertical axis.
	 * @param mode Weight computation mode (InverseDistance or Barycentric).
	 * @param syncNormalizedTime When true, clips share a synchronized [0, 1] phase.
	 */
	BlendTree2D(std::string paramX, std::string paramY,
				BlendTree2DMode mode = BlendTree2DMode::InverseDistance,
				bool syncNormalizedTime = true);

	/**
	 * Adds a clip at a 2D coordinate on the blend plane.
	 * @param clip Non-null SkeletalClip.
	 * @param position 2D coordinate (x, y); components must be finite.
	 * @throws std::invalid_argument when clip is null or position is non-finite.
	 */
	void addClip(std::shared_ptr<const SkeletalClip> clip,
				 const Vector2& position);

	/**
	 * Sets the exponent used by InverseDistance weighting (default 2.0).
	 * Higher powers sharpen influence around each sample point.
	 * @throws std::invalid_argument when power <= 0 or not finite.
	 */
	void setIdwPower(float power);
	float getIdwPower() const { return m_idwPower; }

	void setMode(BlendTree2DMode mode) { m_mode = mode; }
	BlendTree2DMode getMode() const { return m_mode; }

	/** Sets fallback parameter values used when no blackboard is provided. */
	void setParameterValues(const Vector2& value) { m_manualValue = value; }
	Vector2 getParameterValues() const { return m_lastParameterValue; }
	const std::string& getParamX() const { return m_paramX; }
	const std::string& getParamY() const { return m_paramY; }
	size_t getClipCount() const { return m_entries.size(); }

	/**
	 * Computes normalized blend weights for every entry at the given 2D point.
	 * Exposed for inspection, debugging, and unit testing.
	 * @param point 2D parameter coordinate.
	 * @param outWeights Resized to getClipCount(); sums to 1.0 when non-empty.
	 */
	void computeWeights(const Vector2& point,
						std::vector<float>& outWeights) const;

	void evaluate(const PoseEvalContext& ctx, Pose& outPose) override;
	void reset() override;
	float getNormalizedTime() const override { return m_normalizedTime; }
	float getDuration() const override { return m_effectiveDuration; }
	bool isFinished() const override { return m_finished; }
	void setSpeed(float speed) override { m_speed = speed; }
	void setLoopMode(LoopMode loop) override { m_loopMode = loop; }

   private:
	struct RuntimeEntry {
		std::shared_ptr<const SkeletalClip> clip;
		Vector2 position{0.0f};
		PlaybackClock clock;
		ClipCursor cursor;
	};

	void computeInverseDistanceWeights(const Vector2& point,
									   std::vector<float>& outWeights) const;
	void computeBarycentricWeights(const Vector2& point,
								   std::vector<float>& outWeights) const;

	std::string m_paramX;
	std::string m_paramY;
	BlendTree2DMode m_mode = BlendTree2DMode::InverseDistance;
	bool m_syncNormalizedTime = true;
	float m_idwPower = 2.0f;
	Vector2 m_manualValue{0.0f};
	Vector2 m_lastParameterValue{0.0f};
	float m_speed = 1.0f;
	LoopMode m_loopMode = LoopMode::Loop;
	float m_normalizedTime = 0.0f;
	float m_effectiveDuration = 0.0f;
	bool m_finished = false;
	bool m_hasTicked = false;

	std::vector<RuntimeEntry> m_entries;
	std::vector<float> m_weights;
	std::vector<Pose> m_scratchPoses;
	std::vector<const Pose*> m_activePosePtrs;
	std::vector<float> m_activeWeights;
	std::vector<const AnimationNotify*> m_notifyScratch;
};

}  // namespace bird

#endif	// BIRD_ANIMATION_BLENDTREE_H
