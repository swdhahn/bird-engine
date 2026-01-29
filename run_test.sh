cmake -B build && cmake --build build -j8 && cmake --install build --prefix build
cd test/
cmake -B build && cmake --build build -j8 && cd .. && ./build/Bird test/build/libBird_test
