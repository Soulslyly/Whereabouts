cmake_minimum_required(VERSION 3.21)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/version.json" WHEREABOUTS_VERSION_JSON)
string(JSON WHEREABOUTS_NUMERIC_VERSION GET "${WHEREABOUTS_VERSION_JSON}" numeric)
string(JSON WHEREABOUTS_DISPLAY_VERSION GET "${WHEREABOUTS_VERSION_JSON}" display)

project(
    Whereabouts
    VERSION "${WHEREABOUTS_NUMERIC_VERSION}"
    LANGUAGES CXX RC
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WhereaboutsVersion.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/WhereaboutsVersion.h"
    @ONLY
)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/WhereaboutsVersion.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/WhereaboutsVersion.rc"
    @ONLY
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(WHEREABOUTS_OWNER_VALIDATION "Require exact owner-only dependency evidence" OFF)
set(WHEREABOUTS_SMF_DLL "" CACHE FILEPATH "Exact installed SKSE Menu Framework DLL for owner validation")
set(WHEREABOUTS_LEGACY_ADDRESS_LIBRARY "" CACHE FILEPATH "Exact installed Skyrim 1.5.97 Address Library database for owner validation")
include(cmake/OwnerValidation.cmake)

if(MSVC)
    add_compile_options(
        "/experimental:deterministic"
        "/pathmap:${PROJECT_SOURCE_DIR}=."
    )
endif()

include(cmake/Dependencies.cmake)

set(BUILD_TESTS OFF CACHE BOOL "Do not add CommonLib's test suite to the consumer build" FORCE)
set(ENABLE_SKYRIM_SE ON CACHE BOOL "Build CommonLib with Skyrim SE support" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "Build CommonLib with Skyrim AE support" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "Whereabouts does not target Skyrim VR" FORCE)
set(SKSE_SUPPORT_PATCH_SAFETY OFF CACHE BOOL "Whereabouts does not install native patches" FORCE)
add_subdirectory(external/CommonLibSSE-NG)

add_library(
    WhereaboutsCore
    STATIC
        src/API/PublicApi.cpp
        src/Commands/CommandPolicy.cpp
        src/Core/DistanceFormat.cpp
        src/Core/SnapshotState.cpp
        src/Core/SpatialPresentation.cpp
        src/Core/TextFold.cpp
        src/UI/LocalizationFormat.cpp
        src/UI/LocalizationResource.cpp
        src/Lifecycle/OperationEpoch.cpp
        src/Lifecycle/OperationQueue.cpp
        src/Lifecycle/UiCompletionMailbox.cpp
        src/Persistence/Serialization.cpp
        src/Persistence/RuntimeSettingsState.cpp
        src/Persistence/Settings.cpp
        src/Search/EditorIdLookup.cpp
        src/Search/LocationSearch.cpp
        src/Search/SearchEngine.cpp
        src/Search/SearchText.cpp
        src/Tracking/TrackingLifecycle.cpp
        src/Tracking/TrackingOperationLedger.cpp
        src/Tracking/TrackingQuestLayout.cpp
        src/Tracking/UninstallState.cpp
)

target_compile_features(WhereaboutsCore PUBLIC cxx_std_23)
target_include_directories(WhereaboutsCore PUBLIC "${PROJECT_SOURCE_DIR}/src")

add_library(
    Whereabouts
    SHARED
        src/API/PapyrusApi.cpp
        src/AppContext.cpp
        src/Compatibility/MenuFrameworkRuntime.cpp
        src/Lifecycle/OperationQueueSkyrim.cpp
        src/Commands/CommandService.cpp
        src/Core/FormIdentityAdapter.cpp
        src/Persistence/SkseSerialization.cpp
        src/Plugin.cpp
        src/Search/RuntimeIndex.cpp
        src/Search/IndexCoordinator.cpp
        src/Search/IndexCoordinatorSkyrim.cpp
        src/Targets/TargetResolver.cpp
        src/Tracking/TrackingService.cpp
        src/UI/ControllerKeyboard.cpp
        src/UI/FavoritesPage.cpp
        src/UI/Localization.cpp
        src/UI/LocationsPage.cpp
        src/UI/Menu.cpp
        src/UI/RecentPage.cpp
        src/UI/SearchPage.cpp
        src/UI/SettingsPage.cpp
        src/UI/TrackedPage.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/generated/WhereaboutsVersion.rc"
)

target_compile_features(Whereabouts PRIVATE cxx_std_23)
target_link_libraries(Whereabouts PRIVATE WhereaboutsCore CommonLibSSE::CommonLibSSE Version)
target_link_options(Whereabouts PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/Brepro>")
target_precompile_headers(Whereabouts PRIVATE src/PCH.h)
target_include_directories(
    Whereabouts
    PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

set_target_properties(
    Whereabouts
    PROPERTIES
        OUTPUT_NAME "Whereabouts"
)

option(WHEREABOUTS_BUILD_TESTS "Build Whereabouts developer tests when their source tree is available" ON)
if(WHEREABOUTS_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
elseif(WHEREABOUTS_BUILD_TESTS)
    message(STATUS "Whereabouts developer tests are not included in this public Source tree; skipping test targets")
endif()
include(cmake/Packaging.cmake)
