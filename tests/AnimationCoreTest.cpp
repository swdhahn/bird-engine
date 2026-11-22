#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <vector>

#include "animation/AnimationSystem.h"
#include "animation/Curve.h"
#include "animation/Curves.h"
#include "animation/Easing.h"
#include "animation/PlaybackClock.h"
#include "animation/Tween.h"

using namespace bird;

namespace {
constexpr float kEps = 1e-4f;
}

// ------------------------------------------------------------------ easing

TEST(Easing, EveryCurveStartsAtZeroAndEndsAtOne) {
	for (int e = 0; e <= static_cast<int>(Ease::InOutBounce); ++e) {
		const Ease ease = static_cast<Ease>(e);
		EXPECT_NEAR(evaluateEase(ease, 0.0f), 0.0f, kEps) << e;
		EXPECT_NEAR(evaluateEase(ease, 1.0f), 1.0f, kEps) << e;
	}
}

TEST(Easing, InOutCurvesAreSymmetricAtHalf) {
	for (Ease ease : {Ease::InOutQuad, Ease::InOutCubic, Ease::InOutQuart,
					  Ease::InOutSine, Ease::InOutExpo, Ease::InOutBack,
					  Ease::InOutElastic, Ease::InOutBounce}) {
		EXPECT_NEAR(evaluateEase(ease, 0.5f), 0.5f, kEps);
	}
}

TEST(Easing, OutBackOvershootsWithoutClamping) {
	float peak = 0.0f;
	for (int i = 0; i <= 100; ++i) {
		peak = std::max(peak, evaluateEase(Ease::OutBack, i / 100.0f));
	}
	EXPECT_GT(peak, 1.05f);
}

TEST(Easing, InputIsClampedToUnitRange) {
	EXPECT_FLOAT_EQ(evaluateEase(Ease::Linear, -1.0f), 0.0f);
	EXPECT_FLOAT_EQ(evaluateEase(Ease::Linear, 2.0f), 1.0f);
}

TEST(CubicBezierEase, LinearControlPointsAreIdentity) {
	const CubicBezierEase linear(0.0f, 0.0f, 1.0f, 1.0f);
	for (float t : {0.0f, 0.1f, 0.33f, 0.5f, 0.9f, 1.0f}) {
		EXPECT_NEAR(linear.evaluate(t), t, 1e-3f);
	}
}

TEST(CubicBezierEase, CssEaseMatchesKnownValue) {
	// cubic-bezier(0.25, 0.1, 0.25, 1) is CSS "ease"; ~0.8024 at t = 0.5.
	const CubicBezierEase ease(0.25f, 0.1f, 0.25f, 1.0f);
	EXPECT_NEAR(ease.evaluate(0.5f), 0.8024f, 2e-3f);
}

TEST(CubicBezierEase, RejectsOutOfRangeX) {
	EXPECT_THROW(CubicBezierEase(-0.1f, 0.0f, 1.0f, 1.0f),
				 std::invalid_argument);
	EXPECT_THROW(CubicBezierEase(0.0f, 0.0f, 1.5f, 1.0f),
				 std::invalid_argument);
}

// ------------------------------------------------------------------- curve

TEST(Curve, EmptyCurveSampleFailsFast) {
	Curve<float> curve;
	EXPECT_THROW(curve.sample(0.0f), std::logic_error);
}

TEST(Curve, KeysStaySortedAndDuplicateTimesReplace) {
	Curve<float> curve;
	curve.addKey(2.0f, 20.0f);
	curve.addKey(0.0f, 0.0f);
	curve.addKey(1.0f, 10.0f);
	curve.addKey(1.0f, 15.0f);
	ASSERT_EQ(curve.getKeyCount(), 3u);
	EXPECT_FLOAT_EQ(curve.getKeys()[1].value, 15.0f);
	EXPECT_FLOAT_EQ(curve.getEndTime(), 2.0f);
}

