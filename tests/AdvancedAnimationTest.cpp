#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "animation/skeletal/AnimationNotify.h"
#include "animation/skeletal/AnimationParameters.h"
#include "animation/skeletal/BoneMask.h"
#include "animation/skeletal/PoseOps.h"
#include "animation/skeletal/SkeletalClip.h"
#include "animation/skeletal/Skeleton.h"

using namespace bird;

namespace {

constexpr float kEps = 1e-4f;

JointTransform makeTRS(Vector3 t,
					   Quaternion r = Quaternion(1.0f, 0.0f, 0.0f, 0.0f),
					   Vector3 s = Vector3(1.0f)) {
	JointTransform jt;
	jt.translation = t;
	jt.rotation = r;
	jt.scale = s;
	return jt;
}

/**
 * 5-joint hierarchy:
 *   0: pelvis (root)
 *   1: leg    (child of 0)
 *   2: spine  (child of 0)
 *   3: arm    (child of 2)
 *   4: hand   (child of 3)
 */
std::shared_ptr<Skeleton> makeHumanoidLite() {
	auto skel = std::make_shared<Skeleton>();
	skel->addJoint("pelvis", -1, Matrix4(1.0f), makeTRS(Vector3(0, 1, 0)));
	skel->addJoint("leg", 0, Matrix4(1.0f), makeTRS(Vector3(0, -1, 0)));
	skel->addJoint("spine", 0, Matrix4(1.0f), makeTRS(Vector3(0, 0.5f, 0)));
	skel->addJoint("arm", 2, Matrix4(1.0f), makeTRS(Vector3(0.5f, 0.5f, 0)));
	skel->addJoint("hand", 3, Matrix4(1.0f), makeTRS(Vector3(0.5f, 0.0f, 0)));
	return skel;
}

}  // namespace

// ============================================================================
// AnimationParameters Tests
// ============================================================================

TEST(AnimationParameters, StoresAndRetrievesAllTypes) {
	AnimationParameters params;
	params.setFloat("speed", 4.5f);
	params.setBool("grounded", true);
	params.setInt("weapon", 2);
	params.setTrigger("jump");

	EXPECT_FLOAT_EQ(params.getFloat("speed"), 4.5f);
	EXPECT_FLOAT_EQ(params.getFloat("missing", -1.0f), -1.0f);
	EXPECT_TRUE(params.getBool("grounded"));
	EXPECT_FALSE(params.getBool("missing"));
	EXPECT_EQ(params.getInt("weapon"), 2);
	EXPECT_EQ(params.getInt("missing", 99), 99);
	EXPECT_TRUE(params.isTriggerSet("jump"));
	EXPECT_TRUE(params.consumeTrigger("jump"));
	EXPECT_FALSE(params.isTriggerSet("jump"));
	EXPECT_FALSE(params.consumeTrigger("jump"));

	params.setTrigger("attack");
	params.resetTrigger("attack");
	EXPECT_FALSE(params.isTriggerSet("attack"));
	EXPECT_TRUE(params.hasParameter("speed"));
	params.clear();
	EXPECT_FALSE(params.hasParameter("speed"));
}

// ============================================================================
// BoneMask & Masked / Additive Pose Blending Tests
// ============================================================================

TEST(BoneMask, FromBranchSelectsDescendantsOnly) {
	auto skel = makeHumanoidLite();
	BoneMask upper = BoneMask::fromBranch(*skel, "spine", 1.0f);
	ASSERT_EQ(upper.getJointCount(), 5);
	EXPECT_FLOAT_EQ(upper.getJointWeight(0), 0.0f);	 // pelvis
	EXPECT_FLOAT_EQ(upper.getJointWeight(1), 0.0f);	 // leg
	EXPECT_FLOAT_EQ(upper.getJointWeight(2), 1.0f);	 // spine
	EXPECT_FLOAT_EQ(upper.getJointWeight(3), 1.0f);	 // arm
	EXPECT_FLOAT_EQ(upper.getJointWeight(4), 1.0f);	 // hand

	upper.setJointWeight(2, 0.5f);
	EXPECT_FLOAT_EQ(upper.getJointWeight(2), 0.5f);
	EXPECT_THROW(BoneMask::fromBranch(*skel, "nonexistent"),
				 std::invalid_argument);
}

