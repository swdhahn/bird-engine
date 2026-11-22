#include "BlendTree.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

#include "PoseOps.h"

namespace bird {

namespace {

bool advanceNormalizedPhase(float& phase, float deltaTime, float speed,
							float effectiveDuration, LoopMode loopMode,
							bool& finished) {
	if (effectiveDuration <= 1e-6f) {
		phase = 1.0f;
		if (loopMode == LoopMode::Once) finished = true;
		return false;
	}
	if (finished) return false;
	const float deltaPhase = (deltaTime * speed) / effectiveDuration;
	float next = phase + deltaPhase;
	bool wrapped = false;
	if (loopMode == LoopMode::Once) {
		if (speed >= 0.0f && next >= 1.0f) {
			next = 1.0f;
			finished = true;
		} else if (speed < 0.0f && next <= 0.0f) {
			next = 0.0f;
			finished = true;
		}
	} else {
		if (next >= 1.0f || next < 0.0f) {
			wrapped = true;
			next = std::fmod(next, 1.0f);
			if (next < 0.0f) next += 1.0f;
		}
	}
	phase = next;
	return wrapped;
}

void emitDominantNotifies(const SkeletalClip& clip, float prevNorm,
						  float currNorm, bool wrapped, bool reversed,
						  bool includeStart,
						  std::vector<const AnimationNotify*>& scratch,
						  std::vector<AnimationNotifyEvent>* outNotifies) {
	if (!outNotifies || clip.getNotifies().empty()) return;
	const float dur = clip.getDuration();
	scratch.clear();
	clip.collectNotifies(prevNorm * dur, currNorm * dur, wrapped, reversed,
						 includeStart, scratch);
	for (const AnimationNotify* n : scratch) {
		outNotifies->push_back(AnimationNotifyEvent{
			clip.getName(), n->name, n->payload, n->time});
	}
}

/**
 * Computes barycentric coordinates (u, v, w) of point p with respect to
 * triangle (a, b, c). Returns false if the triangle is degenerate.
 */
bool triangleBarycentric(const Vector2& p, const Vector2& a, const Vector2& b,
						 const Vector2& c, float& u, float& v, float& w) {
	const Vector2 v0 = b - a;
	const Vector2 v1 = c - a;
	const Vector2 v2 = p - a;
	const float denom = v0.x * v1.y - v1.x * v0.y;
	if (std::fabs(denom) <= 1e-7f) return false;
	const float invDenom = 1.0f / denom;
	v = (v2.x * v1.y - v1.x * v2.y) * invDenom;
	w = (v0.x * v2.y - v2.x * v0.y) * invDenom;
	u = 1.0f - v - w;
	return true;
}

/** Closest point on segment [a, b] to p; returns t in [0, 1] and squared distance. */
float closestPointOnSegment(const Vector2& p, const Vector2& a,
							const Vector2& b, float& outT) {
	const Vector2 ab = b - a;
	const float lenSq = glm::dot(ab, ab);
	if (lenSq <= 1e-10f) {
		outT = 0.0f;
		const Vector2 d = p - a;
		return glm::dot(d, d);
	}
	outT = std::clamp(glm::dot(p - a, ab) / lenSq, 0.0f, 1.0f);
	const Vector2 closest = a + ab * outT;
	const Vector2 d = p - closest;
	return glm::dot(d, d);
}

}  // namespace

// ============================================================================
// BlendTree1D
// ============================================================================

BlendTree1D::BlendTree1D(std::string parameterName, bool syncNormalizedTime)
	: m_parameterName(std::move(parameterName)),
	  m_syncNormalizedTime(syncNormalizedTime) {}

void BlendTree1D::addClip(std::shared_ptr<const SkeletalClip> clip,
						  float threshold) {
	if (!clip) {
		throw std::invalid_argument("BlendTree1D::addClip: clip is null");
	}
	if (!std::isfinite(threshold)) {
		throw std::invalid_argument(
			"BlendTree1D::addClip: threshold must be finite");
	}
	RuntimeEntry entry{clip, threshold, PlaybackClock(clip->getDuration()),
					   ClipCursor{}};
	entry.clock.setLoopMode(m_loopMode);
	entry.clock.setSpeed(m_speed);
	const auto it = std::upper_bound(
		m_entries.begin(), m_entries.end(), threshold,
		[](float val, const RuntimeEntry& e) { return val < e.threshold; });
	m_entries.insert(it, std::move(entry));
}

void BlendTree1D::evaluate(const PoseEvalContext& ctx, Pose& outPose) {
	if (!ctx.skeleton) {
		throw std::invalid_argument("BlendTree1D::evaluate: skeleton is null");
	}
	if (m_entries.empty()) {
		outPose.joints = ctx.skeleton->getRestPose().joints;
		return;
	}

	float value = m_manualValue;
	if (ctx.parameters && ctx.parameters->hasParameter(m_parameterName)) {
		value = ctx.parameters->getFloat(m_parameterName, m_manualValue);
	}
	m_lastParameterValue = value;

	// Locate bracketing entries [idxA, idxB] and blend factor alpha.
	size_t idxA = 0;
	size_t idxB = 0;
	float alpha = 0.0f;

	if (m_entries.size() == 1 || value <= m_entries.front().threshold) {
		idxA = idxB = 0;
	} else if (value >= m_entries.back().threshold) {
		idxA = idxB = m_entries.size() - 1;
	} else {
		for (size_t i = 0; i + 1 < m_entries.size(); ++i) {
			if (value >= m_entries[i].threshold &&
				value <= m_entries[i + 1].threshold) {
				idxA = i;
				idxB = i + 1;
				const float span =
					m_entries[idxB].threshold - m_entries[idxA].threshold;
				alpha = span > 1e-6f
							? (value - m_entries[idxA].threshold) / span
							: 0.0f;
				break;
			}
		}
	}

	const bool includeStart = !m_hasTicked;
	m_hasTicked = true;

	if (m_syncNormalizedTime) {
		const float durA = m_entries[idxA].clip->getDuration();
		const float durB = m_entries[idxB].clip->getDuration();
		m_effectiveDuration = durA * (1.0f - alpha) + durB * alpha;
		const float prevNorm = m_normalizedTime;
		const bool wrapped = advanceNormalizedPhase(
			m_normalizedTime, ctx.deltaTime, m_speed, m_effectiveDuration,
			m_loopMode, m_finished);

		const auto sampleAtPhase = [&](size_t idx, Pose& dst) {
			dst.joints = ctx.skeleton->getRestPose().joints;
			const float t = m_normalizedTime * m_entries[idx].clip->getDuration();
			m_entries[idx].clip->sample(t, dst, m_entries[idx].cursor);
		};

		if (idxA == idxB || alpha <= 1e-5f) {
			sampleAtPhase(idxA, outPose);
		} else if (alpha >= 1.0f - 1e-5f) {
			sampleAtPhase(idxB, outPose);
		} else {
			sampleAtPhase(idxA, m_poseA);
			sampleAtPhase(idxB, m_poseB);
			blendPoses(m_poseA, m_poseB, alpha, outPose);
		}

		if (ctx.deltaTime > 0.0f || includeStart) {
			const size_t dominant = (alpha >= 0.5f) ? idxB : idxA;
			emitDominantNotifies(*m_entries[dominant].clip, prevNorm,
								 m_normalizedTime, wrapped, m_speed < 0.0f,
								 includeStart, m_notifyScratch,
								 ctx.outNotifies);
		}
	} else {
		// Unsynchronized: each active clip advances its own clock.
		const size_t dominant = (alpha >= 0.5f) ? idxB : idxA;
		const float prevTime = m_entries[dominant].clock.getTime();
		PlaybackEvents domEv{};

		auto stepAndSample = [&](size_t idx, Pose& dst) {
			m_entries[idx].clock.setLoopMode(m_loopMode);
			m_entries[idx].clock.setSpeed(m_speed);
			const PlaybackEvents ev = m_entries[idx].clock.advance(ctx.deltaTime);
			if (idx == dominant) domEv = ev;
			dst.joints = ctx.skeleton->getRestPose().joints;
			m_entries[idx].clip->sample(m_entries[idx].clock.getTime(), dst,
										m_entries[idx].cursor);
		};

		if (idxA == idxB || alpha <= 1e-5f) {
			stepAndSample(idxA, outPose);
		} else if (alpha >= 1.0f - 1e-5f) {
			stepAndSample(idxB, outPose);
		} else {
			stepAndSample(idxA, m_poseA);
			stepAndSample(idxB, m_poseB);
			blendPoses(m_poseA, m_poseB, alpha, outPose);
		}

		m_effectiveDuration = m_entries[dominant].clip->getDuration();
		m_normalizedTime = m_entries[dominant].clock.getNormalizedTime();
		m_finished = m_entries[dominant].clock.isFinished();

		if (ctx.outNotifies && (ctx.deltaTime > 0.0f || includeStart) &&
			!m_entries[dominant].clip->getNotifies().empty()) {
			m_notifyScratch.clear();
			m_entries[dominant].clip->collectNotifies(
				prevTime, m_entries[dominant].clock.getTime(), domEv.looped,
				m_speed < 0.0f, includeStart, m_notifyScratch);
			for (const AnimationNotify* n : m_notifyScratch) {
				ctx.outNotifies->push_back(AnimationNotifyEvent{
					m_entries[dominant].clip->getName(), n->name, n->payload,
					n->time});
			}
		}
	}
}

void BlendTree1D::reset() {
	m_normalizedTime = 0.0f;
	m_finished = false;
	m_hasTicked = false;
	for (RuntimeEntry& e : m_entries) {
		e.clock.restart();
		e.cursor.positions.clear();
	}
}

// ============================================================================
// BlendTree2D
// ============================================================================

BlendTree2D::BlendTree2D(std::string paramX, std::string paramY,
						 BlendTree2DMode mode, bool syncNormalizedTime)
	: m_paramX(std::move(paramX)),
	  m_paramY(std::move(paramY)),
	  m_mode(mode),
	  m_syncNormalizedTime(syncNormalizedTime) {}

void BlendTree2D::addClip(std::shared_ptr<const SkeletalClip> clip,
						  const Vector2& position) {
	if (!clip) {
		throw std::invalid_argument("BlendTree2D::addClip: clip is null");
	}
	if (!std::isfinite(position.x) || !std::isfinite(position.y)) {
		throw std::invalid_argument(
			"BlendTree2D::addClip: position components must be finite");
	}
	RuntimeEntry entry{clip, position, PlaybackClock(clip->getDuration()),
					   ClipCursor{}};
	entry.clock.setLoopMode(m_loopMode);
	entry.clock.setSpeed(m_speed);
	m_entries.push_back(std::move(entry));
}

void BlendTree2D::setIdwPower(float power) {
	if (!std::isfinite(power) || power <= 0.0f) {
		throw std::invalid_argument(
			"BlendTree2D::setIdwPower: power must be finite and > 0");
	}
	m_idwPower = power;
}

void BlendTree2D::computeInverseDistanceWeights(
	const Vector2& point, std::vector<float>& outWeights) const {
	const size_t n = m_entries.size();
	outWeights.assign(n, 0.0f);
	if (n == 0) return;
	if (n == 1) {
		outWeights[0] = 1.0f;
		return;
	}

	constexpr float kExactEps = 1e-5f;
	for (size_t i = 0; i < n; ++i) {
		const float dist = glm::length(point - m_entries[i].position);
		if (dist <= kExactEps) {
			outWeights[i] = 1.0f;
			return;
		}
	}

	float total = 0.0f;
	for (size_t i = 0; i < n; ++i) {
		const float dist = glm::length(point - m_entries[i].position);
		const float w = 1.0f / std::pow(dist, m_idwPower);
		outWeights[i] = w;
		total += w;
	}
	if (total > 1e-12f) {
		const float inv = 1.0f / total;
		for (float& w : outWeights) w *= inv;
	} else {
		outWeights[0] = 1.0f;
	}
}

void BlendTree2D::computeBarycentricWeights(
	const Vector2& point, std::vector<float>& outWeights) const {
	const size_t n = m_entries.size();
	outWeights.assign(n, 0.0f);
	if (n == 0) return;
	if (n == 1) {
		outWeights[0] = 1.0f;
		return;
	}
	if (n == 2) {
		float t = 0.0f;
		closestPointOnSegment(point, m_entries[0].position,
							  m_entries[1].position, t);
		outWeights[0] = 1.0f - t;
		outWeights[1] = t;
		return;
	}

	// Exact-match shortcut.
	constexpr float kExactEps = 1e-5f;
	for (size_t i = 0; i < n; ++i) {
		if (glm::length(point - m_entries[i].position) <= kExactEps) {
			outWeights[i] = 1.0f;
			return;
		}
	}

	// Search for the smallest-perimeter triangle containing the point.
	constexpr float kBaryEps = -1e-4f;
	bool foundContaining = false;
	float bestPerimeter = std::numeric_limits<float>::max();
	size_t bestI = 0, bestJ = 1, bestK = 2;
	float bestU = 1.0f, bestV = 0.0f, bestW = 0.0f;

	for (size_t i = 0; i < n; ++i) {
		for (size_t j = i + 1; j < n; ++j) {
			for (size_t k = j + 1; k < n; ++k) {
				float u = 0.0f, v = 0.0f, w = 0.0f;
				if (!triangleBarycentric(point, m_entries[i].position,
										 m_entries[j].position,
										 m_entries[k].position, u, v, w)) {
					continue;
				}
				if (u >= kBaryEps && v >= kBaryEps && w >= kBaryEps) {
					const float perim =
						glm::length(m_entries[i].position -
									m_entries[j].position) +
						glm::length(m_entries[j].position -
									m_entries[k].position) +
						glm::length(m_entries[k].position -
									m_entries[i].position);
					if (perim < bestPerimeter) {
						foundContaining = true;
						bestPerimeter = perim;
						bestI = i;
						bestJ = j;
						bestK = k;
						bestU = std::max(0.0f, u);
						bestV = std::max(0.0f, v);
						bestW = std::max(0.0f, w);
					}
				}
			}
		}
	}

	if (foundContaining) {
		const float sum = bestU + bestV + bestW;
		const float inv = sum > 1e-8f ? (1.0f / sum) : 1.0f;
		outWeights[bestI] = bestU * inv;
		outWeights[bestJ] = bestV * inv;
		outWeights[bestK] = bestW * inv;
		return;
	}

	// Outside convex hull: project onto the closest edge between any pair.
	float bestDistSq = std::numeric_limits<float>::max();
	size_t edgeA = 0, edgeB = 1;
	float edgeT = 0.0f;
	for (size_t i = 0; i < n; ++i) {
		for (size_t j = i + 1; j < n; ++j) {
			float t = 0.0f;
			const float dSq = closestPointOnSegment(
				point, m_entries[i].position, m_entries[j].position, t);
			if (dSq < bestDistSq) {
				bestDistSq = dSq;
				edgeA = i;
				edgeB = j;
				edgeT = t;
			}
		}
	}
	outWeights[edgeA] = 1.0f - edgeT;
	outWeights[edgeB] = edgeT;
}

void BlendTree2D::computeWeights(const Vector2& point,
								 std::vector<float>& outWeights) const {
	if (m_mode == BlendTree2DMode::Barycentric) {
		computeBarycentricWeights(point, outWeights);
	} else {
		computeInverseDistanceWeights(point, outWeights);
	}
}

void BlendTree2D::evaluate(const PoseEvalContext& ctx, Pose& outPose) {
	if (!ctx.skeleton) {
		throw std::invalid_argument("BlendTree2D::evaluate: skeleton is null");
	}
	if (m_entries.empty()) {
		outPose.joints = ctx.skeleton->getRestPose().joints;
		return;
	}

	Vector2 point = m_manualValue;
	if (ctx.parameters) {
		if (ctx.parameters->hasParameter(m_paramX)) {
			point.x = ctx.parameters->getFloat(m_paramX, m_manualValue.x);
		}
		if (ctx.parameters->hasParameter(m_paramY)) {
			point.y = ctx.parameters->getFloat(m_paramY, m_manualValue.y);
		}
	}
	m_lastParameterValue = point;

	computeWeights(point, m_weights);

	// Identify dominant clip and weighted duration.
	size_t dominant = 0;
	float maxW = -1.0f;
	float weightedDuration = 0.0f;
	for (size_t i = 0; i < m_entries.size(); ++i) {
		if (m_weights[i] > maxW) {
			maxW = m_weights[i];
			dominant = i;
		}
		weightedDuration += m_weights[i] * m_entries[i].clip->getDuration();
	}

	const bool includeStart = !m_hasTicked;
	m_hasTicked = true;

	const float prevNorm = m_normalizedTime;
	bool wrapped = false;
	if (m_syncNormalizedTime) {
		m_effectiveDuration = weightedDuration;
		wrapped = advanceNormalizedPhase(m_normalizedTime, ctx.deltaTime,
										 m_speed, m_effectiveDuration,
										 m_loopMode, m_finished);
	}

	// Ensure scratch storage is warm (no per-frame alloc after first tick).
	if (m_scratchPoses.size() < m_entries.size()) {
		m_scratchPoses.resize(m_entries.size());
	}
	m_activePosePtrs.clear();
	m_activeWeights.clear();

	constexpr float kWeightCutoff = 1e-4f;
	float domPrevTime = m_entries[dominant].clock.getTime();
	PlaybackEvents domEv{};

	for (size_t i = 0; i < m_entries.size(); ++i) {
		const float w = m_weights[i];
		if (w <= kWeightCutoff) continue;

		Pose& dst = m_scratchPoses[i];
		dst.joints = ctx.skeleton->getRestPose().joints;
		if (m_syncNormalizedTime) {
			const float t = m_normalizedTime * m_entries[i].clip->getDuration();
			m_entries[i].clip->sample(t, dst, m_entries[i].cursor);
		} else {
			m_entries[i].clock.setLoopMode(m_loopMode);
			m_entries[i].clock.setSpeed(m_speed);
			const PlaybackEvents ev = m_entries[i].clock.advance(ctx.deltaTime);
			if (i == dominant) domEv = ev;
			m_entries[i].clip->sample(m_entries[i].clock.getTime(), dst,
									  m_entries[i].cursor);
		}
		m_activePosePtrs.push_back(&dst);
		m_activeWeights.push_back(w);
	}

	if (m_activePosePtrs.empty()) {
		outPose.joints = ctx.skeleton->getRestPose().joints;
	} else if (m_activePosePtrs.size() == 1) {
		outPose.joints = m_activePosePtrs[0]->joints;
	} else {
		blendPosesWeighted(m_activePosePtrs.data(), m_activeWeights.data(),
						   m_activePosePtrs.size(), outPose);
	}

	if (m_syncNormalizedTime) {
		if (ctx.deltaTime > 0.0f || includeStart) {
			emitDominantNotifies(*m_entries[dominant].clip, prevNorm,
								 m_normalizedTime, wrapped, m_speed < 0.0f,
								 includeStart, m_notifyScratch,
								 ctx.outNotifies);
		}
	} else {
		m_effectiveDuration = m_entries[dominant].clip->getDuration();
		m_normalizedTime = m_entries[dominant].clock.getNormalizedTime();
		m_finished = m_entries[dominant].clock.isFinished();
		if (ctx.outNotifies && (ctx.deltaTime > 0.0f || includeStart) &&
			!m_entries[dominant].clip->getNotifies().empty()) {
			m_notifyScratch.clear();
			m_entries[dominant].clip->collectNotifies(
				domPrevTime, m_entries[dominant].clock.getTime(), domEv.looped,
				m_speed < 0.0f, includeStart, m_notifyScratch);
			for (const AnimationNotify* n : m_notifyScratch) {
				ctx.outNotifies->push_back(AnimationNotifyEvent{
					m_entries[dominant].clip->getName(), n->name, n->payload,
					n->time});
			}
		}
	}
}

void BlendTree2D::reset() {
	m_normalizedTime = 0.0f;
	m_finished = false;
	m_hasTicked = false;
	for (RuntimeEntry& e : m_entries) {
		e.clock.restart();
		e.cursor.positions.clear();
	}
}

}  // namespace bird