TEST(Curve, RejectsNonFiniteTimes) {
	Curve<float> curve;
	EXPECT_THROW(curve.addKey(NAN, 1.0f), std::invalid_argument);
	EXPECT_THROW(curve.addKey(INFINITY, 1.0f), std::invalid_argument);
}

TEST(Curve, LinearStepAndClampBehaviour) {
	Curve<float> curve;
	curve.addKey(0.0f, 0.0f);
	curve.addKey(1.0f, 10.0f);
	curve.addKey(3.0f, 30.0f);
	EXPECT_FLOAT_EQ(curve.sample(-5.0f), 0.0f);
	EXPECT_FLOAT_EQ(curve.sample(0.5f), 5.0f);
	EXPECT_FLOAT_EQ(curve.sample(2.0f), 20.0f);
	EXPECT_FLOAT_EQ(curve.sample(9.0f), 30.0f);
	curve.setInterpolation(CurveInterpolation::Step);
	EXPECT_FLOAT_EQ(curve.sample(0.99f), 0.0f);
	EXPECT_FLOAT_EQ(curve.sample(1.0f), 10.0f);
}

TEST(Curve, HermiteWithZeroTangentsEasesInAndOut) {
	Curve<float> curve(CurveInterpolation::CubicHermite);
	curve.addKey(0.0f, 0.0f);
	curve.addKey(1.0f, 1.0f);
	EXPECT_NEAR(curve.sample(0.5f), 0.5f, kEps);
	EXPECT_LT(curve.sample(0.1f), 0.1f);  // slow start
	EXPECT_GT(curve.sample(0.9f), 0.9f);  // slow finish
}

TEST(Curve, AutoTangentsReproduceALine) {
	Curve<float> curve(CurveInterpolation::CubicHermite);
	for (int i = 0; i <= 4; ++i) curve.addKey(float(i), float(i) * 2.0f);
	curve.computeAutoTangents();
	for (float t : {0.25f, 1.5f, 3.75f}) {
		EXPECT_NEAR(curve.sample(t), t * 2.0f, kEps);
	}
}

TEST(Curve, CursorSamplingMatchesBinarySearch) {
	Curve<float> curve;
	for (int i = 0; i < 50; ++i) curve.addKey(i * 0.1f, std::sin(i * 0.3f));
	size_t cursor = 0;
	for (float t = 0.0f; t < 5.0f; t += 0.013f) {
		EXPECT_FLOAT_EQ(curve.sample(t, cursor), curve.sample(t));
	}
	cursor = 9999;	// garbage hint after a seek must still be safe
	EXPECT_FLOAT_EQ(curve.sample(2.05f, cursor), curve.sample(2.05f));
}

TEST(Curve, QuaternionTakesShortestArc) {
	Curve<Quaternion> curve;
	const Quaternion a = glm::angleAxis(0.0f, Vector3(0, 1, 0));
	// Same orientation as +170 degrees, stored in the opposite hemisphere.
	const Quaternion b = -glm::angleAxis(glm::radians(170.0f), Vector3(0, 1, 0));
	curve.addKey(0.0f, a);
	curve.addKey(1.0f, b);
	const Quaternion mid = curve.sample(0.5f);
	EXPECT_NEAR(glm::length(mid), 1.0f, kEps);
	EXPECT_NEAR(glm::degrees(glm::angle(mid)), 85.0f, 0.1f);
}

// ------------------------------------------------------------------- clock

TEST(PlaybackClock, OnceFinishesAndClamps) {
	PlaybackClock clock(1.0f);
	EXPECT_FALSE(clock.advance(0.6f).finished);
	const PlaybackEvents events = clock.advance(0.6f);
	EXPECT_TRUE(events.finished);
	EXPECT_TRUE(clock.isFinished());
	EXPECT_FLOAT_EQ(clock.getTime(), 1.0f);
	EXPECT_FALSE(clock.advance(1.0f).finished);  // reported once
}

TEST(PlaybackClock, LoopWrapsAndCountsCycles) {
	PlaybackClock clock(1.0f);
	clock.setLoopMode(LoopMode::Loop);
	EXPECT_TRUE(clock.advance(2.25f).looped);
	EXPECT_NEAR(clock.getTime(), 0.25f, kEps);
	EXPECT_EQ(clock.getCompletedCycles(), 2u);
	EXPECT_FALSE(clock.isFinished());
}

