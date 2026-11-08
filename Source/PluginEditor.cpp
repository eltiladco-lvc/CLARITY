#include "PluginEditor.h"
#include <BinaryData.h>
#include <cmath>

using namespace clarity::ui;

namespace
{
    enum class Align { left, centre, right };

    juce::Typeface::Ptr loadTypeface (const char* resourceName)
    {
        int size = 0;
        if (const char* data = BinaryData::getNamedResource (resourceName, size))
            return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
        return nullptr;
    }

    juce::Font makeFont (juce::Typeface::Ptr tf, const char* fallbackName, float px)
    {
        if (tf != nullptr)
            return juce::Font (juce::FontOptions (tf)).withPointHeight (px);
        return juce::Font (juce::FontOptions (fallbackName, px, juce::Font::plain)).withPointHeight (px);
    }

    float textWidth (const juce::Font& f, const juce::String& t)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, t, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }

    void drawText (juce::Graphics& g, const juce::Font& f, const juce::String& t,
                   float x, float baseline, Align a)
    {
        const float w = textWidth (f, t);
        if (a == Align::centre) x -= w * 0.5f;
        else if (a == Align::right) x -= w;
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, t, x, baseline);
        ga.draw (g);
    }

    constexpr double kMinSpectrumHz = 20.0;
    constexpr double kMaxSpectrumHz = 20000.0;
    const double kSpectrumOctaves = std::log2 (kMaxSpectrumHz / kMinSpectrumHz);

    // Bar centre frequencies span the full range: bar 1 is 20 Hz and bar 62 is 20 kHz.
    double barFreq (int i)
    {
        const double t = (double) i / (double) (kNumBars - 1);
        return kMinSpectrumHz * std::pow (kMaxSpectrumHz / kMinSpectrumHz, t);
    }

    // Adjacent bands meet at their geometric midpoint; the first/last bands
    // reach exactly 20 Hz and 20 kHz without leaving unanalysed space.
    double barEdgeFreq (int edgeIndex)
    {
        const double t = (double) edgeIndex / (double) kNumBars;
        return kMinSpectrumHz * std::pow (kMaxSpectrumHz / kMinSpectrumHz, t);
    }

    float freqToX (float hz)
    {
        const double t = std::log2 ((double) hz / kMinSpectrumHz) / kSpectrumOctaves;
        return kPX + kBW * 0.5f + (float) t * (kRight - kPX - kBW);
    }

    float xToFreq (float x)
    {
        const double t = ((double) x - (kPX + kBW * 0.5f)) / (kRight - kPX - kBW);
        return (float) (kMinSpectrumHz * std::pow (2.0, t * kSpectrumOctaves));
    }

    int xToBar (float x)
    {
        return juce::jlimit (0, kNumBars - 1,
                             juce::roundToInt ((x - kPX - kBW * 0.5f) / kBW));
    }

    juce::Colour zoneColour (double hz)
    {
        if (hz < kLowZoneHz)  return low;
        if (hz < kHighZoneHz) return mid;
        return high;
    }

    double smoothStep (double t)
    {
        t = juce::jlimit (0.0, 1.0, t);
        return t * t * (3.0 - 2.0 * t);
    }

    double logRamp (double hz, double startHz, double endHz, double maxDb)
    {
        if (hz <= startHz) return 0.0;
        if (hz >= endHz) return maxDb;
        const double t = std::log (hz / startHz) / std::log (endHz / startHz);
        return maxDb * smoothStep (t);
    }

    double localPresenceLiftDb (double hz)
    {
        constexpr double fadeInStartHz = 900.0;
        constexpr double flatStartHz = 1000.0;
        constexpr double flatEndHz = 1300.0;
        constexpr double fadeOutEndHz = 1450.0;
        constexpr double liftDb = 3.0;
        if (hz <= fadeInStartHz || hz >= fadeOutEndHz) return 0.0;
        const double attack = smoothStep (std::log (hz / fadeInStartHz) / std::log (flatStartHz / fadeInStartHz));
        const double release = 1.0 - smoothStep (std::log (hz / flatEndHz) / std::log (fadeOutEndHz / flatEndHz));
        return liftDb * juce::jlimit (0.0, 1.0, attack) * juce::jlimit (0.0, 1.0, release);
    }

    double upperPresenceLiftDb (double hz)
    {
        constexpr double fadeInStartHz = 8700.0;
        constexpr double flatStartHz = 9500.0;
        constexpr double flatEndHz = 10200.0;
        constexpr double fadeOutEndHz = 11700.0;
        constexpr double liftDb = 2.0;
        if (hz <= fadeInStartHz || hz >= fadeOutEndHz) return 0.0;
        const double attack = smoothStep (std::log (hz / fadeInStartHz) / std::log (flatStartHz / fadeInStartHz));
        const double release = 1.0 - smoothStep (std::log (hz / flatEndHz) / std::log (fadeOutEndHz / flatEndHz));
        return liftDb * juce::jlimit (0.0, 1.0, attack) * juce::jlimit (0.0, 1.0, release);
    }

    double lowBandReductionDb (double hz)
    {
        constexpr double fadeInStartHz = 42.0;
        constexpr double flatStartHz = 50.0;
        constexpr double flatEndHz = 100.0;
        constexpr double fadeOutEndHz = 120.0;
        constexpr double reductionDb = -5.0;
        if (hz <= fadeInStartHz || hz >= fadeOutEndHz) return 0.0;
        if (hz >= flatStartHz && hz <= flatEndHz) return reductionDb;
        if (hz < flatStartHz)
        {
            const double t = std::log (hz / fadeInStartHz) / std::log (flatStartHz / fadeInStartHz);
            return reductionDb * smoothStep (t);
        }
        const double t = std::log (fadeOutEndHz / hz) / std::log (fadeOutEndHz / flatEndHz);
        return reductionDb * smoothStep (t);
    }
}

