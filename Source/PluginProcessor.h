//  PluginProcessor.h — enveloppe JUCE autour du moteur TuneCore.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <vector>
#include <atomic>

#include "dsp/TuneCore.h"

class HardTuneProcessor : public juce::AudioProcessor
{
public:
    HardTuneProcessor();
    ~HardTuneProcessor() override = default;

    // ---- AudioProcessor ----------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                     { return true; }

    const juce::String getName() const override         { return "HardTune"; }
    bool acceptsMidi() const override                   { return false; }
    bool producesMidi() const override                  { return false; }
    bool isMidiEffect() const override                  { return false; }
    double getTailLengthSeconds() const override        { return 0.05; }

    int getNumPrograms() override                       { return 1; }
    int getCurrentProgram() override                    { return 0; }
    void setCurrentProgram (int) override               {}
    const juce::String getProgramName (int) override    { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- Paramètres --------------------------------------------------------
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    // Identifiants (utilisés par l'éditeur et les presets)
    static constexpr const char* kKey      = "key";
    static constexpr const char* kScale    = "scale";
    static constexpr const char* kRetune   = "retune";
    static constexpr const char* kAmount   = "amount";
    static constexpr const char* kFormant  = "formant";
    static constexpr const char* kHumanize = "humanize";
    static constexpr const char* kVibrato  = "vibrato";
    static constexpr const char* kVibRate  = "vibratoRate";
    static constexpr const char* kDarkMode = "darkMode";
    static constexpr const char* kCorrection = "correctionOn";
    static constexpr const char* kOutput   = "output";
    static constexpr const char* kTranspose= "transpose";
    static constexpr const char* kBypass   = "bypass";
    static constexpr const char* kCustomOn = "customOn";
    static constexpr const char* kNotePrefix = "note";   // note0 .. note11

    // ---- Informations pour l'interface (thread-safe) -----------------------
    float getDetectedHz()  const noexcept { return core.getDetectedHz(); }
    float getDetectedMidi()const noexcept { return core.getDetectedMidi(); }
    int   getTargetNote()  const noexcept { return core.getTargetNote(); }
    float getCentsError()  const noexcept { return core.getCentsError(); }
    bool  isVoiced()       const noexcept { return core.isVoiced(); }

    // ---- vu-mètres (dB, thread-safe) --------------------------------------
    float getInputPeakDb()  const noexcept { return core.getInputPeakDb(); }
    float getInputRmsDb()   const noexcept { return core.getInputRmsDb(); }
    float getOutputPeakDb() const noexcept { return core.getOutputPeakDb(); }
    float getOutputRmsDb()  const noexcept { return core.getOutputRmsDb(); }
    int   getLatencyMs()   const noexcept;

    // ---- oscilloscope pour l'interface -------------------------------------
    static constexpr int kScopeSize = 2048;
    /** Copie le tampon circulaire de l'oscilloscope dans `dest` (kScopeSize valeurs). */
    void copyScope (float* dest) const noexcept;
    /** Historique de la correction en cents (une valeur par bloc). */
    static constexpr int kCentsCount = 256;
    void copyCents (float* dest) const noexcept;

    /** Applique une valeur "métier" à un paramètre (utilisé par les presets). */
    void setParamValue (const juce::String& id, float value);

private:
    void syncEngineFromParameters();

    tune::TuneCore core;

    // ATTENTION : ne JAMAIS initialiser un vecteur de floats avec des accolades.
    // "v { n, 0.0f }" cree un vecteur de 2 elements (n et 0.0f) au lieu de n :
    // processBlock ecrivait alors jusqu'a l'indice 2047 dans 2 emplacements ->
    // corruption du tas, plantage dans ntdll ("Write of address 0x24").
    std::vector<float> scopeBuf = std::vector<float> ((size_t) kScopeSize, 0.0f);
    std::atomic<int>   scopeWrite { 0 };
    std::vector<float> centsBuf = std::vector<float> ((size_t) kCentsCount, 0.0f);
    std::atomic<int>   centsWrite { 0 };
    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HardTuneProcessor)
};
