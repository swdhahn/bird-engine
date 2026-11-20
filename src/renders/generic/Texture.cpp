//
// Created by Seth Hahn on 11/17/22.
//

#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../../util/stb_image.h"
#include <cstring>
#include <stdexcept>
#include "../GraphicsPipeline.h"
#include "../opengl/GLTexture.h"
// #include "../vulkan/VKTexture.h"
#include "../opengl/GLApi.h"
#include "../opengl/GLCapabilities.h"

namespace bird {

TextureBuilder::TextureBuilder(uint8_t* pData, uint32_t width, uint32_t height,
							   TextureFormat format)
	: m_pData(pData), m_width(width), m_height(height), m_format(format) {}

TextureBuilder::TextureBuilder(uint32_t width, uint32_t height,
							   TextureFormat format)
	: TextureBuilder(nullptr, width, height, format) {}

TextureBuilder& TextureBuilder::generateMipMaps() {
	m_generateMipMaps = true;
	return *this;
}

TextureBuilder& TextureBuilder::filterLinear() {
	m_linearFiltering = true;
	return *this;
}

TextureBuilder& TextureBuilder::filterNearest() {
	m_nearestFiltering = true;
	return *this;
}

TextureBuilder& TextureBuilder::srgb() {
	m_srgb = true;
	return *this;
}

TextureBuilder& TextureBuilder::clampToEdge() {
	m_clampToEdge = true;
	return *this;
}

TextureBuilder& TextureBuilder::depthCompare() {
	m_depthCompare = true;
	return *this;
}

namespace {

/** Native OpenGL storage description resolved from a TextureFormat. */
struct GLFormatTriple {
	GLint internalFormat;
	GLenum layout;
	GLenum type;
};

GLFormatTriple resolveGLFormat(uint32_t format, bool srgb) {
	if (format & FORMAT_DEPTH24_STENCIL8) {
		return {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8};
	}
	if (format & FORMAT_DEPTH32F) {
		return {GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT};
	}
	if (format & FORMAT_RGBA16F) return {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT};
	if (format & FORMAT_RG16F) return {GL_RG16F, GL_RG, GL_HALF_FLOAT};

	// 8-bit data uploads in its real channel layout: treating 3-channel data
	// as RGBA reads past the end of the pixel buffer.
	const bool wantsSrgb = srgb || (format & FORMAT_SRGB);
	const GLint internal = wantsSrgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
	if (format & FORMAT_8RGB) return {internal, GL_RGB, GL_UNSIGNED_BYTE};
	if (format & FORMAT_8GA) return {internal, GL_RG, GL_UNSIGNED_BYTE};
	if (format & FORMAT_8G) return {internal, GL_RED, GL_UNSIGNED_BYTE};
	if (format & FORMAT_8RGBA) return {internal, GL_RGBA, GL_UNSIGNED_BYTE};
	throw std::invalid_argument("TextureBuilder: no storage format specified.");
}

/** @return channel count of an 8-bit color format, or 0 for other formats. */
uint32_t eightBitChannels(uint32_t format) {
	if (format & FORMAT_8RGBA) return 4;
	if (format & FORMAT_8RGB) return 3;
	if (format & FORMAT_8GA) return 2;
	if (format & FORMAT_8G) return 1;
	return 0;
}

/**
 * Expands 8-bit gray, gray-alpha or RGB pixels to RGBA; gray replicates
 * into the color channels, missing alpha becomes opaque. O(width * height).
 */
std::vector<uint8_t> expandToRgba(const uint8_t* pixels, uint32_t width,
								  uint32_t height, uint32_t channels) {
	const size_t count = static_cast<size_t>(width) * height;
	std::vector<uint8_t> rgba(count * 4);
	for (size_t i = 0; i < count; i++) {
		const uint8_t* source = pixels + i * channels;
		uint8_t* target = rgba.data() + i * 4;
		const bool gray = channels < 3;
		target[0] = source[0];
		target[1] = gray ? source[0] : source[1];
		target[2] = gray ? source[0] : source[2];
		target[3] = (channels == 2) ? source[1]
					: (channels == 4) ? source[3]
									  : 255;
	}
	return rgba;
}

/** Sets the four channel swizzles; core in desktop 3.3 and ES 3.0. */
void setSwizzle(GLint red, GLint green, GLint blue, GLint alpha) {
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, red);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, green);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, blue);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, alpha);
}

}  // namespace

