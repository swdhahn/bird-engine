//
// Created by Seth Hahn on 11/6/22.
//

#ifndef BIRD_GRAPHICSCONSTANTS_H
#define BIRD_GRAPHICSCONSTANTS_H

#include <volk.h>

#include "GL/glew.h"
#include "GLFW/glfw3.h"

#include <array>
#include <bits/stdc++.h>
#include <cstdint>
#include <iostream>
#include <memory.h>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace bird {

enum GraphicsPipelineType {
  GRAPHICS_PIPELINE_NONE,
  GRAPHICS_PIPELINE_OPENGL,
  GRAPHICS_PIPELINE_VULKAN,
  GRAPHICS_PIPELINE_METAL,
  GRAPHICS_PIPELINE_DIRECTX
};

}

#endif // BIRD_GRAPHICSCONSTANTS_H
