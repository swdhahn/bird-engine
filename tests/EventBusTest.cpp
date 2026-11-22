#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "events/EventBus.h"

namespace bird {
namespace {

TEST(EventBusTest, HashEventNameConstexprAndRuntimeMatch) {
	constexpr EventTypeID kDamage = hashEventName("player.damaged");
	EXPECT_NE(kDamage, kInvalidEventTypeID);
	EXPECT_EQ(kDamage, hashEventName(std::string("player.damaged")));
	EXPECT_NE(kDamage, hashEventName("player.healed"));
	EXPECT_EQ(hashEventName(""), kInvalidEventTypeID);
}

TEST(EventBusTest, PayloadRoundTripAndDefaults) {
	EventPayload p;
	EXPECT_TRUE(p.isEmpty());
	EXPECT_EQ(p.size(), 0u);

	p.set("flag", true)
		.set("count", 42)
		.set("entity", uint64_t{9001})
		.set("ratio", 0.75)
		.set("speed", 3.5f)
		.set("label", "goblin")
		.set("v2", Vector2(1.0f, 2.0f))
		.set("v3", Vector3(3.0f, 4.0f, 5.0f))
		.set("v4", Vector4(6.0f, 7.0f, 8.0f, 9.0f))
		.setNull("nothing");

	EXPECT_FALSE(p.isEmpty());
	EXPECT_EQ(p.size(), 10u);
	EXPECT_TRUE(p.has("label"));
	EXPECT_FALSE(p.has("missing"));

	EXPECT_EQ(p.getType("flag"), EventValueType::Bool);
	EXPECT_EQ(p.getType("count"), EventValueType::Int);
	EXPECT_EQ(p.getType("ratio"), EventValueType::Double);
	EXPECT_EQ(p.getType("label"), EventValueType::String);
	EXPECT_EQ(p.getType("v2"), EventValueType::Vector2);
	EXPECT_EQ(p.getType("v3"), EventValueType::Vector3);
	EXPECT_EQ(p.getType("v4"), EventValueType::Vector4);
	EXPECT_EQ(p.getType("nothing"), EventValueType::Null);

	EXPECT_TRUE(p.getBool("flag"));
	EXPECT_EQ(p.getInt("count"), 42);
	EXPECT_EQ(p.getInt("entity"), 9001);
	EXPECT_DOUBLE_EQ(p.getDouble("ratio"), 0.75);
	EXPECT_FLOAT_EQ(p.getFloat("speed"), 3.5f);
	// Int widens to double/float cleanly.
	EXPECT_DOUBLE_EQ(p.getDouble("count"), 42.0);
	EXPECT_EQ(p.getString("label"), "goblin");
	EXPECT_EQ(p.getVector2("v2"), Vector2(1.0f, 2.0f));
	EXPECT_EQ(p.getVector3("v3"), Vector3(3.0f, 4.0f, 5.0f));
	EXPECT_EQ(p.getVector4("v4"), Vector4(6.0f, 7.0f, 8.0f, 9.0f));

	// Overwrite existing key in place.
	p.set("count", 99);
	EXPECT_EQ(p.size(), 10u);
	EXPECT_EQ(p.getInt("count"), 99);

	// Missing / mismatched type returns fallback.
	EXPECT_EQ(p.getInt("missing", -7), -7);
	EXPECT_EQ(p.getInt("label", -1), -1);
	EXPECT_EQ(p.getString("count", "fallback"), "fallback");

	p.clear();
	EXPECT_TRUE(p.isEmpty());
}

TEST(EventBusTest, QueuedVsImmediateDispatchOrder) {
	EventBus bus;
	std::vector<std::string> log;

	bus.subscribe("step", [&](const EventPayload& p) {
		log.push_back(p.getString("tag"));
	});

	EventPayload q1;
	q1.set("tag", "queued-1");
	EventPayload q2;
	q2.set("tag", "queued-2");
	EventPayload imm;
	imm.set("tag", "immediate");

	EXPECT_TRUE(bus.emit("step", q1));
	EXPECT_TRUE(bus.emit("step", q2));
	EXPECT_EQ(bus.queuedCount(), 2u);
	EXPECT_TRUE(log.empty());

	// Immediate fires synchronously before queued events are dispatched.
	bus.emitImmediate("step", imm);
	ASSERT_EQ(log.size(), 1u);
	EXPECT_EQ(log[0], "immediate");
	EXPECT_EQ(bus.queuedCount(), 2u);

	// Dispatching queued events drains them in FIFO order.
	EXPECT_EQ(bus.dispatchQueued(), 2u);
	EXPECT_EQ(bus.queuedCount(), 0u);
	ASSERT_EQ(log.size(), 3u);
	EXPECT_EQ(log[1], "queued-1");
	EXPECT_EQ(log[2], "queued-2");
}

TEST(EventBusTest, QueuedEmitDuringDispatchDefersToNextFrame) {
	EventBus bus;
	std::vector<int64_t> seen;

	bus.subscribe("chain", [&](const EventPayload& p) {
		const int64_t gen = p.getInt("gen");
		seen.push_back(gen);
		if (gen < 2) {
			EventPayload next;
			next.set("gen", gen + 1);
			bus.emit("chain", next);
		}
	});

	EventPayload start;
	start.set("gen", 0);
	bus.emit("chain", start);

	EXPECT_EQ(bus.dispatchQueued(), 1u);
	ASSERT_EQ(seen.size(), 1u);
	EXPECT_EQ(seen[0], 0);
	EXPECT_EQ(bus.queuedCount(), 1u);

	EXPECT_EQ(bus.dispatchQueued(), 1u);
	ASSERT_EQ(seen.size(), 2u);
	EXPECT_EQ(seen[1], 1);

	EXPECT_EQ(bus.dispatchQueued(), 1u);
	ASSERT_EQ(seen.size(), 3u);
	EXPECT_EQ(seen[2], 2);
	EXPECT_EQ(bus.queuedCount(), 0u);
}

TEST(EventBusTest, RingBufferWrapAroundAndOverflow) {
	EventBus bus(4);
	EXPECT_EQ(bus.queueCapacity(), 4u);

	std::vector<int64_t> received;
	bus.subscribe("tick", [&](const EventPayload& p) {
		received.push_back(p.getInt("i"));
	});

	// Fill capacity = 4, 5th drops.
	for (int i = 0; i < 4; ++i) {
		EventPayload p;
		p.set("i", i);
		EXPECT_TRUE(bus.emit("tick", p));
	}
	EventPayload overflow;
	overflow.set("i", 999);
	EXPECT_FALSE(bus.emit("tick", overflow));
	EXPECT_EQ(bus.droppedCount(), 1u);
	EXPECT_EQ(bus.queuedCount(), 4u);

	// Drain and wrap around multiple times.
	EXPECT_EQ(bus.dispatchQueued(), 4u);
	EXPECT_EQ(received, (std::vector<int64_t>{0, 1, 2, 3}));

	received.clear();
	for (int i = 10; i < 14; ++i) {
		EventPayload p;
		p.set("i", i);
		EXPECT_TRUE(bus.emit("tick", p));
	}
	EXPECT_EQ(bus.dispatchQueued(), 4u);
	EXPECT_EQ(received, (std::vector<int64_t>{10, 11, 12, 13}));
}

TEST(EventBusTest, SafeUnsubscribeSelfAndOtherDuringDispatch) {
	EventBus bus;
	int aCalls = 0;
	int bCalls = 0;
	int cCalls = 0;

	SubscriptionID idA = kInvalidSubscriptionID;
	SubscriptionID idB = kInvalidSubscriptionID;
	SubscriptionID idC = kInvalidSubscriptionID;

	idA = bus.subscribe("ping", [&](const EventPayload&) {
		++aCalls;
		// Unsubscribe self and upcoming listener B mid-dispatch.
		EXPECT_TRUE(bus.unsubscribe(idA));
		EXPECT_TRUE(bus.unsubscribe(idB));
	});
	idB = bus.subscribe("ping", [&](const EventPayload&) { ++bCalls; });
	idC = bus.subscribe("ping", [&](const EventPayload&) { ++cCalls; });

	EXPECT_EQ(bus.subscriberCount(hashEventName("ping")), 3u);

	bus.emitImmediate("ping");
	EXPECT_EQ(aCalls, 1);
	EXPECT_EQ(bCalls, 0);  // Skipped immediately even in the same dispatch!
	EXPECT_EQ(cCalls, 1);
	EXPECT_EQ(bus.subscriberCount(hashEventName("ping")), 1u);

	bus.emitImmediate("ping");
	EXPECT_EQ(aCalls, 1);
	EXPECT_EQ(bCalls, 0);
	EXPECT_EQ(cCalls, 2);

	EXPECT_TRUE(bus.unsubscribe(idC));
	EXPECT_FALSE(bus.hasSubscribers("ping"));
}

TEST(EventBusTest, SafeSubscribeDuringDispatchDefersToNextEvent) {
	EventBus bus;
	int firstCalls = 0;
	int lateCalls = 0;

	bus.subscribe("spawn", [&](const EventPayload&) {
		++firstCalls;
		if (firstCalls == 1) {
			bus.subscribe("spawn", [&](const EventPayload&) { ++lateCalls; });
		}
	});

	bus.emitImmediate("spawn");
	EXPECT_EQ(firstCalls, 1);
	EXPECT_EQ(lateCalls, 0);

	bus.emitImmediate("spawn");
	EXPECT_EQ(firstCalls, 2);
	EXPECT_EQ(lateCalls, 1);
}

TEST(EventBusTest, EntityOwnershipCleanupAndUnsubscribeHook) {
	EventBus bus;
	std::vector<SubscriptionID> hookRemoved;
	bus.setUnsubscribeHook(
		[&](SubscriptionID id) { hookRemoved.push_back(id); });

	int entity1Calls = 0;
	int entity2Calls = 0;
	int globalCalls = 0;

	const EntityID kEntity1 = 101;
	const EntityID kEntity2 = 202;

	SubscriptionID s1 = bus.subscribe(
		"a", [&](const EventPayload&) { ++entity1Calls; }, kEntity1);
	SubscriptionID s2 = bus.subscribe(
		"b", [&](const EventPayload&) { ++entity1Calls; }, kEntity1);
	SubscriptionID s3 = bus.subscribe(
		"a", [&](const EventPayload&) { ++entity2Calls; }, kEntity2);
	SubscriptionID sGlobal = bus.subscribe(
		"a", [&](const EventPayload&) { ++globalCalls; }, kNoOwnerEntityID);

	EXPECT_EQ(bus.totalSubscriberCount(), 4u);

	// Destroy entity 1 while event "a" is NOT dispatching.
	EXPECT_EQ(bus.unsubscribeByOwner(kEntity1), 2u);
	EXPECT_EQ(bus.totalSubscriberCount(), 2u);
	ASSERT_EQ(hookRemoved.size(), 2u);
	EXPECT_EQ(hookRemoved[0], s1);
	EXPECT_EQ(hookRemoved[1], s2);

	bus.emitImmediate("a");
	bus.emitImmediate("b");
	EXPECT_EQ(entity1Calls, 0);
	EXPECT_EQ(entity2Calls, 1);
	EXPECT_EQ(globalCalls, 1);

	// Clear invokes hook for remaining subscriptions.
	bus.clear();
	EXPECT_EQ(bus.totalSubscriberCount(), 0u);
	EXPECT_EQ(hookRemoved.size(), 4u);
	(void)s3;
	(void)sGlobal;
}

TEST(EventBusTest, UnsubscribeByOwnerDuringDispatchSkipsDeadEntity) {
	EventBus bus;
	const EntityID kVictim = 55;
	int victimCalls = 0;

	bus.subscribe("boom", [&](const EventPayload&) {
		EXPECT_EQ(bus.unsubscribeByOwner(kVictim), 1u);
	});
	bus.subscribe(
		"boom", [&](const EventPayload&) { ++victimCalls; }, kVictim);

	bus.emitImmediate("boom");
	EXPECT_EQ(victimCalls, 0);
	EXPECT_EQ(bus.subscriberCount(hashEventName("boom")), 1u);
}

}  // namespace
}  // namespace bird
