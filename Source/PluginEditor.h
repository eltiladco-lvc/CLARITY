#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "StereoPanel.h"
#include "UIConstants.h"

class ClarityAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit ClarityAudioProcessorEditor (ClarityAudioProcessor&);
    ~ClarityAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void analyse();
    void setStereoOpen (bool open);

    static constexpr int kFFTOrder = 13;
    static constexpr int kFFTSize  = 1 << kFFTOrder;

    ClarityAudioProcessor& processor;
    juce::ComponentBoundsConstrainer constrainer;

    juce::dsp::FFT fft { kFFTOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kFFTSize,
                                                 juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> fftData;
    float windowSum = 1.0f;

    std::array<float, clarity::ui::kNumBars> level {};
    std::array<float, clarity::ui::kNumBars> peakHoldDb {};

    juce::Font titleFont, subFont, axisFont;
    juce::Image logo;
    bool hover = false;
    float hoverX = 0.0f, hoverY = 0.0f;
    int hoverReadoutBar = -1;
    float hoverReadoutDb = -90.0f;
    int hoverReadoutHoldTicks = 0;
    bool monochromeBars = false;

    StereoPanel panel;
    bool stereoOpen = false;
    std::vector<float> blkL, blkR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ClarityAudioProcessorEditor)
};
