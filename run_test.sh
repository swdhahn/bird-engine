#!/bin/bash

# Defaults
BUILD_TYPE="Debug"
RUN_PROGRAM=false
LAUNCH_EDITOR=false

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
        editor)
            LAUNCH_EDITOR=true
            ;;
    esac
done

echo "Configuration: [Type: $BUILD_TYPE] [Run: $RUN_PROGRAM] [Editor: $LAUNCH_EDITOR]"


cmake -B build -DCMAKE_BUILD_TYPE=$BUILD_TYPE && cmake --build build -j8 && cmake --install build --prefix build && ./build/BuildTools && cd test/ && cmake -B build -DCMAKE_BUILD_TYPE=$BUILD_TYPE && cmake --build build -j8 && cd .. || exit 1

if [ "$RUN_PROGRAM" = true ]; then
    echo "Executing..."
    if [ "$LAUNCH_EDITOR" = true ]; then
        BIRD_EDITOR=1 ./build/Bird test/build/libBird_test
    else
        ./build/Bird test/build/libBird_test
    fi
else
    echo "Build complete (Skipping execution)."
fi
