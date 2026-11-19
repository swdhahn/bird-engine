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
 * ShaderHeader
 * ShaderGPHeader[]
 * ShaderStage[]
 * ShaderUniformInfo[]
 * uint8_t* uniformNames;
 * uint8_t* shaderData;
 */
struct ShaderHeader {
	uint8_t api_count;	// How many apis are compiled in this file
};
struct ShaderGPHeader {
	uint8_t type;				 // shader pipeline opengl, vulkan...
	uint8_t shader_stage_count;	 // how many stages are there
};
struct ShaderStage {
	uint8_t stage;	// vert, frag, ...
	uint8_t uniformCount;
	uint32_t dataSize;	// blob size.
};
struct ShaderUniformInfo {
	uint8_t bindingLocation;
	uint8_t nameSize;  // size of string with \0
	uint8_t type;	   // ShaderUniformTypes
};
enum ShaderUniformTypes { UNIFORM, TEXTURE, UBO };

// #####################################
// ############### FONTS ###############
// #####################################

/*
 * FontHeader
 * FontGlyph[glyphCount]                  // codepoints firstCodepoint..
 * uint8_t atlas[atlasWidth * atlasHeight] // single-channel coverage
 */
constexpr uint32_t FONT_FORMAT_VERSION = 1;

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
	uint16_t whiteTexelX;  // fully covered texel for untextured quads
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
