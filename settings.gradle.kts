pluginManagement {
    repositories {
        google {
            content {
                includeGroupByRegex("com\\.android.*")
                includeGroupByRegex("com\\.google.*")
                includeGroupByRegex("androidx.*")
            }
        }
        mavenCentral()
        gradlePluginPortal()
    }
}
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "AutoDJ"
// -PdesktopOnly=true skips the Android module (and with it the need for an Android SDK), e.g. when only the macOS app is built.
if (!providers.gradleProperty("desktopOnly").isPresent) include(":app")
include(":desktopApp")
