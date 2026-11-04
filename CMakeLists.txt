cmake_minimum_required(VERSION 3.22)

project(HardTune VERSION 0.9.3 LANGUAGES C CXX)

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
#  Windows : NOMINMAX supprime les macros min/max de Windows.h, qui entrent en
#  conflit avec std::min / std::max (erreur MSVC C2589). Laissé en place : le
#  moteur actuel n'en dépend plus, mais JUCE et les en-têtes système si.
# ---------------------------------------------------------------------------
if (WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# ---------------------------------------------------------------------------
#  Le plugin
# ---------------------------------------------------------------------------
juce_add_plugin(HardTune
    #  Nom commercial du produit (visible dans le navigateur de plugins du DAW)
    #  et nom du vendeur (colonne "Vendor" / "Editeur").
    PRODUCT_NAME                "Alpha-Tune"
    COMPANY_NAME                "Alpha Production"
    BUNDLE_ID                   "org.hardtune.plugin"
    PLUGIN_MANUFACTURER_CODE    Htun
    PLUGIN_CODE                 Ht01
    VERSION                     0.9.2
    FORMATS                     VST3 Standalone
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE)

target_sources(HardTune PRIVATE
    Source/Diag.cpp
    Source/Diag.h
    Source/SafeEditor.h
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/dsp/TuneCore.cpp
    Source/dsp/TuneCore.h
    Source/dsp/PitchDetector.cpp
    Source/dsp/PitchDetector.h
    Source/dsp/LiveEngine.cpp
    Source/dsp/LiveEngine.h
    Source/dsp/NoteMapper.h
    Source/dsp/TuneMath.h
    Source/gui/HardTuneLookAndFeel.cpp
    Source/gui/HardTuneLookAndFeel.h
    Source/gui/Theme.h
    Source/gui/Widgets.h
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
    Source/dsp/LiveEngine.cpp
    Source/dsp/TuneCore.cpp)

target_include_directories(hardtune_dsp_tests PRIVATE Source/dsp)
target_compile_features(hardtune_dsp_tests PRIVATE cxx_std_17)

# ---------------------------------------------------------------------------
#  MSVC : deux réglages indispensables pour ne PLUS faire tomber le DAW
#
#  1. /EHsc (valeur par défaut de JUCE) : /EHa a été RETIRÉ en v0.5.6.
#     C'était le seul changement de génération de code présent dans toutes les
#     versions qui ferment le DAW au chargement (v0.5.4, v0.5.5) et absent de la
#     v0.5.2, qui se chargeait normalement. /EHa fait poser des tables SEH sur
#     CHAQUE fonction du binaire ; c'est le suspect n°1 de la fermeture au
#     chargement. On revient au comportement standard pour lever le doute.
#  2. /MAP : le fichier .map permet de traduire l'offset d'un rapport de crash
#     ("in module HardTune.vst3, offset D5356") en nom de fonction.
# ---------------------------------------------------------------------------
if (MSVC)

    foreach (ht_target HardTune HardTune_VST3 HardTune_Standalone)
        if (TARGET ${ht_target})
            target_link_options (${ht_target} PRIVATE
                "/MAP:$<TARGET_FILE_DIR:${ht_target}>/HardTune-${ht_target}.map"
                /MAPINFO:EXPORTS)
        endif()
    endforeach()
endif()

# ---------------------------------------------------------------------------
#  Compilation Linux : compatibilité des vieilles distributions (glibc)
# ---------------------------------------------------------------------------
if (UNIX AND NOT APPLE)
    target_link_options(HardTune PRIVATE -static-libstdc++ -static-libgcc)
endif()
