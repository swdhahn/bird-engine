//
// Created by Seth Hahn on 11/6/22.
//

#ifndef BIRD_GRAPHICSCONSTANTS_H
#define BIRD_GRAPHICSCONSTANTS_H

#include <cstdint>

namespace bird {

/**
 * Supported rendering backend APIs.
 */
enum GraphicsPipelineType {
	GRAPHICS_PIPELINE_NONE,
	GRAPHICS_PIPELINE_OPENGL,
	GRAPHICS_PIPELINE_VULKAN,
	GRAPHICS_PIPELINE_METAL,
	GRAPHICS_PIPELINE_DIRECTX
};

/** Currently active graphics backend. */
extern GraphicsPipelineType CURRENT_GRAPHICS_PIPELINE;

/** Window width in screen coordinate points. */
extern uint32_t WINDOW_WIDTH;

/** Window height in screen coordinate points. */
extern uint32_t WINDOW_HEIGHT;

}  // namespace bird

#endif	// BIRD_GRAPHICSCONSTANTS_H