TEST(PlaybackClock, PingPongReflects) {
	PlaybackClock clock(1.0f);
	clock.setLoopMode(LoopMode::PingPong);
	clock.advance(1.25f);
	EXPECT_NEAR(clock.getTime(), 0.75f, kEps);
	clock.advance(0.5f);
	EXPECT_NEAR(clock.getTime(), 0.25f, kEps);
	clock.advance(0.5f);  // bounces off zero
	EXPECT_NEAR(clock.getTime(), 0.25f, kEps);
	clock.advance(10.0f);  // long stall stays in range
	EXPECT_GE(clock.getTime(), 0.0f);
	EXPECT_LE(clock.getTime(), 1.0f);
}

TEST(PlaybackClock, DelayHoldsTimeAtStart) {
	PlaybackClock clock(1.0f);
	clock.setDelay(0.5f);
	clock.advance(0.3f);
	EXPECT_TRUE(clock.isDelayed());
	EXPECT_FLOAT_EQ(clock.getTime(), 0.0f);
	clock.advance(0.4f);  // 0.2 delay left, 0.2 played
	EXPECT_NEAR(clock.getTime(), 0.2f, kEps);
}

TEST(PlaybackClock, ReverseAndPauseAndValidation) {
	PlaybackClock clock(1.0f);
	clock.setSpeed(-1.0f);
	clock.restart();
	EXPECT_FLOAT_EQ(clock.getTime(), 1.0f);
	clock.pause();
	clock.advance(0.5f);
	EXPECT_FLOAT_EQ(clock.getTime(), 1.0f);
	clock.resume();
	EXPECT_TRUE(clock.advance(2.0f).finished);
	EXPECT_FLOAT_EQ(clock.getTime(), 0.0f);
	EXPECT_THROW(clock.advance(-0.1f), std::invalid_argument);
	EXPECT_THROW(PlaybackClock(-1.0f), std::invalid_argument);
	EXPECT_THROW(clock.setSpeed(NAN), std::invalid_argument);
}

TEST(PlaybackClock, ZeroDurationFinishesImmediately) {
	PlaybackClock clock(0.0f);
	EXPECT_TRUE(clock.advance(0.0f).finished);
	EXPECT_FLOAT_EQ(clock.getNormalizedTime(), 1.0f);
}

// ------------------------------------------------------------------ system

TEST(AnimationSystem, TweenAppliesFinalValueBeforeFinishedCallback) {
	AnimationSystem system;
	float value = -1.0f;
	float valueSeenByCallback = -1.0f;
	const AnimationHandle handle = system.tweenFloat(
		0.0f, 10.0f, 1.0f, Ease::Linear, [&](float v) { value = v; });
	system.get(handle)->setOnFinished([&] { valueSeenByCallback = value; });
	system.tick(0.5f);
	EXPECT_FLOAT_EQ(value, 5.0f);
	system.tick(0.75f);
	EXPECT_FLOAT_EQ(value, 10.0f);
	EXPECT_FLOAT_EQ(valueSeenByCallback, 10.0f);
	EXPECT_FALSE(system.isActive(handle));
	EXPECT_EQ(system.getActiveCount(), 0u);
}

TEST(AnimationSystem, StaleHandlesNeverResolveAfterSlotReuse) {
	AnimationSystem system;
	const AnimationHandle first =
		system.tweenFloat(0, 1, 1, Ease::Linear, [](float) {});
	EXPECT_TRUE(system.cancel(first));
	const AnimationHandle second =
		system.tweenFloat(0, 1, 1, Ease::Linear, [](float) {});
	EXPECT_EQ(first.index, second.index);  // slot recycled
	EXPECT_FALSE(system.isActive(first));
	EXPECT_FALSE(system.cancel(first));
	EXPECT_TRUE(system.isActive(second));
	EXPECT_FALSE(system.isActive(AnimationHandle{}));
}