std::shared_ptr<Texture> TextureBuilder::build() {
	if (CURRENT_GRAPHICS_PIPELINE != GRAPHICS_PIPELINE_OPENGL) {
		throw std::runtime_error(
			"Graphics pipeline not supported for building images!");
	}
	const bool depth = isDepthFormat(m_format);
	if (m_depthCompare && !depth) {
		throw std::logic_error(
			"TextureBuilder: depthCompare() requires a depth format.");
	}
	// Mipmaps are meaningless on depth and render-target float formats.
	const bool mipmapped =
		m_generateMipMaps && !depth && !isFloatColorFormat(m_format);
	const gl::GLCapabilities& caps = gl::GLCapabilities::current();
	// ES and WebGL pair an internal format with one upload layout only, and
	// WebGL has no swizzles; uploading every 8-bit color image as RGBA
	// satisfies both and keeps sRGB, filtering and mipmaps available.
	const uint32_t channels = eightBitChannels(m_format);
	const bool expandToFour = caps.isEmbedded() && channels != 0 &&
							  channels != 4;
	const uint32_t uploadFormat =
		expandToFour
			? ((m_format & ~(FORMAT_8G | FORMAT_8GA | FORMAT_8RGB)) |
			   FORMAT_8RGBA)
			: static_cast<uint32_t>(m_format);
	const GLFormatTriple gl = resolveGLFormat(uploadFormat, m_srgb);

	uint32_t texID;
	glGenTextures(1, &texID);
	glBindTexture(GL_TEXTURE_2D, texID);

	const GLint wrap = (m_clampToEdge || depth) ? GL_CLAMP_TO_EDGE : GL_REPEAT;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

	// ES and WebGL treat a depth texture as incomplete under LINEAR
	// filtering unless compare mode is on.
	const bool forceNearest =
		depth && !m_depthCompare && !caps.linearDepthFiltering;
	const GLint magFilter =
		(m_nearestFiltering || forceNearest) ? GL_NEAREST : GL_LINEAR;
	GLint minFilter = magFilter;
	if (mipmapped) {
		minFilter = m_nearestFiltering ? GL_NEAREST_MIPMAP_NEAREST
									   : GL_LINEAR_MIPMAP_LINEAR;
	}
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);

	if (m_depthCompare) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE,
						GL_COMPARE_REF_TO_TEXTURE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	}

	std::vector<uint8_t> expanded;
	const uint8_t* upload = m_pData;
	if (expandToFour) {
		if (m_pData != nullptr) {
			expanded = expandToRgba(m_pData, m_width, m_height, channels);
			upload = expanded.data();
		}
	} else if (m_format & FORMAT_8GA) {
		setSwizzle(GL_RED, GL_RED, GL_RED, GL_GREEN);
	} else if (m_format & FORMAT_8G) {
		setSwizzle(GL_RED, GL_RED, GL_RED, GL_ONE);
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, gl.internalFormat, m_width, m_height, 0,
				 gl.layout, gl.type, upload);
	if (mipmapped) glGenerateMipmap(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);

	TextureFormat stored = m_format;
	if (m_srgb) stored = static_cast<TextureFormat>(stored | FORMAT_SRGB);
	if (mipmapped) {
		stored = static_cast<TextureFormat>(stored | FORMAT_HAS_MIPMAPS);
	}
	return std::make_shared<gl::GLTexture>(m_pData, m_width, m_height, stored,
										   texID);
}

Texture::Texture(uint8_t* pData, uint32_t width, uint32_t height,
				 TextureFormat format)
	: m_pData(pData), m_width(width), m_height(height), m_format(format) {}

Texture::~Texture() {
	delete[] m_pData;
}

void Texture::read() {}

void Texture::write() {}

const TextureFormat& Texture::getFormat() {
	return m_format;
}
uint32_t Texture::getWidth() {
	return m_width;
}
uint32_t Texture::getHeight() {
	return m_height;
}

}  // namespace bird
