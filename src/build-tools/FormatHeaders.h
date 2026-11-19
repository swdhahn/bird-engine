#ifndef H_BIRD_FILE_FORMATS
#define H_BIRD_FILE_FORMATS

#include <cstdint>
#pragma pack(push, 1)

namespace bird::format {

// #####################################
// ############### Mesh ################
// #####################################

/*
 * Number of meshes
 * Material index // Mesh 0 start
 * Number of extra buffer data points // vertices, indices, normals,
 * texCoords, etc.  This will be 0 if it's just the standard vertices to
 * texCoords Number of indices Index 0 Index 1
 * ...
 * Number of vertices
 * Number of floats per vertex
 * Vertex 0
 * Vertex 1
 * ...
 * Number of normals
 * number of floats per normal
 * normal 0
 * normal 1
 * ...
 * Number of texCoords
 * Number of floats per texCoord
 * texCoord 0
 * texCoord 1
 * ...
 * Number of indices // Mesh 1 start
 * ...
 */

// not done
struct MeshFileFormat {
	uint16_t numberOfMeshes;
};
struct SingleMeshFormat {
	uint16_t materialIndex;
	uint8_t vboCount;  // indices, vertices, normals, texCoords don't count
};
struct MeshVBOFormat {
	uint32_t vboCount;
	uint8_t floatsPer;
};

// #####################################
// ############## SHADERS ##############
// #####################################

/*
 * One file per shader stage (e.g. ui.vert.bird):
 *
 * ShaderFileHeader
 * { ShaderVariantHeader, uint8_t bytes[byteSize] }  x variantCount
 * { ShaderResourceInfo, char name[nameSize] }       x resourceCount
 * { PushConstantMemberInfo, char name[nameSize] }   x pushConstantMemberCount
 * { ShaderDependencyInfo, char path[pathSize] }     x dependencyCount
 *
 * Dependencies are every file pulled in through #include (transitively),
 * each with the content hash it had at bake time. BuildTools re-bakes when
 * the source or any dependency hash changes.
 * Names are stored without a terminator and match the identifiers emitted
 * in the GLSL variants, so the OpenGL runtime can resolve them by name.
 */
constexpr char SHADER_FILE_MAGIC[4] = {'B', 'S', 'H', 'D'};
constexpr uint32_t SHADER_FORMAT_VERSION = 2;

enum ShaderStageKind : uint8_t {
	SHADER_STAGE_VERTEX,
	SHADER_STAGE_FRAGMENT,
	SHADER_STAGE_GEOMETRY,
	SHADER_STAGE_COMPUTE,
	SHADER_STAGE_TESS_CONTROL,
	SHADER_STAGE_TESS_EVALUATION
};

enum ShaderTarget : uint8_t {
	SHADER_TARGET_SPIRV,	 // Vulkan SPIR-V words
	SHADER_TARGET_GLSL_460,	 // OpenGL 4.6 source, bindings in layout()
	SHADER_TARGET_GLSL_410,	 // OpenGL 4.1 source, bindings via resources
	SHADER_TARGET_MSL,		 // reserved
	SHADER_TARGET_HLSL		 // reserved
};

enum ShaderResourceKind : uint8_t {
	SHADER_RESOURCE_UNIFORM_BUFFER,
	SHADER_RESOURCE_SAMPLER
};

enum PushConstantType : uint8_t {
	PUSH_CONSTANT_FLOAT,
	PUSH_CONSTANT_VEC2,
	PUSH_CONSTANT_VEC3,
	PUSH_CONSTANT_VEC4,
	PUSH_CONSTANT_INT,
	PUSH_CONSTANT_IVEC2,
	PUSH_CONSTANT_IVEC3,
	PUSH_CONSTANT_IVEC4,
	PUSH_CONSTANT_UINT,
	PUSH_CONSTANT_UVEC2,
	PUSH_CONSTANT_UVEC3,
	PUSH_CONSTANT_UVEC4,
	PUSH_CONSTANT_MAT3,	 // std430: three vec3 columns at a 16-byte stride
	PUSH_CONSTANT_MAT4
};

struct ShaderFileHeader {
	char magic[4];
	uint32_t version;
	uint8_t stage;		  // ShaderStageKind
	uint64_t sourceHash;  // FNV-1a of the GLSL source
	uint8_t variantCount;
	uint16_t resourceCount;
	uint16_t pushConstantSize;	// bytes; 0 when the stage has none
	uint16_t pushConstantMemberCount;
	uint16_t dependencyCount;
};
struct ShaderVariantHeader {
	uint8_t target;	 // ShaderTarget
	uint32_t byteSize;
};
struct ShaderResourceInfo {
	uint8_t kind;  // ShaderResourceKind
	uint8_t binding;
	uint16_t nameSize;
};
struct PushConstantMemberInfo {
	uint16_t offset;
	uint8_t type;  // PushConstantType
	uint16_t nameSize;
};
struct ShaderDependencyInfo {
	uint64_t contentHash;  // FNV-1a of the included file
	uint16_t pathSize;
};

// #####################################
// ############### FONTS ###############
// #####################################

/*
 * FontHeader
 * FontGlyph[glyphCount]                  // codepoints firstCodepoint..
 * uint8_t atlas[atlasWidth * atlasHeight] // single-channel SDF
 *
 * The atlas is a signed distance field: 128 lies on the glyph outline,
 * larger values are inside. Glyph rectangles include the SDF padding, so
 * the same quads serve every display size.
 */
constexpr uint32_t FONT_FORMAT_VERSION = 2;

struct FontHeader {
	uint32_t version;
	float bakedPixelHeight;	 // glyph metrics below are in these pixels
	float ascent;			 // baseline to top, positive
	float descent;			 // baseline to bottom, negative
	float lineGap;
	uint32_t atlasWidth;
	uint32_t atlasHeight;
	uint32_t firstCodepoint;
	uint32_t glyphCount;
	uint16_t whiteTexelX;  // fully inside texel (255) for untextured quads
	uint16_t whiteTexelY;
};
struct FontGlyph {
	uint16_t atlasX0, atlasY0, atlasX1, atlasY1;  // atlas pixel rectangle
	float offsetX, offsetY;	 // quad top-left relative to pen on baseline
	float advance;			 // horizontal pen advance
};

// #####################################
// #####################################

#pragma pack(pop)

}  // namespace bird::format

#endif	// H_BIRD_FILE_FORMATS