TEST(PoseOpsAdvanced, BlendPosesNlerpTakesShortestPath) {
	Pose a, b, out;
	a.joints.push_back(makeTRS(Vector3(0, 0, 0), Quaternion(1, 0, 0, 0)));
	// Antipodal quaternion (-q represents same rotation as +q, dot < 0).
	Quaternion q90 = glm::angleAxis(glm::radians(90.0f), Vector3(0, 1, 0));
	b.joints.push_back(makeTRS(Vector3(2, 0, 0), -q90, Vector3(3, 3, 3)));

	blendPosesNlerp(a, b, 0.5f, out);
	EXPECT_NEAR(out.joints[0].translation.x, 1.0f, kEps);
	EXPECT_NEAR(out.joints[0].scale.x, 2.0f, kEps);
	// Shortest path between 0 deg and 90 deg is 45 deg (not 135 deg).
	const Quaternion expected =
		glm::angleAxis(glm::radians(45.0f), Vector3(0, 1, 0));
	EXPECT_NEAR(std::fabs(glm::dot(out.joints[0].rotation, expected)), 1.0f,
				1e-3f);
}

TEST(PoseOpsAdvanced, BlendPosesMaskedIsolatesUpperBody) {
	auto skel = makeHumanoidLite();
	Pose base = skel->getRestPose();
	Pose overlay = skel->getRestPose();
	overlay.joints[1].translation = Vector3(10, 0, 0);	// leg
	overlay.joints[3].translation = Vector3(0, 5, 0);	// arm

	BoneMask upper = BoneMask::fromBranch(*skel, "spine", 1.0f);
	Pose out;
	blendPosesMasked(base, overlay, upper, 1.0f, out);

	// Leg untouched (mask 0), arm fully overridden (mask 1).
	EXPECT_NEAR(out.joints[1].translation.x, 0.0f, kEps);
	EXPECT_NEAR(out.joints[3].translation.y, 5.0f, kEps);
}

TEST(PoseOpsAdvanced, AdditivePoseAppliesDeltaOverBase) {
	Pose ref, target, base, delta, out;
	ref.joints.push_back(
		makeTRS(Vector3(1, 0, 0), Quaternion(1, 0, 0, 0), Vector3(1, 1, 1)));
	target.joints.push_back(makeTRS(
		Vector3(1, 2, 0),
		glm::angleAxis(glm::radians(30.0f), Vector3(0, 0, 1)),
		Vector3(2, 1, 1)));
	base.joints.push_back(makeTRS(
		Vector3(5, 1, 0),
		glm::angleAxis(glm::radians(60.0f), Vector3(0, 0, 1)),
		Vector3(1, 1, 1)));

	makeAdditivePose(target, ref, delta);
	applyAdditivePose(base, delta, 1.0f, out);

	EXPECT_NEAR(out.joints[0].translation.x, 5.0f, kEps);
	EXPECT_NEAR(out.joints[0].translation.y, 3.0f, kEps);
	EXPECT_NEAR(out.joints[0].scale.x, 2.0f, kEps);
	const Quaternion expected =
		glm::angleAxis(glm::radians(90.0f), Vector3(0, 0, 1));
	EXPECT_NEAR(std::fabs(glm::dot(out.joints[0].rotation, expected)), 1.0f,
				1e-3f);
}

// ============================================================================
// AnimationNotify Tests
// ============================================================================

TEST(SkeletalClipNotifies, CollectsForwardAndWrapAround) {
	SkeletalClip clip("Run", 1.0f);
	clip.addNotify(0.0f, "Start");
	clip.addNotify(0.25f, "FootstepL", "grass");
	clip.addNotify(0.75f, "FootstepR", "grass");
	clip.addNotify(0.95f, "AlmostEnd");

	std::vector<const AnimationNotify*> hits;
	// First tick [0, 0.3] includes t == 0.
	clip.collectNotifies(0.0f, 0.3f, false, false, true, hits);
	ASSERT_EQ(hits.size(), 2u);
	EXPECT_EQ(hits[0]->name, "Start");
	EXPECT_EQ(hits[1]->name, "FootstepL");

	// Normal step (0.3, 0.8] hits FootstepR only.
	hits.clear();
	clip.collectNotifies(0.3f, 0.8f, false, false, false, hits);
	ASSERT_EQ(hits.size(), 1u);
	EXPECT_EQ(hits[0]->name, "FootstepR");

	// Wrap-around (0.8, 1.0] + [0.0, 0.26] hits AlmostEnd, Start, FootstepL.
	hits.clear();
	clip.collectNotifies(0.8f, 0.26f, true, false, false, hits);
	ASSERT_EQ(hits.size(), 3u);
	EXPECT_EQ(hits[0]->name, "AlmostEnd");
	EXPECT_EQ(hits[1]->name, "Start");
	EXPECT_EQ(hits[2]->name, "FootstepL");

	EXPECT_THROW(clip.addNotify(-0.1f, "Bad"), std::invalid_argument);
	EXPECT_THROW(clip.addNotify(1.5f, "Bad"), std::invalid_argument);
	EXPECT_THROW(clip.addNotify(0.5f, ""), std::invalid_argument);
}
