#include "SkeletalAsset.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace bird {

namespace {

constexpr char kMagic[4] = {'B', 'S', 'K', 'L'};
/** Guards against absurd counts from corrupt files before allocating. */
constexpr uint32_t kMaxClips = 4096;
constexpr uint32_t kMaxKeysPerChannel = 1u << 22;
constexpr uint32_t kMaxSkinVertices = 1u << 26;

// ----------------------------------------------------------------- writer

class Writer {
   public:
	explicit Writer(std::ostream& out) : m_out(out) {}

	template <typename T>
	void pod(const T& value) {
		static_assert(std::is_trivially_copyable_v<T>);
		m_out.write(reinterpret_cast<const char*>(&value), sizeof(T));
	}
	void floats(const float* values, size_t count) {
		m_out.write(reinterpret_cast<const char*>(values),
					static_cast<std::streamsize>(count * sizeof(float)));
	}
	void string(const std::string& text) {
		if (text.size() > std::numeric_limits<uint16_t>::max()) {
			throw std::runtime_error("SkeletalAsset: name too long: " + text);
		}
		pod(static_cast<uint16_t>(text.size()));
		m_out.write(text.data(), static_cast<std::streamsize>(text.size()));
	}
	void value(float v) { pod(v); }
	void value(const Vector3& v) { floats(&v.x, 3); }
	void value(const Quaternion& q) {
		const float xyzw[4] = {q.x, q.y, q.z, q.w};
		floats(xyzw, 4);
	}

	template <typename T>
	void curve(const Curve<T>& c) {
		pod(static_cast<uint8_t>(c.getInterpolation()));
		pod(static_cast<uint32_t>(c.getKeyCount()));
		for (const Keyframe<T>& key : c.getKeys()) {
			pod(key.time);
			value(key.value);
			value(key.inTangent);
			value(key.outTangent);
		}
	}

   private:
	std::ostream& m_out;
};

// ----------------------------------------------------------------- reader

class Reader {
   public:
	explicit Reader(std::istream& in) : m_in(in) {}

	template <typename T>
	T pod() {
		static_assert(std::is_trivially_copyable_v<T>);
		T value;
		m_in.read(reinterpret_cast<char*>(&value), sizeof(T));
		if (!m_in) fail("unexpected end of file");
		return value;
	}
	float finite() {
		const float v = pod<float>();
		if (!std::isfinite(v)) fail("non-finite number");
		return v;
	}
	void floats(float* values, size_t count) {
		for (size_t i = 0; i < count; ++i) values[i] = finite();
	}
	std::string string() {
		const uint16_t length = pod<uint16_t>();
		std::string text(length, '\0');
		m_in.read(text.data(), length);
		if (!m_in) fail("unexpected end of file in a name");
		return text;
	}
	void value(float& v) { v = finite(); }
	void value(Vector3& v) { floats(&v.x, 3); }
	void value(Quaternion& q) {
		float xyzw[4];
		floats(xyzw, 4);
		q = Quaternion(xyzw[3], xyzw[0], xyzw[1], xyzw[2]);
	}

	template <typename T>
	Curve<T> curve() {
		const uint8_t mode = pod<uint8_t>();
		if (mode > static_cast<uint8_t>(CurveInterpolation::CubicHermite)) {
			fail("unknown interpolation mode");
		}
		const uint32_t count = pod<uint32_t>();
		if (count > kMaxKeysPerChannel) fail("implausible key count");
		std::vector<Keyframe<T>> keys(count);
		for (Keyframe<T>& key : keys) {
			key.time = finite();
			value(key.value);
			value(key.inTangent);
			value(key.outTangent);
		}
		Curve<T> result(static_cast<CurveInterpolation>(mode));
		try {
			result.setKeys(std::move(keys));
		} catch (const std::invalid_argument& e) {
			fail(e.what());
		}
		return result;
	}

	[[noreturn]] static void fail(const std::string& why) {
		throw std::runtime_error("SkeletalAsset: malformed file (" + why + ")");
	}

   private:
	std::istream& m_in;
};

}  // namespace

std::shared_ptr<SkeletalClip> SkeletalAsset::findClip(
	const std::string& name) const {
	for (const auto& clip : clips) {
		if (clip && clip->getName() == name) return clip;
	}
	return nullptr;
}

