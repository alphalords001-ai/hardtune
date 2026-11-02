//  PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

// ---------------------------------------------------------------------------
//  Paramètres
// ---------------------------------------------------------------------------
AudioProcessorValueTreeState::ParameterLayout HardTuneProcessor::createParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;

    StringArray keys;
    for (int i = 0; i < 12; ++i)
        keys.add (tune::NoteMapper::noteName (i));

    layout.add (std::make_unique<AudioParameterChoice> (
        kKey, "Key", keys, 0));

    StringArray scales;
    for (int i = 0; i < (int) tune::NoteMapper::NumScales; ++i)
        scales.add (tune::NoteMapper::scaleName ((tune::NoteMapper::Scale) i));
    scales.add ("Custom");

    layout.add (std::make_unique<AudioParameterChoice> (
        kScale, "Scale", scales, (int) tune::NoteMapper::Major));

    layout.add (std::make_unique<AudioParameterFloat> (
        kRetune, "Retune Speed",
        NormalisableRange<float> (0.0f, 800.0f, 0.1f, 0.4f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        kAmount, "Amount",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        kFormant, "Formant",
        NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        kMix, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        kOutput, "Output",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterInt> (
        kTranspose, "Transpose", -6, 6, 0));

    layout.add (std::make_unique<AudioParameterBool> (kBypass, "Bypass", false));
    layout.add (std::make_unique<AudioParameterBool> (kCustomOn, "Custom scale", false));

    for (int i = 0; i < 12; ++i)
        layout.add (std::make_unique<AudioParameterBool> (
            String (kNotePrefix) + String (i),
            juce::String ("Note ") + tune::NoteMapper::noteName (i),
            true));

    return layout;
}

// ---------------------------------------------------------------------------
HardTuneProcessor::HardTuneProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
}

void HardTuneProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    coreL.prepare (sampleRate, samplesPerBlock);
    coreR.prepare (sampleRate, samplesPerBlock);
    coreR.setExternalPitchMode (true);

    setLatencySamples (coreL.latencySamples());

    syncEngineFromParameters();
}

bool HardTuneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    const auto& mainIn  = layouts.getMainInputChannelSet();

    if (mainOut != mainIn)
        return false;

    return mainOut == AudioChannelSet::mono() || mainOut == AudioChannelSet::stereo();
}

int HardTuneProcessor::getLatencyMs() const noexcept
{
    return (int) std::lround (1000.0 * (double) coreL.latencySamples() / currentSampleRate);
}

void HardTuneProcessor::setParamValue (const juce::String& id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        const float norm = p->convertTo0to1 (value);
        p->setValueNotifyingHost (norm);
    }
}

// ---------------------------------------------------------------------------
void HardTuneProcessor::syncEngineFromParameters()
{
    const int keyIndex   = apvts.getRawParameterValue (kKey)->load();
    const int scaleIndex = apvts.getRawParameterValue (kScale)->load();

    tune::TuneCore::Params p;
    p.amount       = apvts.getRawParameterValue (kAmount)->load()  * 0.01f;
    p.retuneMs     = apvts.getRawParameterValue (kRetune)->load();
    p.formant      = apvts.getRawParameterValue (kFormant)->load() * 0.01f;
    p.mix          = apvts.getRawParameterValue (kMix)->load()     * 0.01f;
    p.outputGainDb = apvts.getRawParameterValue (kOutput)->load();
    p.transpose    = (int) apvts.getRawParameterValue (kTranspose)->load();
    p.bypass       = apvts.getRawParameterValue (kBypass)->load() > 0.5f;

    const bool customOn = apvts.getRawParameterValue (kCustomOn)->load() > 0.5f;

    for (auto* core : { &coreL, &coreR })
    {
        core->setParams (p);

        auto& mapper = core->notes();
        mapper.setKey (keyIndex);

        if (customOn || scaleIndex >= (int) tune::NoteMapper::NumScales)
        {
            unsigned mask = 0u;
            for (int i = 0; i < 12; ++i)
                if (apvts.getRawParameterValue (String (kNotePrefix) + String (i))->load() > 0.5f)
                    mask |= (1u << i);
            if (mask == 0u) mask = 0xFFFu;
            mapper.setCustomMask (mask);
        }
        else
        {
            mapper.setScale ((tune::NoteMapper::Scale) scaleIndex);
        }
    }
}

// ---------------------------------------------------------------------------
void HardTuneProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midi);

    syncEngineFromParameters();

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    if (numSamples <= 0)
        return;

    // Canal gauche (ou canal unique) : il fait la détection.
    coreL.process (buffer.getReadPointer (0), buffer.getWritePointer (0), numSamples);

    if (numChannels > 1)
    {
        // Canal droit : même correction, détection fournie par le canal gauche.
        coreR.setExternalPitch ((double) coreL.getDetectedHz(), coreL.isVoiced());
        coreR.process (buffer.getReadPointer (1), buffer.getWritePointer (1), numSamples);
    }
}

// ---------------------------------------------------------------------------
void HardTuneProcessor::getStateInformation (MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void HardTuneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

// ---------------------------------------------------------------------------
AudioProcessorEditor* HardTuneProcessor::createEditor()
{
    return new HardTuneEditor (*this);
}

// ---------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HardTuneProcessor();
}