TEST(AnimationSystem, CallbacksMayCancelAndStartAnimationsMidTick) {
	AnimationSystem system;
	int follow = 0;
	float other = 0.0f;
	const AnimationHandle victim =
		system.tweenFloat(0, 1, 5, Ease::Linear, [&](float v) { other = v; });
	const AnimationHandle starter =
		system.tweenFloat(0, 1, 0.1f, Ease::Linear, [](float) {});
	system.get(starter)->setOnFinished([&] {
		system.cancel(victim);
		system.tweenFloat(0, 1, 1, Ease::Linear, [&](float) { ++follow; });
	});
	system.tick(0.2f);
	EXPECT_FALSE(system.isActive(victim));
	EXPECT_EQ(follow, 0);  // new animations start on the next tick
	EXPECT_EQ(system.getActiveCount(), 1u);
	system.tick(0.1f);
	EXPECT_EQ(follow, 1);
}

TEST(AnimationSystem, AnimationMayCancelItselfFromItsCallback) {
	AnimationSystem system;
	AnimationHandle self;
	self = system.tweenFloat(0, 1, 1, Ease::Linear, [](float) {});
	system.get(self)->clock().setLoopMode(LoopMode::Loop);
	system.get(self)->setOnLoop([&] { system.cancel(self); });
	system.tick(1.5f);
	EXPECT_FALSE(system.isActive(self));
	system.tick(0.1f);	// graveyard flushed safely
}

TEST(AnimationSystem, CancelOwnerStopsOnlyThatOwner) {
	AnimationSystem system;
	int a = 0, b = 0;
	auto make = [&](int& counter, const void* owner) {
		return system.play(_own_mk<Tween<float>>(
							   0.0f, 1.0f, 1.0f, Ease::Linear,
							   [&counter](const float&) { ++counter; }),
						   owner);
	};
	make(a, &a);
	make(b, &b);
	EXPECT_EQ(system.cancelOwner(&a), 1u);
	system.tick(0.1f);
	EXPECT_EQ(a, 0);
	EXPECT_EQ(b, 1);
}

TEST(AnimationSystem, PauseResumeAndSpeedThroughHandles) {
	AnimationSystem system;
	float value = 0.0f;
	const AnimationHandle h =
		system.tweenFloat(0, 10, 1, Ease::Linear, [&](float v) { value = v; });
	system.pause(h);
	system.tick(0.5f);
	EXPECT_FLOAT_EQ(value, 0.0f);
	system.resume(h);
	system.setSpeed(h, 2.0f);
	system.tick(0.25f);
	EXPECT_FLOAT_EQ(value, 5.0f);
}

TEST(AnimationSystem, RejectsInvalidInput) {
	AnimationSystem system;
	EXPECT_THROW(system.play(nullptr), std::invalid_argument);
	EXPECT_THROW(system.tick(-1.0f), std::invalid_argument);
	EXPECT_THROW(system.tweenFloat(0, 1, 1, Ease::Linear, nullptr),
				 std::invalid_argument);
	EXPECT_THROW(system.tweenFloat(0, 1, -1, Ease::Linear, [](float) {}),
				 std::invalid_argument);
}

TEST(ScriptCurves, SharedCurveDrivesPlayback) {
	AnimationSystem system;
	Vector3Curve curve;
	curve.addKey(0.0f, Vector3(0.0f));
	curve.addKey(2.0f, Vector3(4.0f, 2.0f, 0.0f));
	Vector3 position(0.0f);
	const AnimationHandle h =
		curve.play(system, [&](Vector3 v) { position = v; });
	system.tick(1.0f);
	EXPECT_NEAR(position.x, 2.0f, kEps);
	EXPECT_NEAR(position.y, 1.0f, kEps);
	system.tick(5.0f);
	EXPECT_FALSE(system.isActive(h));
	EXPECT_NEAR(position.x, 4.0f, kEps);
	FloatCurve empty;
	EXPECT_THROW(empty.play(system, [](float) {}), std::invalid_argument);
}
