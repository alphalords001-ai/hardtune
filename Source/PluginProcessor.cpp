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
        kHumanize, "Humanize",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 32.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        kVibrato, "Vibrato",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 20.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        kVibRate, "Vibrato Rate",
        NormalisableRange<float> (0.1f, 12.0f, 0.1f), 6.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        kOutput, "Output",
        NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterInt> (
        kTranspose, "Transpose", -6, 6, 0));

    layout.add (std::make_unique<AudioParameterBool> (kBypass, "Bypass", false));
    layout.add (std::make_unique<AudioParameterBool> (kDarkMode, "Dark mode", false));
    layout.add (std::make_unique<AudioParameterBool> (kCorrection, "Pitch correction", true));
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

    const int channels = juce::jmax (1, getTotalNumInputChannels());
    core.prepare (sampleRate, samplesPerBlock, channels);
    setLatencySamples (core.latencySamples());

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
    return (int) std::lround (1000.0 * (double) core.latencySamples() / currentSampleRate);
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
    const bool corrOn = apvts.getRawParameterValue (kCorrection)->load() > 0.5f;
    p.amount       = corrOn ? apvts.getRawParameterValue (kAmount)->load() * 0.01f : 0.0f;
    p.retuneMs     = apvts.getRawParameterValue (kRetune)->load();
    p.formant      = apvts.getRawParameterValue (kFormant)->load() * 0.01f;
    p.humanize     = apvts.getRawParameterValue (kHumanize)->load() * 0.01f;
    p.vibrato      = apvts.getRawParameterValue (kVibrato)->load()  * 0.01f;
    p.vibratoHz    = apvts.getRawParameterValue (kVibRate)->load();
    p.mix          = 1.0f;                       // plus de MIX : il doublait la voix
    p.outputGainDb = apvts.getRawParameterValue (kOutput)->load();
    p.transpose    = (int) apvts.getRawParameterValue (kTranspose)->load();
    p.bypass       = apvts.getRawParameterValue (kBypass)->load() > 0.5f;

    const bool customOn = apvts.getRawParameterValue (kCustomOn)->load() > 0.5f;

    {
        core.setParams (p);

        auto& mapper = core.notes();
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

    // Un seul moteur pour tous les canaux : gauche et droite sont traitées
    // ensemble, donc un signal mono reste parfaitement mono.
    const float* inPtrs[2]  = { buffer.getReadPointer (0),
                                numChannels > 1 ? buffer.getReadPointer (1)
                                                : buffer.getReadPointer (0) };
    float* outPtrs[2]       = { buffer.getWritePointer (0),
                                numChannels > 1 ? buffer.getWritePointer (1)
                                                : buffer.getWritePointer (0) };

    core.process (inPtrs, outPtrs, numSamples, numChannels);

    // ---- oscilloscope : on ne garde qu'un point sur N pour ne pas saturer ----
    {
        const int step = juce::jmax (1, numSamples / 64);
        int w = scopeWrite.load (std::memory_order_relaxed);
        for (int i = 0; i < numSamples; i += step)
        {
            scopeBuf[(size_t) (w++ & (kScopeSize - 1))] = outPtrs[0][i];
        }
        scopeWrite.store (w, std::memory_order_relaxed);

        const int cw = centsWrite.load (std::memory_order_relaxed);
        centsBuf[(size_t) (cw & (kCentsCount - 1))] = core.getCentsError();
        centsWrite.store (cw + 1, std::memory_order_relaxed);
    }
}

void HardTuneProcessor::copyScope (float* dest) const noexcept
{
    const int w = scopeWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < kScopeSize; ++i)
        dest[i] = scopeBuf[(size_t) ((w + i) & (kScopeSize - 1))];
}

void HardTuneProcessor::copyCents (float* dest) const noexcept
{
    const int w = centsWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < kCentsCount; ++i)
        dest[i] = centsBuf[(size_t) ((w + i) & (kCentsCount - 1))];
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
