#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#include <vector>
#include "UIConstants.h"

class StereoPanel : public juce::Component
{
public:
    StereoPanel();

    void process (const float* l, const float* r, int n);
    void setDesign (int d) { design = juce::jlimit (0, 2, d); repaint(); }
    int  getDesign() const { return design; }
    std::function<void (int)> onDesignChanged;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void drawPolar (juce::Graphics&);
    void drawScope (juce::Graphics&);
    void drawVU (juce::Graphics&);

    int design = 0;
    juce::Font smallFont, axisFont, bigFont;

    std::array<float, clarity::ui::kPolarBins> polar {};
    std::array<float, 4> vuTpDb { -60.0f, -60.0f, -60.0f, -60.0f };
    std::vector<juce::Point<float>> pts, prevPts;
    float corr = 0.0f, peakHold = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereoPanel)
};
