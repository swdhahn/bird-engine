#ifndef BIRD_GPU_LAYOUTS_H
#define BIRD_GPU_LAYOUTS_H

#include <cstddef>
#include <cstdint>

#include "../../util/Math.h"

/**
 * CPU mirrors of every uniform block and push-constant block shared with the
 * engine shaders. Each struct follows std140 rules; the static_asserts pin the
 * offsets so that a layout drift fails at compile time rather than on screen.
 *
 * Binding conventions (descriptor set 0):
 *   UBO 0  Global          camera matrices, screen, environment
 *   UBO 1  Material        per-material PBR parameters
 *   UBO 2  Lights          forward light list / directional lights
 *   UBO 3  Shadows         shadow matrices and atlas rectangles
 *   UBO 4  Skinning        joint palette of the skinned draw
 *   Samplers 0..4          material texture slots (TextureSlot order)
 *   Samplers 5..           pass inputs (G-buffer, shadow atlases)
 */
namespace bird {

constexpr uint32_t UBO_BINDING_GLOBAL = 0;
constexpr uint32_t UBO_BINDING_MATERIAL = 1;
constexpr uint32_t UBO_BINDING_LIGHTS = 2;
constexpr uint32_t UBO_BINDING_SHADOWS = 3;
constexpr uint32_t UBO_BINDING_SKINNING = 4;

constexpr uint32_t SAMPLER_BINDING_GBUFFER_ALBEDO = 5;
constexpr uint32_t SAMPLER_BINDING_GBUFFER_NORMAL = 6;
constexpr uint32_t SAMPLER_BINDING_GBUFFER_DEPTH = 7;
constexpr uint32_t SAMPLER_BINDING_SHADOW_DIRECTIONAL = 8;
constexpr uint32_t SAMPLER_BINDING_SHADOW_LOCAL = 9;
constexpr uint32_t SAMPLER_BINDING_IBL_IRRADIANCE = 10;
constexpr uint32_t SAMPLER_BINDING_IBL_PREFILTER = 11;
constexpr uint32_t SAMPLER_BINDING_IBL_BRDF_LUT = 12;

/** Number of roughness mip levels in the specular prefiltered cubemap. */
constexpr uint32_t IBL_PREFILTER_MIP_LEVELS = 5;

/**
 * Camera and frame constants (binding 0). The first three members form the
 * legacy prefix still declared by the UI and debug-line shaders.
 */
struct GlobalUBOData {
	Matrix4 perspective;
	Matrix4 view;
	Vector4 viewPos;
	Matrix4 inverseProjection;
	Matrix4 inverseView;
	/** x = width, y = height, z = 1 / width, w = 1 / height. */
	Vector4 screenSize;
	/** rgb = ambient color premultiplied by intensity, w = exposure. */
	Vector4 ambientAndExposure;
	/** x = near, y = far, z = debug view mode, w = IBL intensity (0 = disabled). */
	Vector4 cameraParams;
};
static_assert(offsetof(GlobalUBOData, viewPos) == 128, "std140 drift");
static_assert(offsetof(GlobalUBOData, inverseProjection) == 144, "std140 drift");
static_assert(offsetof(GlobalUBOData, screenSize) == 272, "std140 drift");
static_assert(offsetof(GlobalUBOData, cameraParams) == 304, "std140 drift");
static_assert(sizeof(GlobalUBOData) == 320, "std140 drift");

/** Bit positions within MaterialUBOData::textureMask. */
enum MaterialTextureBits : uint32_t {
	MATERIAL_TEX_BASE_COLOR = 1u << 0,
	MATERIAL_TEX_METALLIC_ROUGHNESS = 1u << 1,
	MATERIAL_TEX_NORMAL = 1u << 2,
	MATERIAL_TEX_OCCLUSION = 1u << 3,
	MATERIAL_TEX_EMISSIVE = 1u << 4
};

/** Bit positions within MaterialUBOData::flags. */
enum MaterialFlagBits : uint32_t {
	MATERIAL_FLAG_ALPHA_TEST = 1u << 0,
	MATERIAL_FLAG_UNLIT = 1u << 1
};

/** Metallic-roughness PBR parameters (binding 1). */
struct MaterialUBOData {
	Vector4 baseColor;
	/** rgb = emissive color, w = emissive strength. */
	Vector4 emissive;
	float metallic;
	float roughness;
	float alphaCutoff;
	float normalScale;
	float occlusionStrength;
	uint32_t textureMask;
	uint32_t flags;
	float padding0;
};
static_assert(offsetof(MaterialUBOData, metallic) == 32, "std140 drift");
static_assert(offsetof(MaterialUBOData, textureMask) == 52, "std140 drift");
static_assert(sizeof(MaterialUBOData) == 64, "std140 drift");

/**
 * Per-draw object transform (push constants). The normal matrix occupies
 * three vec4 columns, matching std430 mat3 layout used by SPIR-V reflection.
 */
struct ObjectPushConstants {
	Matrix4 model;
	Vector4 normalMatrix[3];

