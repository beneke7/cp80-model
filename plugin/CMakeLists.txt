cmake_minimum_required(VERSION 3.22)

project(CP80Plugin VERSION 0.1.0 LANGUAGES C CXX)

# Point JUCE_DIR at a local checkout to avoid the download; otherwise the pinned
# release is fetched.  CI always fetches, so the build is reproducible off a clone.
set(JUCE_DIR "" CACHE PATH "Optional local JUCE checkout (empty = fetch pinned release)")

if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

if (JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(STATUS "CP-80: using local JUCE at ${JUCE_DIR}")
    add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.12
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Local builds drop straight into the user plug-in folders; CI must not.
option(CP80_COPY_AFTER_BUILD "Install built plug-ins into the user plug-in folders" ON)

juce_add_plugin(CP80
    COMPANY_NAME "ToneKeep"
    COMPANY_WEBSITE ""
    COMPANY_EMAIL ""
    BUNDLE_ID "com.tonekeep.cp80"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MPE FALSE
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "CP-80"
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD ${CP80_COPY_AFTER_BUILD}
)

juce_add_binary_data(CP80Data SOURCES Resources/cp80.png)

target_sources(CP80 PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(CP80 PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../src"
)

target_compile_features(CP80 PRIVATE cxx_std_17)
target_compile_definitions(CP80 PRIVATE
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
)
target_link_libraries(CP80 PRIVATE CP80Data juce::juce_audio_utils)

# JUCE re-signs the VST3 and AU bundles ad-hoc after linking, but not the .app,
# which then fails `codesign -v` with "code has no resources".  Gatekeeper refuses
# to launch it.  Sign it ourselves.
if (APPLE AND TARGET CP80_Standalone)
    add_custom_command(TARGET CP80_Standalone POST_BUILD
        COMMAND codesign --force --sign - --timestamp=none
                "$<TARGET_BUNDLE_DIR:CP80_Standalone>"
        VERBATIM)
endif()

add_executable(CP80Smoke Source/PluginSmoke.cpp)
target_include_directories(CP80Smoke PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/Source"
    "${CMAKE_CURRENT_LIST_DIR}/../src"
)
target_compile_features(CP80Smoke PRIVATE cxx_std_17)
target_link_libraries(CP80Smoke PRIVATE CP80 juce::juce_audio_utils)

enable_testing()
add_test(NAME CP80Smoke COMMAND CP80Smoke)

# Offscreen UI snapshot, for reviewing the editor without a host.
add_executable(CP80Shot Source/PluginShot.cpp)
target_include_directories(CP80Shot PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/Source"
    "${CMAKE_CURRENT_LIST_DIR}/../src"
)
target_compile_features(CP80Shot PRIVATE cxx_std_17)
target_link_libraries(CP80Shot PRIVATE CP80 CP80Data juce::juce_audio_utils)