void writeSkeletalAsset(std::ostream& out, const SkeletalAsset& asset) {
	if (!asset.skeleton) {
		throw std::invalid_argument("writeSkeletalAsset: skeleton is null");
	}
	const Skeleton& skeleton = *asset.skeleton;
	Writer w(out);
	out.write(kMagic, sizeof(kMagic));
	w.pod(SKELETAL_ASSET_VERSION);
	w.pod(static_cast<uint32_t>(skeleton.getJointCount()));
	w.pod(static_cast<uint32_t>(asset.clips.size()));
	for (int32_t i = 0; i < skeleton.getJointCount(); ++i) {
		const JointTransform& rest = skeleton.getRestPose().joints[i];
		w.string(skeleton.getJointName(i));
		w.pod(skeleton.getParent(i));
		w.floats(&skeleton.getInverseBindMatrices()[i][0][0], 16);
		w.value(rest.translation);
		w.value(rest.rotation);
		w.value(rest.scale);
	}
	for (const auto& clip : asset.clips) {
		if (!clip) throw std::invalid_argument("writeSkeletalAsset: null clip");
		w.string(clip->getName());
		w.pod(clip->getDuration());
		w.pod(static_cast<uint32_t>(clip->getTracks().size()));
		for (const JointTrack& track : clip->getTracks()) {
			w.pod(track.joint);
			w.curve(track.translation);
			w.curve(track.rotation);
			w.curve(track.scale);
		}
	}
	w.pod(static_cast<uint32_t>(asset.skins.size()));
	for (const SkinStream& skin : asset.skins) {
		if (skin.joints.size() != skin.weights.size()) {
			throw std::invalid_argument(
				"writeSkeletalAsset: skin joint/weight counts differ");
		}
		w.pod(skin.meshIndex);
		w.pod(static_cast<uint32_t>(skin.joints.size()));
		for (const auto& joints : skin.joints) w.pod(joints);
		for (const auto& weights : skin.weights) w.floats(weights.data(), 4);
	}
	if (!out) throw std::runtime_error("writeSkeletalAsset: write failed");
}

SkeletalAsset readSkeletalAsset(std::istream& in) {
	Reader r(in);
	char magic[4];
	in.read(magic, sizeof(magic));
	if (!in || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0) {
		Reader::fail("bad magic");
	}
	const uint32_t version = r.pod<uint32_t>();
	if (version != SKELETAL_ASSET_VERSION) {
		Reader::fail("unsupported version " + std::to_string(version));
	}
	const uint32_t jointCount = r.pod<uint32_t>();
	const uint32_t clipCount = r.pod<uint32_t>();
	if (jointCount > Skeleton::kMaxJoints) Reader::fail("too many joints");
	if (clipCount > kMaxClips) Reader::fail("too many clips");

	SkeletalAsset asset;
	asset.skeleton = std::make_shared<Skeleton>();
	for (uint32_t i = 0; i < jointCount; ++i) {
		const std::string name = r.string();
		const int32_t parent = r.pod<int32_t>();
		Matrix4 inverseBind;
		r.floats(&inverseBind[0][0], 16);
		JointTransform rest;
		r.value(rest.translation);
		r.value(rest.rotation);
		r.value(rest.scale);
		try {
			asset.skeleton->addJoint(name, parent, inverseBind, rest);
		} catch (const std::invalid_argument& e) {
			Reader::fail(e.what());
		}
	}
	for (uint32_t c = 0; c < clipCount; ++c) {
		std::string name = r.string();
		const float duration = r.finite();
		const uint32_t trackCount = r.pod<uint32_t>();
		if (trackCount > jointCount) Reader::fail("more tracks than joints");
		std::shared_ptr<SkeletalClip> clip;
		try {
			clip = std::make_shared<SkeletalClip>(std::move(name), duration);
		} catch (const std::invalid_argument& e) {
			Reader::fail(e.what());
		}
		for (uint32_t t = 0; t < trackCount; ++t) {
			JointTrack track;
			track.joint = r.pod<int32_t>();
			if (track.joint < 0 || static_cast<uint32_t>(track.joint) >= jointCount) {
				Reader::fail("track joint out of range");
			}
			track.translation = r.curve<Vector3>();
			track.rotation = r.curve<Quaternion>();
			track.scale = r.curve<Vector3>();
			try {
				clip->addTrack(std::move(track));
			} catch (const std::invalid_argument& e) {
				Reader::fail(e.what());
			}
		}
		asset.clips.push_back(std::move(clip));
	}
	const uint32_t skinCount = r.pod<uint32_t>();
	if (skinCount > kMaxClips) Reader::fail("too many skins");
	for (uint32_t s = 0; s < skinCount; ++s) {
		SkinStream skin;
		skin.meshIndex = r.pod<uint32_t>();
		const uint32_t vertexCount = r.pod<uint32_t>();
		if (vertexCount > kMaxSkinVertices) Reader::fail("implausible skin");
		skin.joints.resize(vertexCount);
		skin.weights.resize(vertexCount);
		for (auto& joints : skin.joints) {
			joints = r.pod<std::array<uint16_t, 4>>();
			for (uint16_t j : joints) {
				if (j >= jointCount) Reader::fail("skin joint out of range");
			}
		}
		for (auto& weights : skin.weights) r.floats(weights.data(), 4);
		asset.skins.push_back(std::move(skin));
	}
	return asset;
}

void saveSkeletalAsset(const std::string& path, const SkeletalAsset& asset) {
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file) throw std::runtime_error("saveSkeletalAsset: cannot open " + path);
	writeSkeletalAsset(file, asset);
}

SkeletalAsset loadSkeletalAsset(const std::string& path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) throw std::runtime_error("loadSkeletalAsset: cannot open " + path);
	return readSkeletalAsset(file);
}

}  // namespace bird
