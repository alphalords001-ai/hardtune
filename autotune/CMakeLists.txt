cmake_minimum_required(VERSION 3.22)

project(HardTune VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
#  JUCE (récupéré automatiquement au premier build, rien à installer à la main)
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.4          # version figée : builds reproductibles
    GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
#  Le plugin
# ---------------------------------------------------------------------------
juce_add_plugin(HardTune
    PRODUCT_NAME                "HardTune"
    COMPANY_NAME                "HardTune (open source)"
    BUNDLE_ID                   "org.hardtune.plugin"
    PLUGIN_MANUFACTURER_CODE    Htun
    PLUGIN_CODE                 Ht01
    FORMATS                     VST3 Standalone
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE)

target_sources(HardTune PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/dsp/TuneCore.cpp
    Source/dsp/TuneCore.h
    Source/dsp/PitchDetector.cpp
    Source/dsp/PitchDetector.h
    Source/dsp/NoteMapper.h
    Source/dsp/TuneMath.h
    Source/gui/HardTuneLookAndFeel.cpp
    Source/gui/HardTuneLookAndFeel.h
    Source/gui/PitchDisplay.cpp
    Source/gui/PitchDisplay.h)

target_compile_definitions(HardTune PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(HardTune
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
#  Tests hors DAW (aucune dépendance JUCE)
# ---------------------------------------------------------------------------
add_executable(hardtune_dsp_tests
    tests/dsp_offline.cpp
    Source/dsp/PitchDetector.cpp
    Source/dsp/TuneCore.cpp)

target_include_directories(hardtune_dsp_tests PRIVATE Source/dsp)
target_compile_features(hardtune_dsp_tests PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
#  Compilation Linux : compatibilité des vieilles distributions (glibc)
# ---------------------------------------------------------------------------
if (UNIX AND NOT APPLE)
    target_link_options(HardTune PRIVATE -static-libstdc++ -static-libgcc)
endif()
