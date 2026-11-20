#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

#include "animation/skeletal/PoseOps.h"
#include "animation/skeletal/SkeletalAnimator.h"
#include "animation/skeletal/SkeletalClip.h"
#include "animation/skeletal/Skeleton.h"

using namespace bird;

namespace {

constexpr float kEps = 1e-4f;

JointTransform at(Vector3 translation) {
	JointTransform t;
	t.translation = translation;
	return t;
}

/** Root at the origin with a child one unit up (a two-bone arm). */
std::shared_ptr<Skeleton> makeArm() {
	auto skeleton = std::make_shared<Skeleton>();
	skeleton->addJoint("root", -1, Matrix4(1.0f), at(Vector3(0.0f)));
	// Bind pose: child sits at y = 1, so its inverse bind moves y back.
	skeleton->addJoint("tip", 0,
					   glm::translate(Matrix4(1.0f), Vector3(0, -1, 0)),
					   at(Vector3(0, 1, 0)));
	return skeleton;
}

/** Rotates the root 90 degrees about Z over one second. */
std::shared_ptr<SkeletalClip> makeSwing(float seconds = 1.0f) {
	auto clip = std::make_shared<SkeletalClip>("Swing", seconds);
	JointTrack track;
	track.joint = 0;
	track.rotation.addKey(0.0f, Quaternion(1, 0, 0, 0));
	track.rotation.addKey(seconds,
						  glm::angleAxis(glm::radians(90.0f), Vector3(0, 0, 1)));
	clip->addTrack(std::move(track));
	return clip;
}

/** Moves the tip along +x over one second. */
std::shared_ptr<SkeletalClip> makeSlide() {
	auto clip = std::make_shared<SkeletalClip>("Slide", 1.0f);
	JointTrack track;
	track.joint = 1;
	track.translation.addKey(0.0f, Vector3(0, 1, 0));
	track.translation.addKey(1.0f, Vector3(2, 1, 0));
	clip->addTrack(std::move(track));
	return clip;
}

Vector3 apply(const Matrix3x4& palette, Vector3 p) {
	return Vector3(Vector4(p, 1.0f) * palette);
}

}  // namespace

TEST(Skeleton, EnforcesTopologicalOrderAndUniqueNames) {
	Skeleton skeleton;
	EXPECT_EQ(skeleton.addJoint("a", -1, Matrix4(1), {}), 0);
	EXPECT_THROW(skeleton.addJoint("b", 1, Matrix4(1), {}),
				 std::invalid_argument);	 // parent not yet defined
	EXPECT_THROW(skeleton.addJoint("a", 0, Matrix4(1), {}),
				 std::invalid_argument);
	EXPECT_THROW(skeleton.addJoint("", 0, Matrix4(1), {}),
				 std::invalid_argument);
	EXPECT_EQ(skeleton.addJoint("b", 0, Matrix4(1), {}), 1);
	EXPECT_EQ(skeleton.findJoint("b"), 1);
	EXPECT_EQ(skeleton.findJoint("missing"), -1);
	EXPECT_THROW(skeleton.getParent(7), std::out_of_range);
}

TEST(PoseOps, RestPoseProducesIdentityPalette) {
	auto skeleton = makeArm();
	std::vector<Matrix4> model;
	std::vector<Matrix3x4> palette;
	localToModel(*skeleton, skeleton->getRestPose(), model);
	buildSkinningPalette(*skeleton, model, palette);
	ASSERT_EQ(palette.size(), 2u);
	for (const Matrix3x4& entry : palette) {
		const Vector3 p = apply(entry, Vector3(0.3f, 1.7f, -2.0f));
		EXPECT_NEAR(p.x, 0.3f, kEps);
		EXPECT_NEAR(p.y, 1.7f, kEps);
		EXPECT_NEAR(p.z, -2.0f, kEps);
	}
}

TEST(PoseOps, ParentRotationCarriesChildren) {
	auto skeleton = makeArm();
	Pose pose = skeleton->getRestPose();
	pose.joints[0].rotation =
		glm::angleAxis(glm::radians(90.0f), Vector3(0, 0, 1));
	std::vector<Matrix4> model;
	localToModel(*skeleton, pose, model);
	const Vector3 tip = Vector3(model[1][3]);
	EXPECT_NEAR(tip.x, -1.0f, kEps);  // (0,1) rotated 90 degrees about Z
	EXPECT_NEAR(tip.y, 0.0f, kEps);
}

TEST(PoseOps, BlendInterpolatesEveryChannel) {
	Pose a, b, out;
	a.joints.push_back(at(Vector3(0.0f)));
	b.joints.push_back(at(Vector3(2.0f)));
	b.joints[0].scale = Vector3(3.0f);
	b.joints[0].rotation = glm::angleAxis(glm::radians(90.0f), Vector3(0, 1, 0));
	blendPoses(a, b, 0.5f, out);
	EXPECT_NEAR(out.joints[0].translation.x, 1.0f, kEps);
	EXPECT_NEAR(out.joints[0].scale.y, 2.0f, kEps);
	EXPECT_NEAR(glm::degrees(glm::angle(out.joints[0].rotation)), 45.0f, 0.01f);
	Pose wrong;
	EXPECT_THROW(blendPoses(a, wrong, 0.5f, out), std::invalid_argument);
}

