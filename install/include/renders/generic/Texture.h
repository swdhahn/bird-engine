//
// Created by Seth Hahn on 11/17/22.
//

#ifndef BIRD_TEXTURE_H
#define BIRD_TEXTURE_H

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <cstdint>
#include <string>

#include "../../util/Constants.h"
#include "../../util/Resource.h"

namespace bird {

enum TextureFormat {
	FORMAT_HAS_MIPMAPS = 1,
	FORMAT_8G = 1 << 1,
	FORMAT_8GA = 1 << 2,
	FORMAT_8RGB = 1 << 3,
	FORMAT_8RGBA = 1 << 4
};

class Texture;

class TextureBuilder {
   public:
	TextureBuilder(void* pData, uint32_t width, uint32_t height,
				   TextureFormat format);

	TextureBuilder& generateMipMaps();
	TextureBuilder& filterLinear();
	TextureBuilder& filterNearest();

	std::shared_ptr<Texture> build();

   private:
	void* m_pData = nullptr;
	uint32_t m_width, m_height, m_channels;
	TextureFormat m_format;
	bool m_generateMipMaps = false;
	bool m_linearFiltering = false;
	bool m_nearestFiltering = false;

	void createLinearInterpolated(void* srcData, void* dstData, uint32_t nWidth,
								  uint32_t nHeight);
	void createNearestInterpolated(void* srcData, void* dstData,
								   uint32_t nWidth, uint32_t nHeight);
};

/**
 * Todo: Add functions that edit textures?
 *
 * Texture is a class that holds a pointer to the texture data itself and the
 * width and height of an image.
 *
 */
class Texture : public Resource {
   public:
	explicit Texture(void* data, uint32_t width, uint32_t height,
					 TextureFormat format);
	virtual ~Texture();

	void read();
	void write();

	static TextureBuilder create(const std::string& path);
	static TextureBuilder create(void* data, uint32_t width, uint32_t height,
								 TextureFormat format);

	const TextureFormat& getFormat();
	uint32_t getWidth();
	uint32_t getHeight();

   protected:
	void* m_pData;
	uint32_t m_width, m_height;
	TextureFormat m_format;

   private:
	Texture(const Texture& tex);
	Texture(const Texture&& tex);
};

}  // namespace bird

#endif	// BIRD_TEXTURE_H
