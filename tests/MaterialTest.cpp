#include <gtest/gtest.h>

#include <sstream>

#include "build-tools/MaterialFormat.h"
#include "renders/generic/Material.h"
#include "renders/generic/RenderQueue.h"

using namespace bird;

TEST(Material, DefaultsPackIntoStd140Block) {
	Material material;
	const MaterialUBOData data = material.buildGpuData();
	EXPECT_EQ(data.baseColor, Vector4(1.0f));
	EXPECT_FLOAT_EQ(data.metallic, 0.0f);
	EXPECT_FLOAT_EQ(data.roughness, 0.5f);
	EXPECT_EQ(data.textureMask, 0u);
	EXPECT_EQ(data.flags, 0u);
}

TEST(Material, SettersClampToPhysicalRanges) {
	Material material;
	material.setMetallic(2.0f).setRoughness(0.0f).setOcclusionStrength(-1.0f);
	EXPECT_FLOAT_EQ(material.getMetallic(), 1.0f);
	EXPECT_FLOAT_EQ(material.getRoughness(), 0.02f);
	EXPECT_FLOAT_EQ(material.getOcclusionStrength(), 0.0f);
}

TEST(Material, FlagsReflectBlendAndDomain) {
	Material material;
	material.setBlendMode(BlendMode::Masked).setDomain(MaterialDomain::Unlit);
	const MaterialUBOData data = material.buildGpuData();
	EXPECT_TRUE(data.flags & MATERIAL_FLAG_ALPHA_TEST);
	EXPECT_TRUE(data.flags & MATERIAL_FLAG_UNLIT);
}

TEST(Material, TextureSlotsSetMaskBits) {
	Material material;
	auto texture = std::make_shared<Texture>(nullptr, 1, 1, FORMAT_8RGBA);
	material.setTexture(TextureSlot::Normal, texture);
	material.setTexture(TextureSlot::Emissive, texture);
	EXPECT_EQ(material.buildGpuData().textureMask,
			  MATERIAL_TEX_NORMAL | MATERIAL_TEX_EMISSIVE);
	material.setTexture(TextureSlot::Normal, nullptr);
	EXPECT_EQ(material.buildGpuData().textureMask, MATERIAL_TEX_EMISSIVE);
	EXPECT_THROW(material.setTexture(TextureSlot::Count, texture),
				 std::out_of_range);
}

TEST(Material, BlendedMaterialsNeverCastShadows) {
	Material material;
	EXPECT_TRUE(material.castsShadows());
	material.setBlendMode(BlendMode::Transparent);
	EXPECT_FALSE(material.castsShadows());
}

TEST(MaterialClassification, RoutesToDeferredForwardOrTransparent) {
	Material lit, unlit, forced, glass, glow;
	unlit.setDomain(MaterialDomain::Unlit);
	forced.setForceForward(true);
	glass.setBlendMode(BlendMode::Transparent);
	glow.setBlendMode(BlendMode::Additive);
	EXPECT_EQ(classifyMaterial(lit), RenderBucket::Deferred);
	EXPECT_EQ(classifyMaterial(unlit), RenderBucket::ForwardOpaque);
	EXPECT_EQ(classifyMaterial(forced), RenderBucket::ForwardOpaque);
	EXPECT_EQ(classifyMaterial(glass), RenderBucket::Transparent);
	EXPECT_EQ(classifyMaterial(glow), RenderBucket::Transparent);
}

TEST(MaterialFormat, RoundTripsVersionTwo) {
	format::MaterialFileData original;
	original.baseColor[0] = 0.25f;
	original.metallic = 0.75f;
	original.roughness = 0.4f;
	original.blendMode = format::MATERIAL_BLEND_MASKED;
	original.doubleSided = 1;
	original.textures = {{format::MATERIAL_SLOT_NORMAL, "rock_n.png"},
						 {format::MATERIAL_SLOT_BASE_COLOR, "rock.png"}};
	std::stringstream stream;
	format::writeMaterialFile(stream, original);

	const format::MaterialFileData decoded = format::readMaterialFile(stream);
	EXPECT_FALSE(decoded.legacy);
	EXPECT_FLOAT_EQ(decoded.baseColor[0], 0.25f);
	EXPECT_FLOAT_EQ(decoded.metallic, 0.75f);
	EXPECT_FLOAT_EQ(decoded.roughness, 0.4f);
	EXPECT_EQ(decoded.blendMode, format::MATERIAL_BLEND_MASKED);
	EXPECT_EQ(decoded.doubleSided, 1);
	EXPECT_EQ(decoded.textures, original.textures);
}

TEST(MaterialFormat, ConvertsLegacyBlinnPhong) {
	std::stringstream stream;
	auto put = [&](float v) {
		stream.write(reinterpret_cast<const char*>(&v), sizeof(v));
	};
	for (float v : {0.8f, 0.1f, 0.1f}) put(v);	 // diffuse
	for (float v : {1.0f, 1.0f, 1.0f}) put(v);	 // specular
	for (float v : {1.0f, 1.0f, 1.0f}) put(v);	 // ambient
	put(32.0f);									 // shininess
	put(0.5f);									 // opacity
	for (float v : {0.0f, 0.0f, 0.0f}) put(v);	 // emission
	put(0.0f);									 // reflectivity
	const uint8_t textureCount = 2;
	stream.write(reinterpret_cast<const char*>(&textureCount), 1);
	for (const std::string path : {"diffuse.png", "bump.png"}) {
		const uint16_t length = static_cast<uint16_t>(path.size());
		stream.write(reinterpret_cast<const char*>(&length), sizeof(length));
		stream.write(path.data(), length);
	}

	const format::MaterialFileData decoded = format::readMaterialFile(stream);
	EXPECT_TRUE(decoded.legacy);
	EXPECT_FLOAT_EQ(decoded.baseColor[0], 0.8f);
	EXPECT_FLOAT_EQ(decoded.baseColor[3], 0.5f);
	EXPECT_EQ(decoded.blendMode, format::MATERIAL_BLEND_TRANSPARENT);
	EXPECT_EQ(decoded.doubleSided, 1);
	EXPECT_NEAR(decoded.roughness, format::shininessToRoughness(32.0f), 1e-6);
	ASSERT_EQ(decoded.textures.size(), 1u);
	EXPECT_EQ(decoded.textures[0].first, format::MATERIAL_SLOT_BASE_COLOR);
	EXPECT_EQ(decoded.textures[0].second, "diffuse.png");
}

TEST(MaterialFormat, ShininessMapsMonotonicallyToRoughness) {
	EXPECT_GT(format::shininessToRoughness(2.0f),
			  format::shininessToRoughness(64.0f));
	EXPECT_GT(format::shininessToRoughness(64.0f),
			  format::shininessToRoughness(1024.0f));
	EXPECT_LE(format::shininessToRoughness(0.0f), 1.0f);
}

TEST(MaterialFormat, RejectsTruncationAndBadSlots) {
	std::stringstream truncated(std::string("BMAT\x02\x00", 6));
	EXPECT_THROW(format::readMaterialFile(truncated), std::runtime_error);

	format::MaterialFileData data;
	data.textures = {{9, "x.png"}};
	std::stringstream stream;
	format::writeMaterialFile(stream, data);
	EXPECT_THROW(format::readMaterialFile(stream), std::runtime_error);
}
