# Building for Android

The Android app in `android/` packages the sample game from `test/` as an APK for arm64 phones running Android 8.0 (API 26) or newer with OpenGL ES 3.0.

## Get the APK from GitHub Actions

Every push to `android-gles` that touches the engine, assets or app runs the **Android APK** workflow (`.github/workflows/android.yml`):

1. Open the repository's **Actions** tab and select the latest **Android APK** run.
2. Download the **bird-android-debug** artifact (a zip holding `app-debug.apk`).
3. Install it: copy the APK to the phone and open it (allow installs from that app), or run `adb install -r app-debug.apk` from a computer with USB debugging enabled.

The same workflow also installs an x86_64 build on an Android emulator and fails if the engine does not reach its frame loop; its log includes the engine's logcat output.

## Build locally

Requirements: JDK 17, Gradle 8.9 or newer, the Android SDK with NDK r27 and CMake 3.23 or newer (from the SDK manager).

```bash
cd android
gradle assembleDebug -PbirdNdkVersion=<installed NDK> -PbirdCmakeVersion=<installed CMake>
# APK: android/app/build/outputs/apk/debug/app-debug.apk
```

Optional property: `-PbirdAbis=arm64-v8a,x86_64` builds more ABIs. Android Studio can open `android/` directly.

## How the app is put together

| Piece | Location | Role |
|---|---|---|
| `libBird_core.so` | root `CMakeLists.txt`, `BIRD_PLATFORM=ANDROID` | Engine runtime plus the NativeActivity launcher (`src/platform/android/AndroidMain.cpp`) |
| `libBird_test.so` | `android/app/src/main/cpp/CMakeLists.txt` | The game, loaded with `dlopen` like the desktop plugin; name set by `BIRD_ANDROID_GAME_LIBRARY` |
| Assets | Gradle task `stageBirdAssets` | Every committed `.bird` file, at its repo path inside the APK's `assets/` |
| Dependencies | `FetchContent` in the root `CMakeLists.txt` | glm and Jolt are built from source for the device |

Baked assets come from the repository, so run `BuildTools` on a desktop and commit the `.bird` files after changing shaders, models, textures or fonts.

Engine output (`std::cout`, `std::cerr` and `platform::log`) appears in logcat under the tag `Bird`:

```bash
adb logcat -s Bird
```

## Current limits

- Touch acts as the left mouse button (first finger); there is no multi-touch, soft keyboard or gesture support yet.
- The UI uses density-independent pixels, so layouts designed for a 1280x720 desktop window can overflow a phone screen.
- Leaving the app keeps the OpenGL ES context and recreates only the surface; if a device loses the context, the app closes instead of rebuilding GPU resources.
- Dart scripting is not available on Android.
