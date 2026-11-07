#pragma once
#include <juce_graphics/juce_graphics.h>

namespace clarity::ui
{
    constexpr float kW = 680.0f;
    constexpr float kH = 204.0f;
    constexpr float kPX = 46.0f;
    constexpr float kBW = 20.0f;
    constexpr float kRight = 664.0f;
    constexpr float kY0 = 52.0f;
    constexpr float kY1 = 178.0f;
    constexpr int kNumBars = 31;
    constexpr float kDbMin = -40.0f;
    constexpr float kDbStep = 5.0f;
    constexpr float kDisplayOffsetDb = 0.0f;
    constexpr float kHButton = 28.0f;
    constexpr float kPanelH = 208.0f;
    constexpr float kHClosed = kH + kHButton;
    constexpr float kHOpen = kHClosed + kPanelH;
    constexpr int kStereoBlock = 1024;
    constexpr int kPolarBins = 60;
    constexpr float kPolarRangeDb = 50.0f;
    constexpr float kVuRefDbFS = -18.0f;
    constexpr float kLowZoneHz = 123.0f;
    constexpr float kHighZoneHz = 4500.0f;
    constexpr bool kHoverShowsBarLevel = true;

    // More relaxed, vintage-style spectrum ballistics at the 60 Hz UI refresh rate.
    constexpr float kAttack = 0.62f;
    constexpr float kReleaseDbPF = 0.16f;
    constexpr float kPeakHoldReleaseDbPF = 0.07f;

    inline const char* kTitle = "CLARITY";
    inline const char* kSubtitle = "Powered By Tiladco";

    inline const juce::Colour bg { 0xff0b101c };
    inline const juce::Colour border { 0xff1d2840 };
    inline const juce::Colour grid { 0xff18233a };
    inline const juce::Colour label { 0xffa9b6d3 };
    inline const juce::Colour powered { 0xff5a6f96 };
    inline const juce::Colour readout { 0xffdce7ff };
    inline const juce::Colour ink { 0xff0a0f1c };
    inline const juce::Colour btn { 0xff35405a };
    inline const juce::Colour btnLine { 0xff252d42 };
    inline const juce::Colour btnText { 0xffeaf1ff };
    inline const juce::Colour dark { 0xff080c16 };
    inline const juce::Colour faceTop { 0xff141b2e };
    inline const juce::Colour white { 0xffffffff };
    inline const juce::Colour low { 0xffe040fb };
    inline const juce::Colour blue { 0xff3887ff };
    inline const juce::Colour mid { 0xff00f0c8 };
    inline const juce::Colour high { 0xffffa90a };

    inline juce::Colour titleLetterColour (int i)
    {
        return i < 2 ? low : (i < 5 ? mid : high);
    }
}
