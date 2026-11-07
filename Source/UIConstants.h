#pragma once
#include <juce_graphics/juce_graphics.h>

// =====================================================================
//  CLARITY - constantes de la interfaz (mismas medidas/colores que el
//  mockup CM3CH PRO, pero SOLO el analizador de espectro)
// =====================================================================
namespace clarity::ui
{
    // Lienzo lógico (la ventana escala proporcionalmente a partir de esto)
    constexpr float kW = 680.0f;
    constexpr float kH = 204.0f;

    // Geometría del analizador
    constexpr float kPX    = 46.0f;    // inicio X de la primera barra
    constexpr float kBW    = 20.0f;    // ancho de cada barra (1/3 de octava)
    constexpr float kRight = 664.0f;   // borde derecho de la rejilla
    constexpr float kY0    = 52.0f;    // Y de 0 dB
    constexpr float kY1    = 178.0f;   // Y de -60 dB
    constexpr int   kNumBars = 31;     // 20 Hz ... 20 kHz
    constexpr float kDbMin  = -40.0f;   // piso de la escala
    constexpr float kDbStep = 5.0f;     // escalones: 0, -5, -10 ... -40
    constexpr float kDisplayOffsetDb = 0.0f;  // sube/baja TODAS las barras (ej. +6 para ver más alto)

    // CampoStereo (botón + panel desplegable)
    constexpr float kHButton = 28.0f;                 // franja del botón bajo el espectro
    constexpr float kPanelH  = 208.0f;                // alto del panel estéreo
    constexpr float kHClosed = kH + kHButton;         // 232
    constexpr float kHOpen   = kHClosed + kPanelH;    // 440
    constexpr int   kStereoBlock = 1024;              // muestras analizadas por frame
    constexpr int   kPolarBins   = 60;                // sectores del polar (3 grados c/u)
    constexpr float kPolarRangeDb = 50.0f;            // rango del polar
    constexpr float kVuRefDbFS    = -18.0f;           // 0 VU = -18 dBFS RMS

    // Zonas de color (YA NO hay líneas ni manijas arrastrables, solo color)
    constexpr float kLowZoneHz  = 123.0f;    // < 123 Hz  -> morado
    constexpr float kHighZoneHz = 4500.0f;   // >= 4.5 kHz -> naranja

    // Lectura al pasar el ratón: true = dB de la barra bajo el cursor, false = dB de la posición vertical del ratón
    constexpr bool kHoverShowsBarLevel = true;

    // Balística de las barras (por frame a 60 fps)
    constexpr float kAttack      = 0.6f;
    constexpr float kReleaseDbPF = 1.6f;

    // Título pegado: CLARITY, cada letra con el color de las barras
    //   C L -> morado | A R I -> turquesa | T Y -> naranja
    inline const char* kTitle = "CLARITY";
    inline const char* kSubtitle   = "Powered By Tiladco";

    // Colores
    inline const juce::Colour bg      { 0xff0b101c };
    inline const juce::Colour border  { 0xff1d2840 };
    inline const juce::Colour grid    { 0xff18233a };
    inline const juce::Colour label   { 0xffa9b6d3 };
    inline const juce::Colour powered { 0xff5a6f96 };
    inline const juce::Colour red     { 0xffe11d2e };
    inline const juce::Colour readout { 0xffdce7ff };
    inline const juce::Colour ink     { 0xff0a0f1c };
    inline const juce::Colour btn     { 0xff35405a };
    inline const juce::Colour btnLine { 0xff252d42 };
    inline const juce::Colour btnText { 0xffeaf1ff };
    inline const juce::Colour dark    { 0xff080c16 };
    inline const juce::Colour faceTop { 0xff141b2e };
    inline const juce::Colour white   { 0xffffffff };
    inline const juce::Colour low     { 0xffe040fb };   // morado
    inline const juce::Colour mid     { 0xff00f0c8 };   // turquesa
    inline const juce::Colour high    { 0xffffa90a };   // naranja

    inline juce::Colour titleLetterColour (int i)
    {
        return i < 2 ? low : (i < 5 ? mid : high);
    }
}
