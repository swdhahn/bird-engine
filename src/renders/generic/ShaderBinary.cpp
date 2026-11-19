#include "ShaderBinary.h"

#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>

namespace bird {

namespace {

/** Appends trivially copyable values and raw byte ranges. */
class ByteWriter {
   public:
	template <typename T>
	void put(const T& value) {
		const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
		m_bytes.insert(m_bytes.end(), bytes, bytes + sizeof(T));
	}
	void putBytes(const void* data, size_t size) {
		const auto* bytes = static_cast<const uint8_t*>(data);
		m_bytes.insert(m_bytes.end(), bytes, bytes + size);
	}
	std::vector<uint8_t> take() { return std::move(m_bytes); }

   private:
	std::vector<uint8_t> m_bytes;
};

/** Bounds-checked sequential reader; throws instead of over-reading. */
class ByteReader {
   public:
	explicit ByteReader(const std::vector<uint8_t>& bytes) : m_bytes(bytes) {}

	template <typename T>
	T get() {
		T value;
		std::memcpy(&value, take(sizeof(T)), sizeof(T));
		return value;
	}
	std::vector<uint8_t> getBytes(size_t size) {
		const uint8_t* start = take(size);
		return std::vector<uint8_t>(start, start + size);
	}
	std::string getString(size_t size) {
		const uint8_t* start = take(size);
		return std::string(reinterpret_cast<const char*>(start), size);
	}
	bool atEnd() const { return m_position == m_bytes.size(); }

   private:
	const uint8_t* take(size_t size) {
		if (size > m_bytes.size() - m_position) {
			throw ShaderBinaryError("Shader binary is truncated");
		}
		const uint8_t* start = m_bytes.data() + m_position;
		m_position += size;
		return start;
	}

