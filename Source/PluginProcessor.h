#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

//  CLARITY: analizador de espectro + CampoStereo. El audio pasa SIN modificarse.
class ClarityAudioProcessor : public juce::AudioProcessor
{
public:
    ClarityAudioProcessor();
    ~ClarityAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "CLARITY"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& dest) override
    {
        const char v[2] = { (char) uiStereoOpen, (char) uiStereoDesign };
        dest.append (v, 2);
    }
    void setStateInformation (const void* data, int size) override
    {
        if (size >= 2)
        {
            auto* p = static_cast<const char*> (data);
            uiStereoOpen   = p[0] != 0 ? 1 : 0;
            uiStereoDesign = juce::jlimit (0, 2, (int) p[1]);
        }
    }

    // Copia las últimas n muestras. Mono = (L+R)/2
    void copyLatest (float* mono, int n) const;
    void copyLatestStereo (float* l, float* r, int n) const;

    // Estado de la interfaz (se recuerda al cerrar/abrir la ventana)
    int uiStereoOpen = 0, uiStereoDesign = 0;

private:
    static constexpr int kRingSize = 32768;           // potencia de 2
    std::array<float, kRingSize> ringL {}, ringR {};
    std::atomic<int> writePos { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClarityAudioProcessor)
};
