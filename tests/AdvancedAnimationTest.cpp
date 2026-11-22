#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "animation/skeletal/AnimationLayer.h"
#include "animation/skeletal/AnimationNotify.h"
#include "animation/skeletal/AnimationParameters.h"
#include "animation/skeletal/AnimationStateMachine.h"
#include "animation/skeletal/BlendTree.h"
#include "animation/skeletal/BoneMask.h"
#include "animation/skeletal/PoseNode.h"
#include "animation/skeletal/PoseOps.h"
#include "animation/skeletal/RootMotion.h"
#include "animation/skeletal/SkeletalClip.h"
#include "animation/skeletal/Skeleton.h"
#include "animation/skeletal/TwoBoneIK.h"

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

// ============================================================================
// PoseNode & BlendTree1D / BlendTree2D Tests
// ============================================================================

namespace {

/** Creates a constant-translation clip on joint 0 for predictable blend tests. */
std::shared_ptr<SkeletalClip> makeConstClip(const std::string& name,
											Vector3 pos, float duration = 1.0f) {
	auto clip = std::make_shared<SkeletalClip>(name, duration);
	JointTrack tr;
	tr.joint = 0;
	tr.translation.addKey(0.0f, pos);
	tr.translation.addKey(duration, pos);
	clip->addTrack(std::move(tr));
	return clip;
}

}  // namespace

TEST(PoseNodeTests, ClipPoseNodeSamplesAndDispatchesNotifiesOnLoop) {
	auto skel = makeHumanoidLite();
	auto clip = makeConstClip("Walk", Vector3(1, 2, 3), 1.0f);
	clip->addNotify(0.5f, "Step");

	ClipPoseNode node(clip, LoopMode::Loop);
	std::vector<AnimationNotifyEvent> notifies;
	PoseEvalContext ctx{skel.get(), nullptr, &notifies, 0.6f};
	Pose out;

	node.evaluate(ctx, out);
	EXPECT_NEAR(out.joints[0].translation.x, 1.0f, kEps);
	ASSERT_EQ(notifies.size(), 1u);
	EXPECT_EQ(notifies[0].eventName, "Step");
	EXPECT_EQ(notifies[0].clipName, "Walk");

	// Advance another 1.0s -> wraps and crosses 0.5 again.
	notifies.clear();
	ctx.deltaTime = 1.0f;
	node.evaluate(ctx, out);
	ASSERT_EQ(notifies.size(), 1u);
	EXPECT_EQ(notifies[0].eventName, "Step");
}

TEST(BlendTreeTests, BlendTree1DBlendsSortedClipsByParameter) {
	auto skel = makeHumanoidLite();
	BlendTree1D tree("speed");
	// Intentionally add out of order to verify sorting.
	tree.addClip(makeConstClip("Run", Vector3(6, 0, 0)), 6.0f);
	tree.addClip(makeConstClip("Idle", Vector3(0, 0, 0)), 0.0f);
	tree.addClip(makeConstClip("Walk", Vector3(2, 0, 0)), 2.0f);

	AnimationParameters params;
	PoseEvalContext ctx{skel.get(), &params, nullptr, 0.1f};
	Pose out;

	// Below minimum clamps to Idle.
	params.setFloat("speed", -1.0f);
	tree.evaluate(ctx, out);
	EXPECT_NEAR(out.joints[0].translation.x, 0.0f, kEps);

	// Halfway between Idle (0) and Walk (2) -> x = 1.
	params.setFloat("speed", 1.0f);
	tree.evaluate(ctx, out);
	EXPECT_NEAR(out.joints[0].translation.x, 1.0f, kEps);

	// Halfway between Walk (2) and Run (6) -> speed = 4 -> x = 4.
	params.setFloat("speed", 4.0f);
	tree.evaluate(ctx, out);
	EXPECT_NEAR(out.joints[0].translation.x, 4.0f, kEps);

	// Above maximum clamps to Run.
	params.setFloat("speed", 12.0f);
	tree.evaluate(ctx, out);
	EXPECT_NEAR(out.joints[0].translation.x, 6.0f, kEps);
}

