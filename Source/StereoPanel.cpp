#include "StereoPanel.h"
#include <cmath>

using namespace clarity::ui;

namespace
{
    constexpr float kPi = juce::MathConstants<float>::pi;

    void drawTxt (juce::Graphics& g, const juce::Font& f, const juce::String& t, float x, float baseline, int align)
    {
        juce::GlyphArrangement m;
        m.addLineOfText (f, t, 0.0f, 0.0f);
        const float w = m.getBoundingBox (0, -1, true).getWidth();
        if (align == 1) x -= w * 0.5f; else if (align == 2) x -= w;
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, t, x, baseline);
        ga.draw (g);
    }

    float vuAngleDb (float dbTp)
    {
        const float p = juce::jlimit (0.0f, 1.0f, (dbTp + 20.0f) / 23.0f);
        return juce::degreesToRadians (-44.0f + 88.0f * p);
    }

    float tickAngle (float db) { return vuAngleDb (db); }

    juce::Point<float> pol (float px, float py, float a, float r)
    {
        return { px + r * std::sin (a), py - r * std::cos (a) };
    }

    juce::Rectangle<float> tabRect (int i) { return { 227.0f + (float) i * 78.0f, 8.0f, 70.0f, 18.0f }; }

    float amplitudeToDbTp (float amplitude)
    {
        return amplitude > 1.0e-9f ? 20.0f * std::log10 (amplitude) : -180.0f;
    }

    template <typename SampleAt>
    float truePeak (int n, SampleAt&& sampleAt)
    {
        if (n <= 0) return 0.0f;
        const auto at = [&] (int index) { return sampleAt (juce::jlimit (0, n - 1, index)); };
        float peak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            peak = juce::jmax (peak, std::abs (at (i)));
            const float p0 = at (i - 1), p1 = at (i), p2 = at (i + 1), p3 = at (i + 2);
            for (int phase = 1; phase < 4; ++phase)
            {
                const float t = (float) phase * 0.25f;
                const float y = p0 * (-t * (t - 1.0f) * (t - 2.0f) / 6.0f)
                              + p1 * ((t + 1.0f) * (t - 1.0f) * (t - 2.0f) * 0.5f)
                              + p2 * (-(t + 1.0f) * t * (t - 2.0f) * 0.5f)
                              + p3 * ((t + 1.0f) * t * (t - 1.0f) / 6.0f);
                peak = juce::jmax (peak, std::abs (y));
            }
        }
        return peak;
    }
}

StereoPanel::StereoPanel()
{
    const auto base = juce::Font (juce::FontOptions ("Segoe UI", 11.0f, juce::Font::plain));
    smallFont = base.withPointHeight (9.0f);
    axisFont = base.withPointHeight (11.0f);
    bigFont = base.withPointHeight (16.0f).boldened();
    pts.reserve (kStereoBlock);
    prevPts.reserve (kStereoBlock);
}

void StereoPanel::process (const float* l, const float* r, int n)
{
    if (n <= 0 || l == nullptr || r == nullptr) return;

    double sl = 0, sr = 0, lr = 0;
    float pk = 0.0f;
    std::array<float, kPolarBins> acc {};
    prevPts.swap (pts);
    pts.clear();

    for (int i = 0; i < n; ++i)
    {
        const float a = l[i], b = r[i];
        const float m = 0.5f * (a + b), s = 0.5f * (a - b);
        sl += (double) a * a; sr += (double) b * b; lr += (double) a * b;
        pk = juce::jmax (pk, std::abs (a), std::abs (b));
        const int bin = juce::jmin (kPolarBins - 1,
                                    (int) std::floor (std::atan2 (std::abs (b), std::abs (a)) * 2.0f / kPi * (float) kPolarBins));
        acc[(size_t) bin] += 0.5f * (a * a + b * b);
        const float x = -s, y = m, mag = std::hypot (x, y);
        const float f = mag > 1.0e-6f ? std::pow (mag, 0.6f) / mag : 0.0f;
        pts.push_back ({ x * f, y * f });
    }

    const float tp[4] = {
        truePeak (n, [l] (int i) { return l[i]; }),
        truePeak (n, [r] (int i) { return r[i]; }),
        truePeak (n, [l, r] (int i) { return 0.5f * (l[i] + r[i]); }),
        truePeak (n, [l, r] (int i) { return 0.5f * (l[i] - r[i]); })
    };

    for (int k = 0; k < 4; ++k)
    {
        const float db = amplitudeToDbTp (tp[(size_t) k]);
        float& current = vuTpDb[(size_t) k];
        current = db > current ? current + (db - current) * 0.6f : juce::jmax (db, current - 1.6f);
    }

    const float outputDb = amplitudeToDbTp (juce::jmax (tp[0], tp[1]));
    outputPeakDbTp = outputDb > outputPeakDbTp ? outputDb : juce::jmax (outputDb, outputPeakDbTp - 0.25f);
    const float c = (sl + sr < 1.0e-10) ? 0.0f : (float) (lr / (std::sqrt (sl * sr) + 1.0e-12));
    corr += (c - corr) * 0.15f;
    peakHold = juce::jmax (pk, peakHold * 0.95f);

    for (int b = 0; b < kPolarBins; ++b)
    {
        const float p0 = acc[(size_t) juce::jmax (0, b - 1)];
        const float p1 = acc[(size_t) b];
        const float p2 = acc[(size_t) juce::jmin (kPolarBins - 1, b + 1)];
        const float p = (p0 + 2.0f * p1 + p2) * 0.25f / (float) n;
        const float db = 10.0f * std::log10 (p + 1.0e-12f);
        const float v = juce::jlimit (0.0f, 1.0f, (db + kPolarRangeDb) / kPolarRangeDb);
        float& cur = polar[(size_t) b];
        cur += (v - cur) * (v > cur ? 0.5f : 0.12f);
    }
    repaint();
}

