import org.jetbrains.kotlin.gradle.dsl.JvmTarget

plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}

android {
    namespace = "com.autodj.app"
    compileSdk = 36
    ndkVersion = "29.0.14206865"

    defaultConfig {
        applicationId = "com.autodj.app"
        minSdk = 26
        targetSdk = 36
        versionCode = 1
        versionName = "0.2.0"

        ndk {
            // 64-bit phones + x86_64 emulator. armeabi-v7a is deliberately left out (no NEON-64 budget guarantees).
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
        externalNativeBuild {
            cmake {
                arguments += listOf(
                    "-DANDROID_STL=c++_shared",
                    "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON",
                    "-DAUTODJ_BUILD_TESTS=OFF",
                    "-DAUTODJ_BUILD_BENCH=OFF",
                )
            }
        }
    }

    buildTypes {
        debug {
            externalNativeBuild {
                // Debug builds carry the audio-thread allocation detector (violations are logged and counted).
                cmake { arguments += listOf("-DAUTODJ_RT_GUARD=ON", "-DCMAKE_BUILD_TYPE=Debug") }
            }
        }
        release {
            isMinifyEnabled = false
            externalNativeBuild {
                cmake { arguments += listOf("-DAUTODJ_RT_GUARD=OFF", "-DCMAKE_BUILD_TYPE=Release") }
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlin {
        compilerOptions { jvmTarget.set(JvmTarget.JVM_17) }
    }
    buildFeatures {
        compose = true
        prefab = true
    }
    packaging {
        jniLibs { useLegacyPackaging = false }
    }
    testOptions {
        unitTests {
            isReturnDefaultValues = true
            isIncludeAndroidResources = true  // Robolectric needs the merged manifest / resources for the Compose renders
            all { test ->
                // Where Robolectric downloads its android-all jars from. Default (Maven Central) works everywhere;
                // behind a proxy that blocks it, pass -ProbolectricRepoUrl=<mirror>.
                providers.gradleProperty("robolectricRepoUrl").orNull?.let { test.systemProperty("robolectric.dependency.repo.url", it) }
                test.systemProperty("autodj.screenshotDir", layout.buildDirectory.dir("screenshots").get().asFile.absolutePath)
                test.maxHeapSize = "2g"
            }
        }
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(libs.androidx.lifecycle.viewmodel.compose)
    implementation(libs.androidx.lifecycle.runtime.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.compose.ui)
    implementation(libs.androidx.compose.ui.tooling.preview)
    implementation(libs.androidx.compose.material3)
    implementation(libs.androidx.compose.material.icons.core)
    implementation(libs.oboe)
    debugImplementation(libs.androidx.compose.ui.tooling)

    testImplementation(libs.junit)
    testImplementation(libs.org.json)
    testImplementation(libs.robolectric)
    testImplementation(platform(libs.androidx.compose.bom))
    testImplementation(libs.androidx.compose.ui.test.junit4)
    debugImplementation(libs.androidx.compose.ui.test.manifest)
}