	const std::vector<uint8_t>& m_bytes;
	size_t m_position = 0;
};

template <typename T>
T checkedNarrow(size_t value, const char* what) {
	if (value > std::numeric_limits<T>::max()) {
		throw ShaderBinaryError(std::string("Shader binary field too large: ") +
								what);
	}
	return static_cast<T>(value);
}

void validateHeader(const format::ShaderFileHeader& header) {
	if (std::memcmp(header.magic, format::SHADER_FILE_MAGIC,
					sizeof(header.magic)) != 0) {
		throw ShaderBinaryError("Not a Bird shader binary (bad magic)");
	}
	if (header.version != format::SHADER_FORMAT_VERSION) {
		throw ShaderBinaryError(
			"Shader binary version mismatch; re-run BuildTools");
	}
}

}  // namespace

const std::vector<uint8_t>& ShaderBinary::variant(
	format::ShaderTarget target) const {
	auto it = variants.find(target);
	if (it == variants.end()) {
		throw ShaderBinaryError("Shader variant not baked: target " +
								std::to_string(static_cast<int>(target)));
	}
	return it->second;
}

std::string ShaderBinary::variantText(format::ShaderTarget target) const {
	const std::vector<uint8_t>& bytes = variant(target);
	return std::string(bytes.begin(), bytes.end());
}

uint64_t hashShaderSource(const std::string& source) {
	uint64_t hash = 14695981039346656037ull;
	for (unsigned char c : source) {
		hash ^= c;
		hash *= 1099511628211ull;
	}
	return hash;
}

uint32_t pushConstantTypeSize(format::PushConstantType type) {
	switch (type) {
		case format::PUSH_CONSTANT_FLOAT:
		case format::PUSH_CONSTANT_INT:
		case format::PUSH_CONSTANT_UINT:
			return 4;
		case format::PUSH_CONSTANT_VEC2:
		case format::PUSH_CONSTANT_IVEC2:
		case format::PUSH_CONSTANT_UVEC2:
			return 8;
		case format::PUSH_CONSTANT_VEC3:
		case format::PUSH_CONSTANT_IVEC3:
		case format::PUSH_CONSTANT_UVEC3:
			return 12;
		case format::PUSH_CONSTANT_VEC4:
		case format::PUSH_CONSTANT_IVEC4:
		case format::PUSH_CONSTANT_UVEC4:
			return 16;
		case format::PUSH_CONSTANT_MAT3:
			return 44;	// two padded columns of 16 bytes plus one vec3
		case format::PUSH_CONSTANT_MAT4:
			return 64;
	}
	throw ShaderBinaryError("Unknown push constant type");
}

std::vector<uint8_t> serializeShaderBinary(const ShaderBinary& binary) {
	format::ShaderFileHeader header{};
	std::memcpy(header.magic, format::SHADER_FILE_MAGIC, sizeof(header.magic));
	header.version = format::SHADER_FORMAT_VERSION;
	header.stage = binary.stage;
	header.sourceHash = binary.sourceHash;
	header.variantCount =
		checkedNarrow<uint8_t>(binary.variants.size(), "variantCount");
	header.resourceCount =
		checkedNarrow<uint16_t>(binary.resources.size(), "resourceCount");
	header.pushConstantSize =
		checkedNarrow<uint16_t>(binary.pushConstantSize, "pushConstantSize");
	header.pushConstantMemberCount = checkedNarrow<uint16_t>(
		binary.pushConstants.size(), "pushConstantMemberCount");
	header.dependencyCount =
		checkedNarrow<uint16_t>(binary.dependencies.size(), "dependencyCount");

	ByteWriter writer;
	writer.put(header);
	for (const auto& [target, bytes] : binary.variants) {
		writer.put(format::ShaderVariantHeader{
			target, checkedNarrow<uint32_t>(bytes.size(), "variant size")});
		writer.putBytes(bytes.data(), bytes.size());
	}
	for (const ShaderResource& resource : binary.resources) {
		writer.put(format::ShaderResourceInfo{
			resource.kind,
			checkedNarrow<uint8_t>(resource.binding, "binding"),
			checkedNarrow<uint16_t>(resource.name.size(), "name")});
		writer.putBytes(resource.name.data(), resource.name.size());
	}
	for (const PushConstantMember& member : binary.pushConstants) {
		writer.put(format::PushConstantMemberInfo{
			checkedNarrow<uint16_t>(member.offset, "offset"), member.type,
			checkedNarrow<uint16_t>(member.name.size(), "name")});
		writer.putBytes(member.name.data(), member.name.size());
	}
	for (const ShaderDependency& dependency : binary.dependencies) {
		writer.put(format::ShaderDependencyInfo{
			dependency.contentHash,
			checkedNarrow<uint16_t>(dependency.path.size(), "path")});
		writer.putBytes(dependency.path.data(), dependency.path.size());
	}
	return writer.take();
}

ShaderBinary parseShaderBinary(const std::vector<uint8_t>& bytes) {
	ByteReader reader(bytes);
	const auto header = reader.get<format::ShaderFileHeader>();
	validateHeader(header);

	ShaderBinary binary;
	binary.stage = static_cast<format::ShaderStageKind>(header.stage);
	binary.sourceHash = header.sourceHash;
	binary.pushConstantSize = header.pushConstantSize;
	for (uint8_t i = 0; i < header.variantCount; i++) {
		const auto info = reader.get<format::ShaderVariantHeader>();
		binary.variants[static_cast<format::ShaderTarget>(info.target)] =
			reader.getBytes(info.byteSize);
	}
	for (uint16_t i = 0; i < header.resourceCount; i++) {
		const auto info = reader.get<format::ShaderResourceInfo>();
		binary.resources.push_back(
			{static_cast<format::ShaderResourceKind>(info.kind), info.binding,
			 reader.getString(info.nameSize)});
	}
	for (uint16_t i = 0; i < header.pushConstantMemberCount; i++) {
		const auto info = reader.get<format::PushConstantMemberInfo>();
		binary.pushConstants.push_back(
			{reader.getString(info.nameSize), info.offset,
			 static_cast<format::PushConstantType>(info.type)});
	}
	for (uint16_t i = 0; i < header.dependencyCount; i++) {
		const auto info = reader.get<format::ShaderDependencyInfo>();
		binary.dependencies.push_back(
			{reader.getString(info.pathSize), info.contentHash});
	}
	if (!reader.atEnd()) {
		throw ShaderBinaryError("Shader binary has trailing bytes");
	}
	return binary;
}

ShaderBinary readShaderBinaryFile(const std::string& path) {
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		throw ShaderBinaryError("Missing shader binary " + path +
								" (run BuildTools)");
	}
	std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)),
							   std::istreambuf_iterator<char>());
	try {
		return parseShaderBinary(bytes);
	} catch (const ShaderBinaryError& error) {
		throw ShaderBinaryError(path + ": " + error.what());
	}
}

void writeShaderBinaryFile(const std::string& path,
						   const ShaderBinary& binary) {
	const std::vector<uint8_t> bytes = serializeShaderBinary(binary);
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	if (!file) {
		throw ShaderBinaryError("Unable to write shader binary " + path);
	}
}

}  // namespace bird
