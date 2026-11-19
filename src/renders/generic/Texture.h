//
// Created by Seth Hahn on 11/17/22.
//

#ifndef BIRD_TEXTURE_H
#define BIRD_TEXTURE_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "../../util/Constants.h"
#include "../../util/Resource.h"

namespace bird {

/**
 * Texture pixel formats and generation flags.
 */
enum TextureFormat {
	FORMAT_HAS_MIPMAPS = 1,
	FORMAT_8G = 1 << 1,
	FORMAT_8GA = 1 << 2,
	FORMAT_8RGB = 1 << 3,
	FORMAT_8RGBA = 1 << 4
};

class Texture;

/**
 * Fluent builder for configuring and uploading 2D GPU textures.
 */
class TextureBuilder {
   public:
	TextureBuilder(uint8_t* pData, uint32_t width, uint32_t height,
				   TextureFormat format);

	/** Enables automatic mipmap level generation on GPU upload. */
	TextureBuilder& generateMipMaps();

	/** Sets minification/magnification filter to bilinear interpolation. */
	TextureBuilder& filterLinear();

	/** Sets minification/magnification filter to nearest-neighbor (pixelated). */
	TextureBuilder& filterNearest();

	/** Instantiates and uploads the GPU texture using the active backend. */
	std::shared_ptr<Texture> build();

   private:
	uint8_t* m_pData = nullptr;
	uint32_t m_width, m_height, m_channels;
	TextureFormat m_format;
	bool m_generateMipMaps = false;
	bool m_linearFiltering = false;
	bool m_nearestFiltering = false;

	void createLinearInterpolated(uint8_t* srcData, uint8_t* dstData,
								  uint32_t nWidth, uint32_t nHeight);
	void createNearestInterpolated(uint8_t* srcData, uint8_t* dstData,
								   uint32_t nWidth, uint32_t nHeight);
};

/**
 * 2D GPU texture resource.
 *
 * Owns raw pixel memory and dimensions. Implements Resource serialization
 * for reading/writing cached .bird textures.
 */
class Texture : public Resource {
   public:
	explicit Texture(uint8_t* data, uint32_t width, uint32_t height,
					 TextureFormat format);
	virtual ~Texture();

	void read() override;
	void write() override;

	/** Creates a builder loading an image from disk. */
	static TextureBuilder create(const std::string& path);

	/** Creates a builder from in-memory raw pixel data. */
	static TextureBuilder create(uint8_t* data, uint32_t width, uint32_t height,
								 TextureFormat format);

	const TextureFormat& getFormat();
	uint32_t getWidth();
	uint32_t getHeight();

   protected:
	uint8_t* m_pData;
	uint32_t m_width, m_height;
	TextureFormat m_format;

   private:
	Texture(const Texture& tex);
	Texture(const Texture&& tex);
};

}  // namespace bird

#endif	// BIRD_TEXTURE_H
