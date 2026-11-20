// Android app for the sample game in test/: the engine runtime
// (libBird_core.so, which holds the NativeActivity launcher) plus the game
// library (libBird_test.so), and the engine's baked .bird assets.
plugins {
    id("com.android.application")
}

// CI passes the NDK and CMake installed on the runner; these are defaults.
val birdNdkVersion = providers.gradleProperty("birdNdkVersion")
    .getOrElse("27.2.12479018")
val birdCmakeVersion = providers.gradleProperty("birdCmakeVersion")
    .getOrElse("3.31.6")
// Comma-separated ABIs; CI adds x86_64 to smoke-test on an emulator.
val birdAbis = providers.gradleProperty("birdAbis")
    .getOrElse("arm64-v8a").split(",").map { it.trim() }

android {
    namespace = "dev.birdengine.sample"
    compileSdk = 35
    ndkVersion = birdNdkVersion

    defaultConfig {
        applicationId = "dev.birdengine.sample"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"

        ndk {
            abiFilters += birdAbis
        }
        externalNativeBuild {
            cmake {
                arguments += listOf(
                    // Engine and game share C++ objects across libraries.
                    "-DANDROID_STL=c++_shared",
                    "-DBIRD_ANDROID_GAME_LIBRARY=libBird_test.so",
                )
                targets += listOf("Bird_core", "Bird_test")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = birdCmakeVersion
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }

    packaging {
        // Keep libraries uncompressed so they load straight from the APK.
        jniLibs {
            useLegacyPackaging = false
        }
    }

    sourceSets["main"].assets.srcDir(layout.buildDirectory.dir("generated/birdAssets"))
}

// Copies the baked assets into the APK under assets/, keeping their repo
// paths ("assets/shaders/ui.vert.bird"), which ApkAssetVFS resolves as-is.
// Sources (.png, .glb, .ttf, shader text) stay out: the runtime reads only
// .bird files, which BuildTools bakes on the host.
val stageBirdAssets by tasks.registering(Sync::class) {
    from(rootProject.file("../assets")) {
        include("**/*.bird")
        // Not used by the sample game; keeps the APK about 57 MB smaller.
        exclude("models/bugatti/**")
    }
    into(layout.buildDirectory.dir("generated/birdAssets/assets"))
}

tasks.named("preBuild") {
    dependsOn(stageBirdAssets)
}
