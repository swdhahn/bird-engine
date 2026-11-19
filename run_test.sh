#!/bin/bash

# Defaults
BUILD_TYPE="Debug"
RUN_PROGRAM=false

# Loop through arguments
for arg in "$@"
do
    case ${arg,,} in
        release)
            BUILD_TYPE="Release"
            ;;
        run)
            RUN_PROGRAM=true
            ;;
    esac
done

echo "Configuration: [Type: $BUILD_TYPE] [Run: $RUN_PROGRAM]"


cmake -B build -DCMAKE_BUILD_TYPE=$BUILD_TYPE && cmake --build build -j8 && cmake --install build --prefix build && cd test/ && cmake -B build -DCMAKE_BUILD_TYPE=$BUILD_TYPE && cmake --build build -j8 && cd .. || exit 1

if [ "$RUN_PROGRAM" = true ]; then
    echo "Executing..."
    ./build/Bird test/build/libBird_test
else
    echo "Build complete (Skipping execution)."
fi