	explicit ObjectPushConstants(const Matrix4& modelMatrix)
		: model(modelMatrix) {
		const Matrix3 normal =
			glm::transpose(glm::inverse(Matrix3(modelMatrix)));
		for (int column = 0; column < 3; column++) {
			normalMatrix[column] = Vector4(normal[column], 0.0f);
		}
	}
};
static_assert(sizeof(ObjectPushConstants) == 112, "push constant drift");

/**
 * Joints addressable by one skinned draw. 256 palette entries of 48 bytes
 * fill 12 KB, inside the 16 KB uniform block minimum that OpenGL ES 3 and
 * WebGL 2 guarantee. Must equal Skeleton::kMaxJoints.
 */
constexpr uint32_t MAX_SKINNING_JOINTS = 256;

/**
 * Skinning palette (binding 4): per joint, model * inverseBind with the
 * affine matrix's three rows stored as glm::mat3x4 columns. GLSL declares
 * `mat3x4 joints[256]` and transforms with `vec4(p, 1.0) * joints[j]`.
 */
struct SkinningUBOData {
	Matrix3x4 joints[MAX_SKINNING_JOINTS];
};
static_assert(sizeof(Matrix3x4) == 48, "std140 drift");
static_assert(sizeof(SkinningUBOData) == 48 * MAX_SKINNING_JOINTS,
			  "std140 drift");

/** Light type codes stored in GpuLight::colorType.w. */
constexpr float GPU_LIGHT_DIRECTIONAL = 0.0f;
constexpr float GPU_LIGHT_POINT = 1.0f;
constexpr float GPU_LIGHT_SPOT = 2.0f;

/** One light as consumed by shaders (std140, 64 bytes). */
struct GpuLight {
	/** xyz = world position, w = range. */
	Vector4 positionRange;
	/** rgb = color * intensity, w = light type code. */
	Vector4 colorType;
	/** xyz = normalized emission direction, w = cos(outer cone angle). */
	Vector4 directionCosOuter;
	/**
	 * x = cos(inner cone angle), y = first shadow matrix index or -1,
	 * z = depth bias (texels), w = normal bias (texels).
	 */
	Vector4 shadowParams;
};
static_assert(sizeof(GpuLight) == 64, "std140 drift");

/** Maximum lights evaluated per fragment in the forward pass. */
constexpr uint32_t MAX_FORWARD_LIGHTS = 32;

/**
 * Light list (binding 2). Directional lights occupy the first
 * counts.y entries; counts.x is the total.
 */
struct LightsUBOData {
	GpuLight lights[MAX_FORWARD_LIGHTS];
	int32_t counts[4];
};
static_assert(offsetof(LightsUBOData, counts) == 64 * MAX_FORWARD_LIGHTS,
			  "std140 drift");

/** Shadow matrix slots: cascades first, then local-light faces. */
constexpr uint32_t MAX_SHADOW_MATRICES = 64;

/** Shadow projections and atlas placement (binding 3). */
struct ShadowUBOData {
	Matrix4 matrices[MAX_SHADOW_MATRICES];
	/** xy = atlas UV offset, zw = atlas UV scale, per matrix. */
	Vector4 atlasRects[MAX_SHADOW_MATRICES];
	/**
	 * x = texel world size (orthographic) or texel size at unit distance
	 * (perspective), y = 1 when perspective, per matrix.
	 */
	Vector4 matrixParams[MAX_SHADOW_MATRICES];
	/** Far view distance of each cascade. */
	Vector4 cascadeSplits;
	/**
	 * x = cascade count (0 = no directional shadow), y = 1 / directional
	 * atlas size, z = 1 / local atlas size, w = unused.
	 */
	Vector4 shadowInfo;
};
static_assert(offsetof(ShadowUBOData, atlasRects) == 64 * MAX_SHADOW_MATRICES,
			  "std140 drift");
static_assert(sizeof(ShadowUBOData) == 96 * MAX_SHADOW_MATRICES + 32,
			  "std140 drift");

/** Per-light push constants for deferred light volumes (128 bytes). */
struct LightVolumePushConstants {
	Matrix4 model;
	GpuLight light;
};
static_assert(sizeof(LightVolumePushConstants) == 128, "push constant drift");

/** Per-draw push constants for shadow casters (128 bytes). */
struct ShadowPushConstants {
	Matrix4 model;
	Matrix4 lightViewProjection;
};
static_assert(sizeof(ShadowPushConstants) == 128, "push constant drift");

/** Per-face push constants for cubemap capture and irradiance passes (64 bytes). */
struct IBLCubemapPushConstants {
	Matrix4 viewProjection;
};
static_assert(sizeof(IBLCubemapPushConstants) == 64, "push constant drift");

/** Per-face push constants for GGX specular prefilter convolution (80 bytes). */
struct IBLPrefilterPushConstants {
	Matrix4 viewProjection;
	/** x = roughness in [0, 1], y = source cubemap face resolution, zw = unused. */
	Vector4 params;
};
static_assert(sizeof(IBLPrefilterPushConstants) == 80, "push constant drift");

/** Per-emitter push constants for batched particle rendering (16 bytes). */
struct ParticlePushConstants {
	/** x = 1.0 when sampling a sprite texture, 0.0 for procedural soft circle. */
	Vector4 params;
};
static_assert(sizeof(ParticlePushConstants) == 16, "push constant drift");

/** Uniform block for procedural HDR sky generation (binding 1, 80 bytes). */
struct ProceduralSkyUBOData {
	/** xyz = normalized direction toward sun, w = cos(sunAngularRadius). */
	Vector4 sunDirAndCosRadius;
	/** rgb = linear sun color, w = sun intensity. */
	Vector4 sunColorAndIntensity;
	/** rgb = linear zenith sky color, w = unused. */
	Vector4 zenithColor;
	/** rgb = linear horizon sky color, w = unused. */
	Vector4 horizonColor;
	/** rgb = linear ground bounce color, w = unused. */
	Vector4 groundColor;
};
static_assert(sizeof(ProceduralSkyUBOData) == 80, "std140 drift");

}  // namespace bird

#endif	// BIRD_GPU_LAYOUTS_H