TEST(BlendTreeTests, BlendTree2DInverseDistanceAndBarycentric) {
	auto skel = makeHumanoidLite();
	auto idle = makeConstClip("Idle", Vector3(0, 0, 0));
	auto fwd = makeConstClip("Forward", Vector3(0, 0, 1));
	auto back = makeConstClip("Back", Vector3(0, 0, -1));
	auto left = makeConstClip("Left", Vector3(-1, 0, 0));
	auto right = makeConstClip("Right", Vector3(1, 0, 0));

	// Inverse-Distance mode
	BlendTree2D idwTree("moveX", "moveY", BlendTree2DMode::InverseDistance);
	idwTree.addClip(idle, Vector2(0, 0));
	idwTree.addClip(fwd, Vector2(0, 1));
	idwTree.addClip(back, Vector2(0, -1));
	idwTree.addClip(left, Vector2(-1, 0));
	idwTree.addClip(right, Vector2(1, 0));

	AnimationParameters params;
	PoseEvalContext ctx{skel.get(), &params, nullptr, 0.1f};
	Pose out;

	// Exact sample point returns 100% Forward.
	params.setFloat("moveX", 0.0f);
	params.setFloat("moveY", 1.0f);
	idwTree.evaluate(ctx, out);
	EXPECT_NEAR(out.joints[0].translation.z, 1.0f, kEps);
	EXPECT_NEAR(out.joints[0].translation.x, 0.0f, kEps);

	// Diagonal (0.5, 0.5) blends symmetrically between Forward and Right.
	params.setFloat("moveX", 0.5f);
	params.setFloat("moveY", 0.5f);
	idwTree.evaluate(ctx, out);
	EXPECT_GT(out.joints[0].translation.x, 0.1f);
	EXPECT_NEAR(out.joints[0].translation.x, out.joints[0].translation.z, kEps);

	// Barycentric mode
	BlendTree2D baryTree("moveX", "moveY", BlendTree2DMode::Barycentric);
	baryTree.addClip(idle, Vector2(0, 0));
	baryTree.addClip(fwd, Vector2(0, 1));
	baryTree.addClip(right, Vector2(1, 0));
	baryTree.addClip(left, Vector2(-1, 0));
	baryTree.addClip(back, Vector2(0, -1));

	// Inside triangle (Idle, Forward, Right) at (0.25, 0.5):
	// exact barycentric weights -> Idle=0.25, Forward=0.5, Right=0.25.
	params.setFloat("moveX", 0.25f);
	params.setFloat("moveY", 0.5f);
	baryTree.evaluate(ctx, out);
	EXPECT_NEAR(out.joints[0].translation.x, 0.25f, kEps);
	EXPECT_NEAR(out.joints[0].translation.z, 0.5f, kEps);

	// Outside convex hull at (2, 2) clamps onto edge (Forward, Right) midpoint (0.5, 0.5).
	params.setFloat("moveX", 2.0f);
	params.setFloat("moveY", 2.0f);
	baryTree.evaluate(ctx, out);
	EXPECT_NEAR(out.joints[0].translation.x, 0.5f, kEps);
	EXPECT_NEAR(out.joints[0].translation.z, 0.5f, kEps);
}

// ============================================================================
// AnimationStateMachine Tests
// ============================================================================