ClarityAudioProcessorEditor::ClarityAudioProcessorEditor (ClarityAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), fftData ((size_t) kFFTSize * 2, 0.0f),
      blkL ((size_t) kStereoBlock, 0.0f), blkR ((size_t) kStereoBlock, 0.0f)
{
    level.fill (-90.0f);
    peakHoldDb.fill (-90.0f);
    std::vector<float> ones ((size_t) kFFTSize, 1.0f);
    window.multiplyWithWindowingTable (ones.data(), (size_t) kFFTSize);
    windowSum = 0.0f;
    for (float v : ones) windowSum += v;

    auto titleTf = loadTypeface ("Hoshiko_ttf");
    if (titleTf == nullptr) titleTf = loadTypeface ("YujiBoku_ttf");
    titleFont = makeFont (titleTf, "Brush Script MT", 30.0f).withExtraKerningFactor (2.0f / 30.0f);
    subFont = makeFont (loadTypeface ("GreatVibes_ttf"), "Segoe Script", 22.0f);
    axisFont = makeFont (nullptr, "Segoe UI", 11.0f);

    int logoSize = 0;
    if (const char* d = BinaryData::getNamedResource ("homesounds_png", logoSize))
        logo = juce::ImageCache::getFromMemory (d, logoSize);

    setOpaque (true);
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    stereoOpen = processor.uiStereoOpen != 0;
    panel.setDesign (processor.uiStereoDesign);
    panel.onDesignChanged = [this] (int d) { processor.uiStereoDesign = d; };
    addChildComponent (panel);
    panel.setVisible (stereoOpen);

    const float H = stereoOpen ? kHOpen : kHClosed;
    constrainer.setSizeLimits ((int) (kW * 0.5f), (int) (H * 0.5f), (int) (kW * 2.0f), (int) (H * 2.0f));
    constrainer.setFixedAspectRatio ((double) kW / (double) H);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize ((int) kW, (int) H);
    startTimerHz (60);
}

ClarityAudioProcessorEditor::~ClarityAudioProcessorEditor()
{
    stopTimer();
}

void ClarityAudioProcessorEditor::timerCallback()
{
    analyse();
    if (hover)
    {
        const int bar = xToBar (hoverX);
        const float bandDb = level[(size_t) bar];
        if (bar != hoverReadoutBar)
        {
            hoverReadoutBar = bar;
            hoverReadoutDb = bandDb;
            hoverReadoutHoldTicks = 150;
        }
        else if (bandDb >= hoverReadoutDb)
        {
            hoverReadoutDb = bandDb;
            hoverReadoutHoldTicks = 150;
        }
        else if (hoverReadoutHoldTicks > 0)
            --hoverReadoutHoldTicks;
        else
            hoverReadoutDb = juce::jmax (bandDb, hoverReadoutDb - 0.1f);
    }
    else
    {
        hoverReadoutBar = -1;
        hoverReadoutDb = -90.0f;
        hoverReadoutHoldTicks = 0;
    }

    const float sc = (float) getWidth() / kW;
    repaint (0, 0, getWidth(), juce::roundToInt (kHClosed * sc));
    if (stereoOpen)
    {
        processor.copyLatestStereo (blkL.data(), blkR.data(), kStereoBlock);
        panel.process (blkL.data(), blkR.data(), kStereoBlock);
    }
}

void ClarityAudioProcessorEditor::resized()
{
    const int y = juce::roundToInt (kHClosed * (float) getWidth() / kW);
    panel.setBounds (0, y, getWidth(), juce::jmax (0, getHeight() - y));
}

