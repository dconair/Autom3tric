import org.jetbrains.compose.desktop.application.dsl.TargetFormat
import org.jetbrains.kotlin.gradle.dsl.JvmTarget

// macOS desktop app: the same instrument UI as the Android app (Compose Multiplatform shares the androidx.compose
// API) on top of the same C++ engine, with CoreAudio instead of Oboe. The UI and engine-wrapper sources are shared
// with :app by copying an explicit file list (syncSharedSources), not by duplicating them.
plugins {
    alias(libs.plugins.kotlin.jvm)
    alias(libs.plugins.kotlin.compose)
    alias(libs.plugins.compose.multiplatform)
}

java {
    sourceCompatibility = JavaVersion.VERSION_17
    targetCompatibility = JavaVersion.VERSION_17
}
kotlin {
    compilerOptions { jvmTarget.set(JvmTarget.JVM_17) }
}

dependencies {
    implementation(compose.desktop.currentOs)
    implementation(compose.material3)
    implementation(libs.org.json)
    // Java Sound providers for the formats the JDK cannot read: MP3, FLAC, Ogg Vorbis (macOS uses afconvert first)
    implementation(libs.mp3spi)
    implementation(libs.jflac.codec)
    implementation(libs.vorbisspi)
    testImplementation(kotlin("test"))
}

val sharedSources = layout.buildDirectory.dir("generated/shared-src")
val syncSharedSources = tasks.register<Sync>("syncSharedSources") {
    from("../app/src/main/java") {
        include(
            "com/autodj/app/audio/AudioEngine.kt",
            "com/autodj/app/audio/DjStatus.kt",
            "com/autodj/app/audio/EngineStatus.kt",
            "com/autodj/app/audio/NativeEngine.kt",
            "com/autodj/app/audio/ParamSpec.kt",
            "com/autodj/app/dj/DeckController.kt",
            "com/autodj/app/dj/DjController.kt",
            "com/autodj/app/dj/GranularPattern.kt",
            "com/autodj/app/dj/LibraryStore.kt",
            "com/autodj/app/dj/MixController.kt",
            "com/autodj/app/dj/MixModels.kt",
            "com/autodj/app/dj/PerformanceController.kt",
            "com/autodj/app/dj/TrackDecoder.kt",
            "com/autodj/app/dj/TrackModels.kt",
            "com/autodj/app/ui/dj/ClockInfo.kt",
            "com/autodj/app/ui/dj/DecksScreen.kt",
            "com/autodj/app/ui/dj/EffectsScreen.kt",
            "com/autodj/app/ui/dj/LibraryScreen.kt",
            "com/autodj/app/ui/dj/MixScreen.kt",
            "com/autodj/app/ui/dj/PadsScreen.kt",
            "com/autodj/app/design/AppBar.kt",
            "com/autodj/app/design/Components.kt",
            "com/autodj/app/design/Design.kt",
            "com/autodj/app/design/SongWave.kt",
            "com/autodj/app/ui/BeatBuilder.kt",
            "com/autodj/app/ui/Guide.kt",
            "com/autodj/app/ui/Help.kt",
            "com/autodj/app/ui/Theme.kt",
            "com/autodj/app/ui/InstrumentControls.kt",
            "com/autodj/app/ui/InstrumentScreen.kt",
            "com/autodj/app/ui/WaveformScreen.kt",
        )
    }
    into(sharedSources)
}
kotlin.sourceSets.named("main") { kotlin.srcDir(syncSharedSources) }

// Test helpers shared with the Android unit tests: the fake engine host and the registry snapshot.
val sharedTestSources = layout.buildDirectory.dir("generated/shared-test-src")
val syncSharedTestSources = tasks.register<Sync>("syncSharedTestSources") {
    from("../app/src/test") {
        include("java/com/autodj/app/ui/FakeInstrumentHost.kt", "resources/param_specs.json")
    }
    into(sharedTestSources)
}
kotlin.sourceSets.named("test") { kotlin.srcDir(sharedTestSources.map { it.dir("java") }) }
sourceSets.named("test") { resources.srcDir(sharedTestSources.map { it.dir("resources") }) }
tasks.named("compileTestKotlin") { dependsOn(syncSharedTestSources) }
tasks.named("processTestResources") { dependsOn(syncSharedTestSources) }

// libautodj_native.dylib (universal arm64 + x86_64) built from the repository's CMake project, only on macOS.
val isMac = System.getProperty("os.name").lowercase().contains("mac")
val nativeResources = layout.buildDirectory.dir("native-resources")
val buildNative = tasks.register<Exec>("buildNative") {
    group = "build"
    description = "Builds libautodj_native.dylib (CoreAudio + JNI) with CMake"
    onlyIf { isMac }
    inputs.dir("../native")
    inputs.file("../CMakeLists.txt")
    inputs.file("build-native-macos.sh")
    outputs.dir(nativeResources)
    commandLine("sh", "build-native-macos.sh", layout.buildDirectory.get().asFile.absolutePath)
}
// The same JNI library for Linux (null audio driver), so that the import/analysis tests run on a Linux machine too.
val isLinux = System.getProperty("os.name").lowercase().contains("linux")
val buildNativeLinux = tasks.register<Exec>("buildNativeLinux") {
    group = "build"
    description = "Builds libautodj_native.so (no audio output) for the Linux test run"
    onlyIf { isLinux }
    inputs.dir("../native")
    inputs.file("../CMakeLists.txt")
    inputs.file("build-native-linux.sh")
    outputs.dir(layout.buildDirectory.dir("native-resources/linux"))
    commandLine("sh", "build-native-linux.sh", layout.buildDirectory.get().asFile.absolutePath)
}
tasks.matching { it.name == "run" || it.name.startsWith("prepareAppResources") || it.name.startsWith("package") ||
    it.name.startsWith("createDistributable") || it.name.startsWith("createRuntimeImage") }
    .configureEach { dependsOn(buildNative) }

compose.desktop {
    application {
        mainClass = "com.autodj.desktop.MainKt"
        nativeDistributions {
            targetFormats(TargetFormat.Dmg)
            packageName = "AutoDJ"
            packageVersion = "1.0.0"  // macOS requires a major version >= 1; the app itself is version 0.2 (see README)
            description = "Granular instrument and automatic DJ"
            includeAllModules = true
            appResourcesRootDir.set(nativeResources)
            macOS {
                bundleID = "com.autodj.desktop"
                dockName = "AutoDJ"
            }
        }
    }
}

tasks.withType<Test>().configureEach {
    systemProperty("autodj.screenshotDir", layout.buildDirectory.dir("screenshots").get().asFile.absolutePath)
    systemProperty("java.awt.headless", "true")
    if (isMac) {
        // the JNI smoke tests load the freshly built dylib
        dependsOn(buildNative)
        systemProperty("autodj.native.path", nativeResources.get().file("macos/libautodj_native.dylib").asFile.absolutePath)
    } else if (isLinux) {
        dependsOn(buildNativeLinux)
        systemProperty("autodj.native.path", nativeResources.get().file("linux/libautodj_native.so").asFile.absolutePath)
    }
    systemProperty("autodj.fixtureDir", layout.projectDirectory.dir("src/test/fixtures").asFile.absolutePath)
}
