cmake_minimum_required(VERSION 3.22)
project(CLARITY VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Runtime estático: el .vst3 NO necesita instalar el "Visual C++ Redistributable" en el PC del usuario
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.8
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(CLARITY
    COMPANY_NAME               "Tiladco"
    PRODUCT_NAME               "CLARITY"
    PLUGIN_MANUFACTURER_CODE   Tila
    PLUGIN_CODE                Clry
    FORMATS                    VST3
    VST3_CATEGORIES            Fx Analyzer
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    IS_MIDI_EFFECT             FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD    FALSE)

# Recursos incrustados en el .vst3: logo HomeSounds + tipografías (Resources/)
file(GLOB CLARITY_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/fonts/*.ttf"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/fonts/*.otf")
juce_add_binary_data(CLARITYAssets SOURCES ${CLARITY_ASSETS})

target_sources(CLARITY PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/StereoPanel.cpp
    Source/StereoPanel.h
    Source/UIConstants.h)

target_compile_definitions(CLARITY PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(CLARITY
    PRIVATE
        CLARITYAssets
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags)