void ClarityAudioProcessorEditor::setStereoOpen (bool open)
{
    stereoOpen = open;
    processor.uiStereoOpen = open ? 1 : 0;
    const float H = open ? kHOpen : kHClosed;
    constrainer.setSizeLimits ((int) (kW * 0.5f), (int) (H * 0.5f), (int) (kW * 2.0f), (int) (H * 2.0f));
    constrainer.setFixedAspectRatio ((double) kW / (double) H);
    panel.setVisible (open);
    setSize (getWidth(), juce::roundToInt ((float) getWidth() * H / kW));
    repaint();
}

void ClarityAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const float sc = (float) getWidth() / kW;
    const juce::Rectangle<float> btnR (270.0f, 208.0f, 140.0f, 20.0f);
    if (btnR.contains (e.position / sc))
        setStereoOpen (! stereoOpen);
}

void ClarityAudioProcessorEditor::analyse()
{
    processor.copyLatest (fftData.data(), kFFTSize);
    std::fill (fftData.begin() + kFFTSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) kFFTSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const double sr = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 44100.0;
    const double binHz = sr / (double) kFFTSize;
    const int maxBin = kFFTSize / 2 - 1;
    const double norm = 2.0 / (double) windowSum;

    const auto midCorrectionDb = [] (double hz)
    {
        constexpr double fadeInHz = 43.0, fullStartHz = 55.0, fullEndHz = 900.0, fadeOutHz = 1100.0;
        constexpr double gainDb = 5.0;
        if (hz < fadeInHz || hz >= fadeOutHz) return 0.0;
        if (hz >= fullStartHz && hz <= fullEndHz) return gainDb;
        if (hz < fullStartHz)
            return gainDb * smoothStep (std::log (hz / fadeInHz) / std::log (fullStartHz / fadeInHz));
        return gainDb * smoothStep (std::log (fadeOutHz / hz) / std::log (fadeOutHz / fullEndHz));
    };

    const auto highBandCorrectionDb = [] (double hz)
    {
        return logRamp (hz, 1000.0, 2000.0, 10.0);
    };

    for (int i = 0; i < kNumBars; ++i)
    {
        const double fc = barFreq (i);
        const double bLo = barEdgeFreq (i) / binHz;
        const double bHi = barEdgeFreq (i + 1) / binHz;
        float db = -100.0f;

        if (bLo <= (double) maxBin)
        {
            const int b0 = juce::jlimit (0, maxBin, (int) std::ceil (bLo));
            const int b1 = juce::jlimit (0, maxBin, (int) std::floor (bHi));
            double power = 0.0;
            if (b1 >= b0)
            {
                for (int b = b0; b <= b1; ++b)
                {
                    const double a = (double) fftData[(size_t) b] * norm;
                    power += a * a;
                }
                power /= 1.5;
            }
            else
            {
                const int b = juce::jlimit (0, maxBin, (int) std::lround (0.5 * (bLo + bHi)));
                const double a = (double) fftData[(size_t) b] * norm;
                power = a * a;
            }

            db = (float) (10.0 * std::log10 (power + 1.0e-12)
                          + midCorrectionDb (fc)
                          + highBandCorrectionDb (fc)
                          + localPresenceLiftDb (fc)
                          + upperPresenceLiftDb (fc)
                          + lowBandReductionDb (fc)
                          + kDisplayOffsetDb);
        }

        float& current = level[(size_t) i];
        const float previous = current;
        if (db >= previous)
            current = previous + (db - previous) * kAttack;
        else
            current = juce::jmax (db, previous - kReleaseDbPF);

        float& heldPeak = peakHoldDb[(size_t) i];
        heldPeak = db >= heldPeak ? db : juce::jmax (db, heldPeak - kPeakHoldReleaseDbPF);
    }
}

void ClarityAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / kW));

    g.setColour (bg);
    g.fillRoundedRectangle (10.0f, 46.0f, 660.0f, 150.0f, 6.0f);
    g.setColour (border);
    g.drawRoundedRectangle (10.0f, 46.0f, 660.0f, 150.0f, 6.0f, 2.0f);

    for (int d = 0; d >= (int) kDbMin; d -= (int) kDbStep)
    {
        const float y = kY0 + (float) (-d) / (-kDbMin) * (kY1 - kY0);
        const bool highlight = (d == -10);
        g.setColour (highlight ? grid.brighter (0.5f) : grid);
        g.drawLine (kPX, y, kRight, y, highlight ? 1.6f : 1.0f);
        g.setColour (highlight ? readout : label);
        const juce::String tickLabel (d);
        drawText (g, axisFont, tickLabel, 40.0f, y + 4.0f, Align::right);
        drawText (g, axisFont, tickLabel, 678.0f, y + 4.0f, Align::right);
    }

    if (logo.isValid())
    {
        g.setOpacity (0.45f);
        g.drawImage (logo,
                     juce::Rectangle<float> (kPX + (kRight - kPX - 356.0f) * 0.5f, kY0 - 4.0f, 356.0f, 126.0f),
                     juce::RectanglePlacement::centred);
    }

    for (int i = 0; i < kNumBars; ++i)
    {
        const float dB = juce::jlimit (kDbMin, 0.0f, level[(size_t) i]);
        const float y = kY0 + (-dB) / (-kDbMin) * (kY1 - kY0);
        const float h = juce::jmax (0.0f, kY1 - y);
        const float x = kPX + (float) i * kBW + 2.0f;
        g.setColour (zoneColour (barFreq (i)).withAlpha (0.9f));
        g.fillRect (x, y, juce::jmax (1.0f, kBW - 4.0f), h);

        const float heldDb = juce::jlimit (kDbMin, 0.0f, peakHoldDb[(size_t) i]);
        const float heldY = kY0 + (-heldDb) / (-kDbMin) * (kY1 - kY0);
        if (peakHoldDb[(size_t) i] > kDbMin && heldY + 2.0f < kY1)
        {
            g.setColour (white.withAlpha (0.75f));
            g.fillRect (x, heldY, juce::jmax (1.0f, kBW - 4.0f), 2.0f);
        }
    }

    g.setColour (label);
    const struct { float hz; const char* txt; } fl[] = {
        {25.0f,"25"},{50.0f,"50"},{100.0f,"100"},{200.0f,"200"},{500.0f,"500"},
        {1000.0f,"1k"},{2000.0f,"2k"},{5000.0f,"5k"},{10000.0f,"10k"},{20000.0f,"20k"} };
    for (auto& l : fl)
        drawText (g, axisFont, l.txt, freqToX (l.hz), 192.0f, Align::centre);

    g.setColour (bg);
    g.fillRect (0.0f, 0.0f, kW, 40.0f);
    g.setColour (border);
    g.drawLine (0.0f, 40.0f, kW, 40.0f, 1.0f);

    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (titleFont, kTitle, 16.0f, 32.0f);
        for (int i = 0; i < ga.getNumGlyphs(); ++i)
        {
            g.setColour (titleLetterColour (i));
            ga.getGlyph (i).draw (g);
        }
    }

    g.setColour (powered);
    drawText (g, subFont, kSubtitle, 664.0f, 28.0f, Align::right);

    {
        const juce::Rectangle<float> r (270.0f, 208.0f, 140.0f, 20.0f);
        g.setColour (stereoOpen ? mid : btn);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (btnLine);
        g.drawRoundedRectangle (r, 4.0f, 1.5f);
        g.setColour (stereoOpen ? ink : btnText);
        drawText (g, axisFont, "CampoStereo", r.getCentreX(), r.getY() + 14.0f, Align::centre);
    }

    if (hover)
    {
        const float x = juce::jlimit (kPX + kBW * 0.5f, kRight - kBW * 0.5f, hoverX);
        const float hz = juce::jlimit (20.0f, 20000.0f, xToFreq (x));
        const int bar = xToBar (x);
        const float db = (bar == hoverReadoutBar) ? hoverReadoutDb : level[(size_t) bar];

        g.setColour (white.withAlpha (0.25f));
        g.drawLine (x, kY0, x, kY1, 1.0f);
        const float w = 118.0f, h = 18.0f, bx = kRight - w - 2.0f, by = 52.0f;
        g.setColour (bg.withAlpha (0.85f));
        g.fillRoundedRectangle (bx, by, w, h, 4.0f);
        g.setColour (border);
        g.drawRoundedRectangle (bx, by, w, h, 4.0f, 1.0f);
        const juce::String dbTxt = db <= -80.0f ? juce::String ("-inf") : juce::String (db, 1);
        g.setColour (readout);
        drawText (g, axisFont, juce::String (juce::roundToInt (hz)) + " Hz, " + dbTxt + " dB",
                  bx + w * 0.5f, by + 13.0f, Align::centre);
    }
}

void ClarityAudioProcessorEditor::mouseMove (const juce::MouseEvent& e)
{
    const float s = (float) getWidth() / kW;
    hoverX = e.position.x / s;
    hoverY = e.position.y / s;
    hover = hoverX >= kPX && hoverX <= kRight && hoverY >= 46.0f && hoverY <= 196.0f;
}

void ClarityAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e) { mouseMove (e); }
void ClarityAudioProcessorEditor::mouseExit (const juce::MouseEvent&) { hover = false; }
