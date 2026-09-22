# Third-party KiCad libraries

This directory vendors the Seeed Studio XIAO Series symbol and footprint
libraries so the Dyno KiCad project can be opened without machine-specific
library paths.

- **Creator:** Seeed Studio and community contributors
- **Upstream:** [Seeed Studio OPL KiCad Library](https://github.com/Seeed-Studio/OPL_Kicad_Library)
- **Project page:** [Seeed Studio XIAO Series open-source materials](https://wiki.seeedstudio.com/SeeedStudio_XIAO_Series_Introduction/)
- **License:** [Creative Commons Attribution-ShareAlike 4.0 International](LICENSE-CC-BY-SA-4.0.md)

Local modification (2026-09-21): the XIAO-ESP32-C3-SMD footprint graphic
outline was moved from F.SilkS to F.Fab because it crosses module pads and
the carrier board edge. Pad geometry and numbering are unchanged. This
modified footprint retains the upstream CC BY-SA 4.0 license. The Dyno board
uses a separate inset silkscreen outline and pin-1 marking. Other vendored
files are unchanged. Project library tables point to these local copies.

Seeed Studio and XIAO are names and marks of their respective owner. Their use
here identifies the compatible module and does not imply endorsement.
