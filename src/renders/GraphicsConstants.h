//
// Created by Seth Hahn on 11/6/22.
//

#ifndef BIRD_GRAPHICSCONSTANTS_H
#define BIRD_GRAPHICSCONSTANTS_H

#define GLFW_INCLUDE_VULKAN
#include "GL/glew.h"
#include "GLFW/glfw3.h"

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


}

#endif //BIRD_GRAPHICSCONSTANTS_H
