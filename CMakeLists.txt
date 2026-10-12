cmake_minimum_required(VERSION 3.24)
project(MusicToMyMouth VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
include(FetchContent)
set(JUCE_SOURCE_DIR "" CACHE PATH "Optional existing JUCE checkout")
if(JUCE_SOURCE_DIR)
    add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/juce")
else()
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG b5df4524c0f01ba570a2949b625fecc426e7b8cc GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(juce)
endif()
FetchContent_Declare(rubberband URL https://github.com/breakfastquay/rubberband/archive/refs/tags/v4.0.0.tar.gz URL_HASH SHA256=24300f48a8014b7c863b573a9647e61b1b19b37875e2cdd92005e64c6424d266 DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(rubberband)
add_library(mouth_pitch_engine STATIC "${rubberband_SOURCE_DIR}/single/RubberBandSingle.cpp")
target_include_directories(mouth_pitch_engine PUBLIC "${rubberband_SOURCE_DIR}")
target_link_libraries(mouth_pitch_engine PUBLIC "-framework Accelerate")
set_target_properties(mouth_pitch_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
juce_add_plugin(MusicToMyMouth
    COMPANY_NAME "Sabarno Islam" BUNDLE_ID "com.sabcodes.musictomymouth"
    PLUGIN_MANUFACTURER_CODE Sbcd PLUGIN_CODE Mtmm
    FORMATS AU PRODUCT_NAME "MusicToMyMouth"
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE AU_SANDBOX_SAFE TRUE)
juce_generate_juce_header(MusicToMyMouth)
target_sources(MusicToMyMouth PRIVATE
    Source/Parameters.cpp Source/DSP/Pitch.cpp Source/DSP/VocalChain.cpp
    Source/Model/Capture.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(MusicToMyMouth PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(MusicToMyMouth PRIVATE mouth_pitch_engine juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
target_compile_options(MusicToMyMouth PRIVATE -Wall -Wextra -Wpedantic)

option(MOUTH_BUILD_VALIDATION "Build the offline vocal validation runner" OFF)
if(MOUTH_BUILD_VALIDATION)
    add_executable(MusicToMyMouthValidation Tests/Validation.cpp)
    target_link_libraries(MusicToMyMouthValidation PRIVATE MusicToMyMouth)
    target_include_directories(MusicToMyMouthValidation PRIVATE
        "$<TARGET_PROPERTY:MusicToMyMouth,INCLUDE_DIRECTORIES>")
    target_compile_definitions(MusicToMyMouthValidation PRIVATE
        "$<TARGET_PROPERTY:MusicToMyMouth,COMPILE_DEFINITIONS>")
    target_compile_options(MusicToMyMouthValidation PRIVATE -Wall -Wextra -Wpedantic)
endif()