TEST(AnimationStateMachineTests, TransitionsCrossFadeExitTimeAndTriggers) {
	auto skel = makeHumanoidLite();
	auto idleClip = makeConstClip("Idle", Vector3(0, 0, 0), 1.0f);
	auto runClip = makeConstClip("Run", Vector3(4, 0, 0), 1.0f);
	auto attackClip = makeConstClip("Attack", Vector3(0, 5, 0), 1.0f);
	auto hitClip = makeConstClip("Hit", Vector3(-3, 0, 0), 0.5f);

	AnimationStateMachine sm;
	sm.addClipState("Idle", idleClip, LoopMode::Loop);
	sm.addClipState("Run", runClip, LoopMode::Loop);
	sm.addClipState("Attack", attackClip, LoopMode::Once);
	sm.addClipState("Hit", hitClip, LoopMode::Once);
	sm.setEntryState("Idle");

	// Idle -> Run when speed > 0.5 (crossfade 0.5s)
	sm.addTransition({"Idle", "Run", 0.5f, false, 1.0f, 0, false,
					  {TransitionCondition::floatGreater("speed", 0.5f)}});
	// Run -> Attack on trigger "attack" (instant cut)
	sm.addTransition({"Run", "Attack", 0.0f, false, 1.0f, 1, false,
					  {TransitionCondition::trigger("attack")}});
	// Attack -> Idle via exit time >= 0.8 (crossfade 0.2s)
	sm.addTransition({"Attack", "Idle", 0.2f, true, 0.8f, 0, false, {}});
	// Any-State -> Hit on trigger "hurt" (high priority 10)
	sm.addTransition({"", "Hit", 0.0f, false, 1.0f, 10, false,
					  {TransitionCondition::trigger("hurt")}});

	AnimationParameters params;
	PoseEvalContext ctx{skel.get(), &params, nullptr, 0.25f};
	Pose out;

	// Starts in Idle.
	sm.evaluate(ctx, out);
	EXPECT_EQ(sm.getCurrentStateName(), "Idle");
	EXPECT_NEAR(out.joints[0].translation.x, 0.0f, kEps);

	// Set speed > 0.5 -> transitions to Run over 0.5s; after 0.25s it's 50% blended (x = 2).
	params.setFloat("speed", 3.0f);
	sm.evaluate(ctx, out);
	EXPECT_EQ(sm.getCurrentStateName(), "Run");
	EXPECT_TRUE(sm.isTransitioning());
	EXPECT_NEAR(out.joints[0].translation.x, 2.0f, kEps);

	// After another 0.25s -> crossfade completes (x = 4).
	sm.evaluate(ctx, out);
	EXPECT_FALSE(sm.isTransitioning());
	EXPECT_NEAR(out.joints[0].translation.x, 4.0f, kEps);

	// Arm trigger "attack" -> transitions to Attack and consumes trigger.
	params.setTrigger("attack");
	sm.evaluate(ctx, out);
	EXPECT_EQ(sm.getCurrentStateName(), "Attack");
	EXPECT_FALSE(params.isTriggerSet("attack"));
	EXPECT_NEAR(out.joints[0].translation.y, 5.0f, kEps);

	// Advance Attack to 0.85 normalized time, then next tick triggers exit-time -> Idle.
	ctx.deltaTime = 0.6f;
	sm.evaluate(ctx, out);
	EXPECT_EQ(sm.getCurrentStateName(), "Attack");
	ctx.deltaTime = 0.1f;
	sm.evaluate(ctx, out);
	EXPECT_EQ(sm.getCurrentStateName(), "Idle");
	EXPECT_TRUE(sm.isTransitioning());

	// Any-State high-priority trigger "hurt" interrupts immediately.
	params.setTrigger("hurt");
	sm.evaluate(ctx, out);
	EXPECT_EQ(sm.getCurrentStateName(), "Hit");
	EXPECT_NEAR(out.joints[0].translation.x, -3.0f, kEps);
}

// ============================================================================
// 2-Bone Analytic IK Tests
// ============================================================================

namespace {

/** 3-joint leg: hip at (0, 2, 0), knee at (0, 1, 0), ankle at (0, 0, 0). */
std::shared_ptr<Skeleton> makeLegSkeleton() {
	auto skel = std::make_shared<Skeleton>();
	skel->addJoint("hip", -1, Matrix4(1.0f), makeTRS(Vector3(0, 2, 0)));
	skel->addJoint("knee", 0, Matrix4(1.0f), makeTRS(Vector3(0, -1, 0)));
	skel->addJoint("ankle", 1, Matrix4(1.0f), makeTRS(Vector3(0, -1, 0)));
	return skel;
}

}  // namespace

TEST(TwoBoneIKTests, ReachesTargetAndRespectsPoleVector) {
	auto skel = makeLegSkeleton();
	Pose pose = skel->getRestPose();
	std::vector<Matrix4> model;
	localToModel(*skel, pose, model);

	TwoBoneIKConstraint ik;
	ik.name = "LeftFoot";
	ik.rootJoint = 0;
	ik.midJoint = 1;
	ik.endJoint = 2;
	// Reachable target: 1.0 unit below hip (two 1.0 bones form equilateral triangle).
	ik.targetPosition = Vector3(0.0f, 1.0f, 0.0f);
	ik.polePosition = Vector3(0.0f, 1.5f, 2.0f);  // Bend knee toward +Z
	ik.usePolePosition = true;
	ik.weight = 1.0f;
	ik.enabled = true;

	ASSERT_TRUE(solveTwoBoneIK(*skel, pose, model, ik));

	const Vector3 anklePos(model[2][3]);
	const Vector3 kneePos(model[1][3]);
	const Vector3 hipPos(model[0][3]);

	// Ankle must land on target within 1e-3.
	EXPECT_NEAR(anklePos.x, 0.0f, 1e-3f);
	EXPECT_NEAR(anklePos.y, 1.0f, 1e-3f);
	EXPECT_NEAR(anklePos.z, 0.0f, 1e-3f);

	// Knee must bend toward +Z pole and preserve bone lengths (1.0 each).
	EXPECT_GT(kneePos.z, 0.8f);
	EXPECT_NEAR(glm::length(kneePos - hipPos), 1.0f, 1e-3f);
	EXPECT_NEAR(glm::length(anklePos - kneePos), 1.0f, 1e-3f);
}

