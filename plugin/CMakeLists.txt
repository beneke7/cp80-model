cmake_minimum_required(VERSION 3.22)

project(CP80Plugin VERSION 0.1.0 LANGUAGES C CXX)

set(JUCE_DIR "/Users/bene/test/bene_pedal/JUCE" CACHE PATH
    "JUCE source directory")
if (NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE_DIR must point to a JUCE checkout")
endif()

if (APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")

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
    COPY_PLUGIN_AFTER_BUILD FALSE
)

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
target_link_libraries(CP80 PRIVATE juce::juce_audio_utils)

add_executable(CP80Smoke Source/PluginSmoke.cpp)
target_include_directories(CP80Smoke PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/Source"
    "${CMAKE_CURRENT_LIST_DIR}/../src"
)
target_compile_features(CP80Smoke PRIVATE cxx_std_17)
target_link_libraries(CP80Smoke PRIVATE CP80 juce::juce_audio_utils)

enable_testing()
add_test(NAME CP80Smoke COMMAND CP80Smoke)
