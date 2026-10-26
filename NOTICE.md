# Notice

This is an independent study of the visual behaviour of a user interface, written from
measurements of rendered output. It contains no Apple source code, no Apple binaries and
no Apple image assets.

* **Apple, iOS, SwiftUI, SF Pro and SF Symbols** are trademarks of Apple Inc. This project
  is not affiliated with or endorsed by Apple.
* **Fonts**: the upstream working copy renders with SF Pro for fidelity. That font is
  licensed by Apple and is *not* present in this repository. `tools/make_font_data.py`
  generates an equivalent embedded header from **Inter** (SIL Open Font License 1.1),
  fetched at build time, so that the code compiles and renders without redistributing
  Apple assets.
* **Icons**: the symbols in `SfIcons.h` are hand-transcribed outlines used for study, not
  extracted SF Symbols assets.
* **Dear ImGui** is vendored under its MIT licence.
* The reference screenshots used to fit the material are captures of the operating system
  made by the author for interoperability research and are not distributed here.
