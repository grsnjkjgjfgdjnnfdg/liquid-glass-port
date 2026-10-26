# liquid-glass-port

A C++ / Direct3D 11 port of the controls and the material from Apple's iOS 26 and 27
SwiftUI runtime - sliders, switches, segmented controls, sheets, navigation transitions
and the Liquid Glass material itself - not an impression of them.

The work is driven by measurement rather than by eye. A companion iOS app renders a set
of fixed screens and captures them at 3x (1206x2622 for a 402x874 pt layout); the port
renders the same screens at the same scale and the two are compared pixel by pixel, with
every material constant - lift coefficients, lens displacement, edge alphas, spring
responses - recovered from those captures.

## How it is verified

`Port/SwiftUiPort.exe --ios --screen glass --shot out.png` renders one screen at 3x.
`Tools/_compare3x.py` resamples both sides to 402x874 with the same filter and reports
the per-region mean error, which is the number every change is judged by. Ten screens are
scored in `Tools/_verify_all.py`.

## Layout

    imgui/            Dear ImGui (MIT), vendored
    opal/             the DX11 backend and the glass renderer the port draws with
    Port/src/         the port: SDF lens shader, spring solver, every screen
    tools/            the font generator and the comparison harness
    fonts/            generated at build time from an open-licence face

## What this repository does not contain

Apple's SF Pro fonts and Apple's screenshots are not redistributed here. The build
generates a font header from Inter (SIL OFL) so the geometry is reproducible; the type
metrics differ slightly from SF. The reference captures stay on the machine that made
them. See NOTICE.md.

## Status

Ten screens are scored, from 1.8 (Settings) to 5.9 (the refraction chart that measures
the lens). The material's edge, its lens displacement profile and its per-appearance
lift are all fitted to measured curves; the remaining gap is mostly glyph rasterisation
and sub-pixel registration.
