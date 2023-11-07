# Bird Engine
A game engine that encapsulates multiple graphics libraries (OpenGL, Vulkan, Metal, DirectX). The goal of this project is to create something usable and enjoyable to use for developers. 
*Note* This project is currently under heavy development and may not work properly or be fully usable yet.

## Downloads
If you want the latest version, you must build it yourself. For the most stable version, look for stable tagged branches. The releases provided are simply built from the most stable branch.


## File Organization
The engine's code is contained in the `bird::` namespace. The meat of the code is in the Application class; from there, we initialize all other engine systems (GraphicsPipeline, Game Managers, Physics Processor, and similar). Each graphics pipeline has its own namespace: OpenGL to `gl::` Vulkan to `vk::`, Metal to `mt`, and DirectX to `dx`. All graphics libraries are abstracted away, so users should only have to worry about everything in the `bird::` namespace.

## Getting and Building
1. Clone the repo
```bash
git clone https://github.com/swdhahn/bird-engine $SOURCE_DIR
cd $SOURCE_DIR
```
2. Run the cmake file. CMAKE_BUILD_TYPE can be Release or Debug.
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -B $BUILD_DIR
cd $BUILD_DIR
cmake --build .
```
3. This should download all dependencies through CMake, then compile and link all required files/libraries. The libraries should only download once unless there is a necessary update.
4. The fun begins, and you may begin programming 😎

## License
© 2023 Seth Hahn
https://choosealicense.com/licenses/gpl-3.0/
