//  PluginProcessor.h — enveloppe JUCE autour du moteur TuneCore.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

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
    static constexpr const char* kMix      = "mix";
    static constexpr const char* kOutput   = "output";
    static constexpr const char* kTranspose= "transpose";
    static constexpr const char* kBypass   = "bypass";
    static constexpr const char* kCustomOn = "customOn";
    static constexpr const char* kNotePrefix = "note";   // note0 .. note11

    // ---- Informations pour l'interface (thread-safe) -----------------------
    float getDetectedHz()  const noexcept { return coreL.getDetectedHz(); }
    float getDetectedMidi()const noexcept { return coreL.getDetectedMidi(); }
    int   getTargetNote()  const noexcept { return coreL.getTargetNote(); }
    float getCentsError()  const noexcept { return coreL.getCentsError(); }
    bool  isVoiced()       const noexcept { return coreL.isVoiced(); }
    int   getLatencyMs()   const noexcept;

    /** Applique une valeur "métier" à un paramètre (utilisé par les presets). */
    void setParamValue (const juce::String& id, float value);

private:
    void syncEngineFromParameters();

    tune::TuneCore coreL, coreR;
    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HardTuneProcessor)
};