void StereoPanel::mouseDown (const juce::MouseEvent& e)
{
    const float s = (float) getWidth() / kW;
    const auto p = e.position / s;
    for (int i = 0; i < 3; ++i)
        if (tabRect (i).contains (p))
        {
            setDesign (i);
            if (onDesignChanged) onDesignChanged (i);
            return;
        }
}

void StereoPanel::paint (juce::Graphics& g)
{
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / kW));
    g.setColour (bg);
    g.fillRoundedRectangle (10.0f, 32.0f, 660.0f, 168.0f, 6.0f);
    g.setColour (border);
    g.drawRoundedRectangle (10.0f, 32.0f, 660.0f, 168.0f, 6.0f, 2.0f);

    const char* names[3] = { "POLAR", "SCOPE", "VU" };
    for (int i = 0; i < 3; ++i)
    {
        const auto r = tabRect (i);
        const bool on = (i == design);
        g.setColour (on ? mid : btn);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (btnLine);
        g.drawRoundedRectangle (r, 4.0f, 1.5f);
        g.setColour (on ? ink : btnText);
        drawTxt (g, axisFont, names[i], r.getCentreX(), r.getY() + 13.0f, 1);
    }

    if (design == 0) drawPolar (g);
    else if (design == 1) drawScope (g);
    else drawVU (g);
}

void StereoPanel::drawPolar (juce::Graphics& g)
{
    const float cx = 340.0f, by = 192.0f, R = 140.0f;
    const auto signalColour = monochrome ? white : high;

    juce::Path disc;
    disc.startNewSubPath (cx - R, by);
    disc.addCentredArc (cx, by, R, R, 0.0f, -kPi * 0.5f, kPi * 0.5f);
    disc.closeSubPath();
    g.setColour (dark);
    g.fillPath (disc);
    g.setColour (border);
    g.strokePath (disc, juce::PathStrokeType (2.0f));

    juce::Path ring;
    ring.addCentredArc (cx, by, R * 0.5f, R * 0.5f, 0.0f, -kPi * 0.5f, kPi * 0.5f, true);
    g.setColour (grid);
    g.strokePath (ring, juce::PathStrokeType (1.0f));

    const float dash[] = { 3.0f, 4.0f };
    g.setColour (juce::Colour (0xff2a3552));
    for (float deg : { 45.0f, 90.0f, 135.0f })
    {
        const float a = juce::degreesToRadians (deg);
        g.drawDashedLine ({ cx, by, cx - std::cos (a) * R, by - std::sin (a) * R }, dash, 2, 1.0f);
    }

    juce::Path p;
    p.startNewSubPath (cx, by);
    for (int k = 0; k < kPolarBins; ++k)
    {
        const float a = ((float) k + 0.5f) / (float) kPolarBins * kPi;
        const float v = polar[(size_t) k];
        p.lineTo (cx - std::cos (a) * v * R, by - std::sin (a) * v * R);
    }
    p.closeSubPath();
    g.setColour (signalColour.withAlpha (0.22f));
    g.fillPath (p);
    g.setColour (signalColour);
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));

    g.setColour (label);
    drawTxt (g, axisFont, "L", cx - R - 14.0f, by - 2.0f, 1);
    drawTxt (g, axisFont, "R", cx + R + 14.0f, by - 2.0f, 1);
}

