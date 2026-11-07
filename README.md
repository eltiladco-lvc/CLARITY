# CLARITY — Spectrum Analyzer VST3 (Windows 10/11, 64-bit)
Powered By Tiladco · analizador de 31 bandas (1/3 de octava, 20 Hz–20 kHz), escala de 0 a -40 dB en pasos de 5 dB. El audio pasa sin alterarse.

## Compilar en GitHub (sin instalar nada)
1. Sube todo este contenido a un repositorio (rama `main`).
2. Pestaña **Actions → Build CLARITY VST3 (Windows) → Run workflow** (o simplemente haz push).
3. Al terminar, descarga **CLARITY-VST3-win64** desde *Artifacts*.
4. Descomprime y copia la carpeta `CLARITY.vst3` a `C:\Program Files\Common Files\VST3\`.
5. Reescanea plugins en tu DAW.

Release automático: `git tag v1.0.0 && git push --tags`.

## Compilar en local
Requisitos: Visual Studio 2022 (Desktop C++), CMake ≥ 3.22, Git.
    cmake -B build -A x64
    cmake --build build --config Release
Resultado: `build/CLARITY_artefacts/Release/VST3/CLARITY.vst3`

## CampoStereo
Botón bajo el espectro que despliega el panel estéreo con 3 diseños (pestañas POLAR / SCOPE / VU): polar de campo estéreo, goniómetro con correlación, y 4 vúmetros L/R/M/S. 0 VU = -18 dBFS (`kVuRefDbFS`).

## Personalizar
Todo en `Source/UIConstants.h` (colores, zonas de color, textos, velocidad de caída).
