//
// Created by Seth Hahn on 11/6/22.
//

#ifndef BIRD_GRAPHICSCONSTANTS_H
#define BIRD_GRAPHICSCONSTANTS_H

#include <volk.h>

#include "GL/glew.h"
#include "GLFW/glfw3.h"

#include "../util/Math.h"

#include <cstdint>
#include <string>
#include <stdexcept>
#include <vector>
#include <optional>
#include <set>
#include <iostream>
#include <array>

namespace bird {

    enum GraphicsPipelineType {
        GRAPHICS_PIPELINE_NONE,
        GRAPHICS_PIPELINE_OPENGL, GRAPHICS_PIPELINE_VULKAN,
        GRAPHICS_PIPELINE_METAL, GRAPHICS_PIPELINE_DIRECTX
    };

    extern GraphicsPipelineType CURRENT_GRAPHICS_PIPELINE;



}

#endif //BIRD_GRAPHICSCONSTANTS_H