void StereoPanel::drawScope (juce::Graphics& g)
{
    const float cx = 340.0f, cy = 93.0f, r = 51.0f;
    const juce::Rectangle<float> box (250.0f, 34.0f, 180.0f, 128.0f);
    const auto traceColour = monochrome ? white : mid;
    const auto persistenceColour = monochrome ? white : low;

    g.setColour (dark);
    g.fillRoundedRectangle (box, 6.0f);
    g.setColour (border);
    g.drawRoundedRectangle (box, 6.0f, 2.0f);

    const float dash[] = { 3.0f, 4.0f };
    g.setColour (persistenceColour.withAlpha (0.28f));
    for (int k = 0; k < 16; ++k)
    {
        const float a = (float) k * kPi / 8.0f;
        g.drawDashedLine ({ cx + std::cos (a) * 12.0f, cy + std::sin (a) * 12.0f,
                            cx + std::cos (a) * r, cy + std::sin (a) * r }, dash, 2, 1.0f);
    }

    auto trace = [&] (const std::vector<juce::Point<float>>& v)
    {
        juce::Path p;
        for (size_t i = 0; i < v.size(); ++i)
        {
            const float x = cx + v[i].x * r;
            const float y = cy - v[i].y * r;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        return p;
    };

    g.setColour (persistenceColour.withAlpha (0.24f));
    g.strokePath (trace (prevPts), juce::PathStrokeType (1.0f));
    g.setColour (traceColour.withAlpha (0.9f));
    g.strokePath (trace (pts), juce::PathStrokeType (1.0f));

    const float meterX = 88.0f, meterY = 178.0f, meterW = 486.0f, meterH = 10.0f;
    constexpr int segments = 36;
    const float gap = 3.0f;
    const float segmentW = (meterW - gap * (segments - 1)) / (float) segments;
    const float fill = juce::jlimit (0.0f, 1.0f, (outputPeakDbTp + 36.0f) / 36.0f);
    const juce::Colour palette[4] = { low, blue, mid, high };

    g.setColour (label);
    drawTxt (g, smallFont, "OUTPUT", 20.0f, meterY + 9.0f, 0);
    for (int i = 0; i < segments; ++i)
    {
        const float x = meterX + (float) i * (segmentW + gap);
        const float fraction = juce::jlimit (0.0f, 1.0f, fill * (float) segments - (float) i);
        const juce::Colour colour = monochrome ? white : palette[juce::jmin (3, i / (segments / 4))];
        g.setColour (faceTop);
        g.fillRect (x, meterY, segmentW, meterH);
        if (fraction > 0.0f)
        {
            g.setColour (colour);
            g.fillRect (x, meterY, segmentW * fraction, meterH);
        }
    }

    const juce::String db = outputPeakDbTp < -60.0f ? juce::String ("-INF") : juce::String (outputPeakDbTp, 1);
    g.setColour (readout);
    drawTxt (g, smallFont, db + " dBTP", 650.0f, meterY + 9.0f, 2);
}

void StereoPanel::drawVU (juce::Graphics& g)
{
    const char* names[4] = { "L", "R", "M", "S" };
    const juce::Colour originalCols[4] = { low, blue, mid, high };
    const float Rs = 92.0f;
    const auto overloadColour = monochrome ? white : high;

    for (int i = 0; i < 4; ++i)
    {
        const auto needleColour = monochrome ? white : originalCols[i];
        const juce::Rectangle<float> cell (15.0f + (float) i * 164.0f, 36.0f, 158.0f, 160.0f);
        juce::Graphics::ScopedSaveState sss (g);
        g.reduceClipRegion (cell.toNearestInt());

        g.setGradientFill (juce::ColourGradient (faceTop, cell.getX(), cell.getY(), bg, cell.getX(), cell.getBottom(), false));
        g.fillRoundedRectangle (cell, 6.0f);
        g.setColour (border);
        g.drawRoundedRectangle (cell, 6.0f, 2.0f);
        const float px = cell.getCentreX(), py = cell.getBottom() + 9.0f;

        juce::Path a1, a2;
        a1.addCentredArc (px, py, Rs, Rs, 0.0f, tickAngle (-20.0f), tickAngle (0.0f), true);
        a2.addCentredArc (px, py, Rs, Rs, 0.0f, tickAngle (0.0f), tickAngle (3.0f), true);
        g.setColour (label.withAlpha (0.8f));
        g.strokePath (a1, juce::PathStrokeType (1.2f));
        g.setColour (overloadColour);
        g.strokePath (a2, juce::PathStrokeType (3.0f));

        for (float v : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
        {
            const float a = tickAngle (v);
            const auto t0 = pol (px, py, a, Rs - 5.0f), t1 = pol (px, py, a, Rs);
            const auto lp = pol (px, py, a, Rs + 10.0f);
            g.setColour (v > 0.0f ? overloadColour : label);
            g.drawLine (t0.x, t0.y, t1.x, t1.y, 1.2f);
            drawTxt (g, smallFont, juce::String ((int) v), lp.x, lp.y + 3.0f, 1);
        }

        const float a = vuAngleDb (vuTpDb[(size_t) i]);
        const auto tip = pol (px, py, a, Rs + 8.0f);
        g.setColour (needleColour.withAlpha (0.25f));
        g.drawLine (px, py, tip.x, tip.y, 4.0f);
        g.setColour (needleColour);
        g.drawLine (px, py, tip.x, tip.y, 1.5f);
        g.setColour (needleColour);
        drawTxt (g, bigFont, names[i], px, cell.getBottom() - 18.0f, 1);
        g.setColour (readout);
        drawTxt (g, smallFont, juce::String (vuTpDb[(size_t) i], 1) + " dBTP", px, cell.getBottom() - 5.0f, 1);
    }
}
