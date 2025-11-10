//
// Created by Seth Hahn on 11/6/22.
//

#ifndef BIRD_GRAPHICSCONSTANTS_H
#define BIRD_GRAPHICSCONSTANTS_H

#include <bits/stdc++.h>
#include <memory.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "../util/Math.h"
#include "GL/glew.h"
#include "GLFW/glfw3.h"

namespace bird {

enum GraphicsPipelineType {
	GRAPHICS_PIPELINE_NONE,
	GRAPHICS_PIPELINE_OPENGL,
	GRAPHICS_PIPELINE_VULKAN,
	GRAPHICS_PIPELINE_METAL,
	GRAPHICS_PIPELINE_DIRECTX
};

extern GraphicsPipelineType CURRENT_GRAPHICS_PIPELINE;

}  // namespace bird

#endif	// BIRD_GRAPHICSCONSTANTS_H