TEST(PoseOps, PaletteEntryMatchesMatrixTransform) {
	const Matrix4 m = glm::translate(Matrix4(1.0f), Vector3(1, 2, 3)) *
					  glm::mat4_cast(glm::angleAxis(0.7f, Vector3(0, 1, 0))) *
					  glm::scale(Matrix4(1.0f), Vector3(2.0f));
	const Vector3 p(0.5f, -1.0f, 4.0f);
	const Vector3 expected = Vector3(m * Vector4(p, 1.0f));
	const Vector3 actual = apply(toPaletteEntry(m), p);
	EXPECT_NEAR(actual.x, expected.x, kEps);
	EXPECT_NEAR(actual.y, expected.y, kEps);
	EXPECT_NEAR(actual.z, expected.z, kEps);
}

TEST(SkeletalClip, SamplesOnlyAnimatedChannels) {
	auto skeleton = makeArm();
	auto clip = makeSwing();
	Pose pose = skeleton->getRestPose();
	ClipCursor cursor;
	clip->sample(0.5f, pose, cursor);
	EXPECT_NEAR(glm::degrees(glm::angle(pose.joints[0].rotation)), 45.0f, 0.01f);
	EXPECT_EQ(pose.joints[1].translation, Vector3(0, 1, 0));  // untouched
	EXPECT_THROW(SkeletalClip("bad", -1.0f), std::invalid_argument);
	JointTrack duplicate;
	duplicate.joint = 0;
	EXPECT_THROW(clip->addTrack(std::move(duplicate)), std::invalid_argument);
}

TEST(SkeletalAnimator, PlaysLoopsAndSkinsVertices) {
	SkeletalAnimator animator(makeArm());
	animator.play(makeSwing(), LoopMode::Loop);
	animator.update(1.0f - 1e-4f);
	// A vertex bound to the tip at (0, 1) swings to (-1, 0).
	const Vector3 skinned =
		apply(animator.getSkinningPalette()[1], Vector3(0, 1, 0));
	EXPECT_NEAR(skinned.x, -1.0f, 1e-3f);
	EXPECT_NEAR(skinned.y, 0.0f, 1e-3f);
	animator.update(0.5f);	// wraps
	EXPECT_NEAR(glm::degrees(glm::angle(animator.getLocalPose().joints[0].rotation)),
				45.0f, 0.1f);
	EXPECT_TRUE(animator.isPlaying());
}

TEST(SkeletalAnimator, OnceClipHoldsLastPoseAndNotifies) {
	SkeletalAnimator animator(makeArm());
	int finished = 0;
	animator.setOnFinished([&] { ++finished; });
	animator.play(makeSwing(), LoopMode::Once);
	animator.update(2.0f);
	animator.update(1.0f);
	EXPECT_EQ(finished, 1);
	EXPECT_NEAR(glm::degrees(glm::angle(animator.getLocalPose().joints[0].rotation)),
				90.0f, 0.01f);
}

TEST(SkeletalAnimator, CrossFadeBlendsThenSettles) {
	SkeletalAnimator animator(makeArm());
	animator.play(makeSlide(), LoopMode::Once);
	animator.update(1.0f);	// tip at x = 2
	animator.crossFade(makeSwing(), 0.5f);
	EXPECT_TRUE(animator.isFading());
	animator.update(0.25f);
	// Half way: tip translation between 2 (slide end) and 0 (rest).
	EXPECT_NEAR(animator.getLocalPose().joints[1].translation.x, 1.0f, kEps);
	animator.update(0.5f);
	EXPECT_FALSE(animator.isFading());
	EXPECT_FLOAT_EQ(animator.getLocalPose().joints[1].translation.x, 0.0f);
	EXPECT_EQ(animator.getCurrentClip()->getName(), "Swing");
}

TEST(SkeletalAnimator, StopReturnsToRestAndValidatesInput) {
	SkeletalAnimator animator(makeArm());
	animator.play(makeSlide());
	animator.update(0.5f);
	animator.stop();
	animator.update(0.1f);
	EXPECT_EQ(animator.getLocalPose().joints[1].translation, Vector3(0, 1, 0));
	EXPECT_THROW(animator.play(nullptr), std::invalid_argument);
	EXPECT_THROW(animator.update(-1.0f), std::invalid_argument);
	EXPECT_THROW(animator.crossFade(makeSwing(), -1.0f), std::invalid_argument);
	EXPECT_THROW(SkeletalAnimator(nullptr), std::invalid_argument);
	EXPECT_THROW(animator.getJointModelMatrix(5), std::out_of_range);

	auto foreign = std::make_shared<SkeletalClip>("Foreign", 1.0f);
	JointTrack track;
	track.joint = 9;
	track.rotation.addKey(0.0f, Quaternion(1, 0, 0, 0));
	foreign->addTrack(std::move(track));
	EXPECT_THROW(animator.play(foreign), std::invalid_argument);
}
