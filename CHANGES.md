# SampVoice 3.1 (patched) - Voice Lab update

Client only. Protocol and server plugin are unchanged (kVersion 11), nothing to update on the server.

- 9 new voice effects (14 in total): Giant, Chipmunk, Megaphone, Telephone, Echo, Cave (reverb), Alien,
  Underwater, and Custom pitch (free slider, -12 .. +12 semitones). The first five effects keep their
  index, so saved settings stay valid.
- Low cut filter (90 Hz): removes rumble, desk bumps and plosive thumps before the noise gate.
- Compressor (3:1, make-up gain): evens out loud and quiet speech, works together with auto gain.
- Live meters in the Microphone tab: input level and sent level (move while you talk or with "Check device").
- One-click presets: Clean voice, Noisy room, Streamer, Radio operator.
- Settings are saved in a second optional block at the end of svconfig.bin; files from earlier builds still load.
- tests/: unit tests for the DSP and the config file (g++ -std=c++17), see tests/README.txt.
- Processing order: low cut -> noise gate -> auto gain -> compressor -> effect -> limiter.

Note: the Windows client itself (Record / menu wiring) was not compiled by the author of this patch.
Build with the included GitHub workflow or Visual Studio.

# SampVoice 3.1 (patched)

Same protocol as stock 3.1 (kVersion 11) - works with the 3.1 server plugin in /server.
Not compiled by the author of the patch - build with the included GitHub workflow or Visual Studio.

- New mic icons (client/resources/micro_*.png, embedded at build time).
- Voice effect dropdown in the Microphone tab: Normal / Deep / High / Robot / Radio
  (client/VoiceDsp.h; also gentle noise gate + soft limiter). Saved in svconfig.bin
  (old config files still load).
- client.vcxproj: also searches the Windows 10 SDK (needed on GitHub runners).
- .github/workflows/build.yml: builds client -> bin/sampvoice.asi


## Mic overhaul (patched build)
- Mic icon is drawn in code (no PNGs): styles Filled / Outline / Minimal, idle / talking / muted colors,
  optional equalizer bars, optional "fade when idle" (all in Microphone tab -> Icon style).
- Noise gate with adaptive noise floor + hysteresis, adjustable threshold (Voice processing).
- Auto gain (slow AGC, max +12 dB) keeps the voice level even.
- Roger beep (two short tones) appended after the last frame when you release the key.
- Microphone auto-recover: unplug / replug / device switch is detected every 1.5 s and the capture is
  re-opened (the device you picked is remembered); retries Record::Init if no mic was present at connect.
- Exports for Lua scripts: SvGetMicState() and SvGetChannelName(char*, int).
- Settings are stored in an optional extension block at the end of svconfig.bin (old files still load).
