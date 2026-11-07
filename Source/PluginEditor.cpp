#include "PluginEditor.h"
#include <BinaryData.h>      // generado por juce_add_binary_data (logo + fuentes)
#include <cmath>

using namespace clarity::ui;

namespace
{
    enum class Align { left, centre, right };

    // Carga una tipografía incrustada (ej. "Hoshiko_ttf"); nullptr si no existe
    juce::Typeface::Ptr loadTypeface (const char* resourceName)
    {
        int size = 0;
        if (const char* data = BinaryData::getNamedResource (resourceName, size))
            return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
        return nullptr;
    }

    // Tamaño en "px CSS" (em), igual que font-size en el mockup
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

    // Dibuja texto por línea base (igual que SVG <text x y>)
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

    // Posición X <-> frecuencia (coincide con el centro de cada barra de 1/3 de octava)
    float freqToX (float hz) { return kPX + kBW * 0.5f + 3.0f * std::log2 (hz / 20.0f) * kBW; }
    float xToFreq (float x)  { return 20.0f * std::pow (2.0f, (x - kPX - kBW * 0.5f) / (3.0f * kBW)); }

    float barFreq (int i) { return 20.0f * std::pow (2.0f, (float) i / 3.0f); }

    juce::Colour zoneColour (float hz)
    {
        if (hz < kLowZoneHz)  return low;
        if (hz < kHighZoneHz) return mid;
        return high;
    }
}

ClarityAudioProcessorEditor::ClarityAudioProcessorEditor (ClarityAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), fftData ((size_t) kFFTSize * 2, 0.0f),
      blkL ((size_t) kStereoBlock, 0.0f), blkR ((size_t) kStereoBlock, 0.0f)
{
    level.fill (-90.0f);

    // Normalización de la ventana (para que un seno a escala completa lea 0 dB)
    std::vector<float> ones ((size_t) kFFTSize, 1.0f);
    window.multiplyWithWindowingTable (ones.data(), (size_t) kFFTSize);
    windowSum = 0.0f;
    for (float v : ones) windowSum += v;

    // Tipografías: Hoshiko (si la pones en Resources/fonts) -> Yuji Boku -> sistema
    auto titleTf = loadTypeface ("Hoshiko_ttf");
    if (titleTf == nullptr) titleTf = loadTypeface ("YujiBoku_ttf");
    titleFont = makeFont (titleTf, "Brush Script MT", 30.0f).withExtraKerningFactor (2.0f / 30.0f);
    subFont   = makeFont (loadTypeface ("GreatVibes_ttf"), "Segoe Script", 22.0f);
    axisFont  = makeFont (nullptr, "Segoe UI", 11.0f);

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
    const float sc = (float) getWidth() / kW;
    repaint (0, 0, getWidth(), juce::roundToInt (kHClosed * sc));       // solo espectro + botón

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

    const double sr    = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 44100.0;
    const double binHz = sr / (double) kFFTSize;
    const int maxBin   = kFFTSize / 2 - 1;
    const float norm   = 2.0f / windowSum;
    const double edge  = std::pow (2.0, 1.0 / 6.0);       // límites de 1/3 de octava

    for (int i = 0; i < kNumBars; ++i)
    {
        const double fc  = (double) barFreq (i);
        const double bLo = (fc / edge) / binHz;
        const double bHi = (fc * edge) / binHz;
        float db = -100.0f;

        if (bLo <= (double) maxBin)
        {
            const int b0 = (int) std::ceil (bLo);
            const int b1 = juce::jmin (maxBin, (int) std::floor (bHi));
            double power = 0.0;

            if (b1 >= b0)
            {
                for (int b = b0; b <= b1; ++b)
                {
                    const double a = (double) fftData[(size_t) b] * (double) norm;
                    power += a * a;
                }
                power /= 1.5;                              // ENBW de la ventana Hann
            }
            else
            {
                const int b = juce::jlimit (0, maxBin, (int) std::lround (0.5 * (bLo + bHi)));
                const double a = (double) fftData[(size_t) b] * (double) norm;
                power = a * a;
            }
            db = (float) (10.0 * std::log10 (power + 1.0e-12)) + kDisplayOffsetDb;
        }

        float& d = level[(size_t) i];
        d = (db > d) ? d + (db - d) * kAttack
                     : juce::jmax (db, d - kReleaseDbPF);
    }
}

void ClarityAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (bg);
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / kW));

    // --- Panel del analizador ---------------------------------------
    g.setColour (bg);
    g.fillRoundedRectangle (10.0f, 46.0f, 660.0f, 150.0f, 6.0f);
    g.setColour (border);
    g.drawRoundedRectangle (10.0f, 46.0f, 660.0f, 150.0f, 6.0f, 2.0f);

    // Rejilla horizontal + escala en dB
    for (int d = 0; d >= (int) kDbMin; d -= (int) kDbStep)
    {
        const float y = kY0 + (float) (-d) / (-kDbMin) * (kY1 - kY0);
        g.setColour (grid);
        g.drawLine (kPX, y, kRight, y, 1.0f);
        g.setColour (label);
        drawText (g, axisFont, juce::String (d), 40.0f, y + 4.0f, Align::right);
    }

    // Fondo HomeSounds (semitransparente, centrado detrás de las barras)
    if (logo.isValid())
    {
        g.setOpacity (0.45f);
        g.drawImage (logo,
                     juce::Rectangle<float> (kPX + (kRight - kPX - 356.0f) * 0.5f, kY0 - 4.0f, 356.0f, 126.0f),
                     juce::RectanglePlacement::centred);
    }

    // --- Barras (31 bandas de 1/3 de octava, SIN líneas divisorias) ----
    for (int i = 0; i < kNumBars; ++i)
    {
        const float dB = juce::jlimit (kDbMin, 0.0f, level[(size_t) i]);
        const float y  = kY0 + (-dB) / (-kDbMin) * (kY1 - kY0);
        const float h  = juce::jmax (0.0f, kY1 - y);
        const float x  = kPX + (float) i * kBW + 2.0f;

        g.setColour (zoneColour (barFreq (i)).withAlpha (0.9f));
        g.fillRect (x, y, kBW - 4.0f, h);

        if (h > 3.0f)
        {
            g.setColour (white.withAlpha (0.7f));
            g.fillRect (x, y, kBW - 4.0f, 2.0f);
        }
    }

    // Etiquetas de frecuencia
    g.setColour (label);
    const struct { float hz; const char* txt; } fl[] = {
        {25.0f,"25"},{50.0f,"50"},{100.0f,"100"},{200.0f,"200"},{500.0f,"500"},
        {1000.0f,"1k"},{2000.0f,"2k"},{5000.0f,"5k"},{10000.0f,"10k"},{20000.0f,"20k"} };
    for (auto& l : fl)
        drawText (g, axisFont, l.txt, freqToX (l.hz), 192.0f, Align::centre);

    // --- Cabecera: título + Powered By -------------------------------
    g.setColour (bg);
    g.fillRect (0.0f, 0.0f, kW, 40.0f);
    g.setColour (border);
    g.drawLine (0.0f, 40.0f, kW, 40.0f, 1.0f);

    // Título pegado, cada letra con el color de las barras
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

    // --- Botón CampoStereo ---------------------------------------------
    {
        const juce::Rectangle<float> r (270.0f, 208.0f, 140.0f, 20.0f);
        g.setColour (stereoOpen ? mid : btn);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (btnLine);
        g.drawRoundedRectangle (r, 4.0f, 1.5f);
        g.setColour (stereoOpen ? ink : btnText);
        drawText (g, axisFont, "CampoStereo", r.getCentreX(), r.getY() + 14.0f, Align::centre);
    }

    // --- Lectura al pasar el ratón (esquina superior derecha del analizador) ---
    if (hover)
    {
        const float x   = juce::jlimit (kPX, kRight, hoverX);
        const float hz  = juce::jlimit (20.0f, 20000.0f, xToFreq (x));
        const int   bar = juce::jlimit (0, kNumBars - 1, (int) std::floor ((x - kPX) / kBW));
        const float db  = kHoverShowsBarLevel
                            ? level[(size_t) bar]
                            : (juce::jlimit (kY0, kY1, hoverY) - kY0) / (kY1 - kY0) * kDbMin;

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
    hover  = hoverX >= kPX && hoverX <= kRight && hoverY >= 46.0f && hoverY <= 196.0f;
}

void ClarityAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e) { mouseMove (e); }
void ClarityAudioProcessorEditor::mouseExit (const juce::MouseEvent&)   { hover = false; }