TEST(TwoBoneIKTests, ClampsUnreachableTargetAndRespectsWeight) {
	auto skel = makeLegSkeleton();
	Pose pose = skel->getRestPose();
	std::vector<Matrix4> model;
	localToModel(*skel, pose, model);

	TwoBoneIKConstraint ik;
	ik.rootJoint = 0;
	ik.midJoint = 1;
	ik.endJoint = 2;
	// 5 units out along +X (max reach is 2.0).
	ik.targetPosition = Vector3(5.0f, 2.0f, 0.0f);
	ik.polePosition = Vector3(0.0f, 2.0f, 1.0f);
	ik.weight = 1.0f;

	ASSERT_TRUE(solveTwoBoneIK(*skel, pose, model, ik));
	const Vector3 anklePos(model[2][3]);
	EXPECT_NEAR(anklePos.x, 2.0f, 2e-3f);
	EXPECT_NEAR(anklePos.y, 2.0f, 2e-3f);

	// Weight 0 leaves FK pose untouched.
	Pose fkPose = skel->getRestPose();
	localToModel(*skel, fkPose, model);
	ik.weight = 0.0f;
	EXPECT_FALSE(solveTwoBoneIK(*skel, fkPose, model, ik));
	EXPECT_NEAR(Vector3(model[2][3]).y, 0.0f, kEps);
}

// ============================================================================
// Root Motion Tests
// ============================================================================

TEST(RootMotionTests, ExtractsPerFrameDeltaAndZerosRootTranslation) {
	auto skel = makeHumanoidLite();
	RootMotionConfig cfg;
	cfg.enabled = true;
	cfg.rootJoint = 0;
	cfg.extractTranslationX = true;
	cfg.extractTranslationY = false;  // Keep vertical bounce in pose
	cfg.extractTranslationZ = true;

	RootMotionExtractor extractor;
	extractor.setConfig(cfg);

	// Frame 1: root at (0, 1.1, 0) -> initializes reference, delta = 0.
	Pose p1 = skel->getRestPose();
	p1.joints[0].translation = Vector3(0.0f, 1.1f, 0.0f);
	RootMotionDelta d1 = extractor.extract(*skel, p1, false);
	EXPECT_NEAR(glm::length(d1.translation), 0.0f, kEps);
	EXPECT_NEAR(p1.joints[0].translation.x, 0.0f, kEps);
	EXPECT_NEAR(p1.joints[0].translation.y, 1.1f, kEps);  // Y preserved
	EXPECT_NEAR(p1.joints[0].translation.z, 0.0f, kEps);

	// Frame 2: root moves to (0.5, 1.2, 1.5) -> delta = (0.5, 0, 1.5), XZ zeroed to rest.
	Pose p2 = skel->getRestPose();
	p2.joints[0].translation = Vector3(0.5f, 1.2f, 1.5f);
	RootMotionDelta d2 = extractor.extract(*skel, p2, false);
	EXPECT_NEAR(d2.translation.x, 0.5f, kEps);
	EXPECT_NEAR(d2.translation.y, 0.0f, kEps);
	EXPECT_NEAR(d2.translation.z, 1.5f, kEps);
	EXPECT_NEAR(p2.joints[0].translation.x, 0.0f, kEps);
	EXPECT_NEAR(p2.joints[0].translation.y, 1.2f, kEps);
	EXPECT_NEAR(p2.joints[0].translation.z, 0.0f, kEps);

	// Frame 3 (loop wrapped): root wrapped back to (0.1, 1.0, 0.2) -> positive delta from rest, no backward spike.
	Pose p3 = skel->getRestPose();
	p3.joints[0].translation = Vector3(0.1f, 1.0f, 0.2f);
	RootMotionDelta d3 = extractor.extract(*skel, p3, true);
	EXPECT_NEAR(d3.translation.x, 0.1f, kEps);
	EXPECT_NEAR(d3.translation.z, 0.2f, kEps);
}
