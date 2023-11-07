# Bird Engine

## Downloads
If you want the latest version, you must build it yourself. For the most stable version, look for stable tagged branches.

## Status
This project is currently under heavy development and may not work properly.

## File Organization
The engine's code is contained in the `bird::` namespace. The meat of the code is in the Application class; from there, we initialize all other 

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

## Running the code


## License
© 2023 Seth Hahn
https://choosealicense.com/licenses/gpl-3.0/
