# TR-9090 Rhythm Composer (TR-909 inspired) - Standalone + VST3

Repo layout (everything at the ROOT of the repo):
  CMakeLists.txt
  Source/            (PluginProcessor.*, PluginEditor.*, Voices.h)
  installer/TR9090.iss
  .github/workflows/build.yml

Build: GitHub -> Actions -> "TR-9090 Build" -> Run workflow.
Download artifact TR-9090-Windows -> run TR-9090-Setup.exe.
VST3 installs to C:\Program Files\Common Files\VST3\TR-9090.vst3

Features: 11 voices with 909-style controls, 16-step sequencer with accent and shuffle,
16 patterns (7 factory), pattern copy/paste/clear, 6 factory kits + 8 user kit memory slots,
bank export/import, host sync and GM-drum MIDI input.
