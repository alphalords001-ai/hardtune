//  PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Diag.h"

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
    ht::diag::install();
    ht::diag::log ("processeur : construction");
}

void HardTuneProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Un hote peut appeler prepareToPlay avec des valeurs limites (0 canal sur
    // une piste vide, sampleRate 0 pendant le scan...). On borne, et surtout on
    // ne laisse AUCUNE exception remonter : sur Windows, une exception qui
    // traverse l'interface VST3 ferme le DAW sans message.
    // Les tampons du moteur sont realloues ici : si l'hote appelait prepareToPlay
    // pendant un bloc audio (certains le font au demarrage de la lecture), le
    // fil audio lirait de la memoire liberee. Le verrou rend les deux exclusifs.
    ht::diag::stage ("prepareToPlay");
    ht::diag::log ("prepareToPlay");

    const juce::ScopedLock sl (getCallbackLock());

    currentSampleRate = (sampleRate > 0.0 ? sampleRate : 44100.0);
    currentBlockSize  = juce::jmax (16, samplesPerBlock);

    const int channels = juce::jlimit (1, 2, juce::jmax (1, getTotalNumInputChannels()));

    try
    {
        core.prepare (currentSampleRate, currentBlockSize, channels);
        setLatencySamples (core.latencySamples());
        syncEngineFromParameters();
    }
    catch (const std::exception& e)
    {
        // Le plugin reste charge et transparent plutot que de tuer l'hote.
        ht::diag::log (("EXCEPTION dans prepareToPlay : " + juce::String (e.what())).toRawUTF8());
        DBG ("HardTune: prepareToPlay a echoue - " << e.what());
        try { core.prepare (currentSampleRate, 512, 1); } catch (...) {}
        setLatencySamples (0);
    }
    catch (...)
    {
        setLatencySamples (0);
    }
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
    // Lecture tolerante : si un identifiant n'existe pas (ancien preset, etat
    // restaure d'une version precedente), on retombe sur une valeur par defaut
    // au lieu de dereferencer un pointeur nul.
    auto raw = [this] (const juce::String& id, float fallback = 0.0f) -> float
    {
        if (auto* p = apvts.getRawParameterValue (id))
            return p->load();
        return fallback;
    };

    const int keyIndex   = (int) raw (kKey);
    const int scaleIndex = (int) raw (kScale);

    tune::TuneCore::Params p;
    const bool corrOn = raw (kCorrection, 1.0f) > 0.5f;
    p.amount       = corrOn ? raw (kAmount, 100.0f) * 0.01f : 0.0f;
    p.retuneMs     = raw (kRetune);
    p.formant      = raw (kFormant) * 0.01f;
    p.humanize     = raw (kHumanize, 32.0f) * 0.01f;
    p.vibrato      = raw (kVibrato, 20.0f) * 0.01f;
    p.vibratoHz    = raw (kVibRate, 6.0f);
    p.mix          = 1.0f;                       // plus de MIX : il doublait la voix
    p.outputGainDb = raw (kOutput);
    p.transpose    = (int) raw (kTranspose);
    p.bypass       = raw (kBypass) > 0.5f;

    const bool customOn = raw (kCustomOn) > 0.5f;

    {
        core.setParams (p);

        auto& mapper = core.notes();
        mapper.setKey (keyIndex);

        if (customOn || scaleIndex >= (int) tune::NoteMapper::NumScales)
        {
            unsigned mask = 0u;
            for (int i = 0; i < 12; ++i)
                if (raw (String (kNotePrefix) + String (i), 1.0f) > 0.5f)
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

    ht::diag::logOnce ("premier appel a processBlock");
    ht::diag::stage ("processBlock");

    const juce::ScopedLock sl (getCallbackLock());

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // Rien a traiter, ou tampon sans canal : ne jamais demander getReadPointer(0).
    if (numSamples <= 0 || numChannels <= 0)
        return;

    try
    {
        syncEngineFromParameters();
    }
    catch (...)
    {
        return;
    }

    // Un seul moteur pour tous les canaux : gauche et droite sont traitées
    // ensemble, donc un signal mono reste parfaitement mono.
    const float* inPtrs[2]  = { buffer.getReadPointer (0),
                                numChannels > 1 ? buffer.getReadPointer (1)
                                                : buffer.getReadPointer (0) };
    float* outPtrs[2]       = { buffer.getWritePointer (0),
                                numChannels > 1 ? buffer.getWritePointer (1)
                                                : buffer.getWritePointer (0) };

    try
    {
        core.process (inPtrs, outPtrs, numSamples, numChannels);
    }
    catch (...)
    {
        ht::diag::logOnce ("EXCEPTION rattrapee dans processBlock");
        return;   // on laisse le tampon tel quel : silence plutot que plantage
    }

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
    try
    {
        if (auto xml = apvts.copyState().createXml())
            copyXmlToBinary (*xml, destData);
    }
    catch (...) {}
}

void HardTuneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    try
    {
        if (auto xml = getXmlFromBinary (data, sizeInBytes))
            if (xml->hasTagName (apvts.state.getType()))
                apvts.replaceState (ValueTree::fromXml (*xml));
    }
    catch (...) {}
}

// ---------------------------------------------------------------------------
AudioProcessorEditor* HardTuneProcessor::createEditor()
{
    ht::diag::log ("createEditor");

    try
    {
        return new HardTuneEditor (*this);
    }
    catch (...)
    {
        return nullptr;   // l'hote affichera une fenetre vide, pas un crash
    }
}

// ---------------------------------------------------------------------------
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HardTuneProcessor();
}
