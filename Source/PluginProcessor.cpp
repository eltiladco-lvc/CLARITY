#include "PluginProcessor.h"
#include "PluginEditor.h"

ClarityAudioProcessor::ClarityAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

void ClarityAudioProcessor::prepareToPlay (double, int)
{
    ringL.fill (0.0f);
    ringR.fill (0.0f);
    writePos.store (0);
}

bool ClarityAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void ClarityAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (getTotalNumInputChannels(), buffer.getNumChannels());
    const int n     = buffer.getNumSamples();
    if (numCh <= 0 || n <= 0) return;

    int pos = writePos.load (std::memory_order_relaxed);
    const float* inL = buffer.getReadPointer (0);
    const float* inR = buffer.getReadPointer (numCh > 1 ? 1 : 0);   // mono: R = L

    for (int i = 0; i < n; ++i)
    {
        ringL[(size_t) pos] = inL[i];
        ringR[(size_t) pos] = inR[i];
        pos = (pos + 1) & (kRingSize - 1);
    }
    writePos.store (pos, std::memory_order_release);
    // El buffer no se toca: el audio sale idéntico a como entró.
}

void ClarityAudioProcessor::copyLatest (float* mono, int n) const
{
    n = juce::jlimit (1, kRingSize, n);
    const int pos   = writePos.load (std::memory_order_acquire);
    const int start = (pos - n) & (kRingSize - 1);
    for (int i = 0; i < n; ++i)
    {
        const size_t k = (size_t) ((start + i) & (kRingSize - 1));
        mono[i] = 0.5f * (ringL[k] + ringR[k]);
    }
}

void ClarityAudioProcessor::copyLatestStereo (float* l, float* r, int n) const
{
    n = juce::jlimit (1, kRingSize, n);
    const int pos   = writePos.load (std::memory_order_acquire);
    const int start = (pos - n) & (kRingSize - 1);
    for (int i = 0; i < n; ++i)
    {
        const size_t k = (size_t) ((start + i) & (kRingSize - 1));
        l[i] = ringL[k];
        r[i] = ringR[k];
    }
}

juce::AudioProcessorEditor* ClarityAudioProcessor::createEditor()
{
    return new ClarityAudioProcessorEditor (*this);
}

// Punto de entrada del plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ClarityAudioProcessor();
}
