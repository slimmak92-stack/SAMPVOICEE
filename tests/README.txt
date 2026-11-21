Run from the repository root (Linux / macOS / MinGW):

  g++ -std=c++17 -O2 -o dsp_test tests/voice_dsp_test.cpp && ./dsp_test
  g++ -std=c++17 -I tests/stub -include cstring -include iterator -include cmath -w \
      -o cfg_test tests/config_test.cpp client/PluginConfig.cpp && ./cfg_test

The DSP test checks every effect (finite, bounded, not silent), pitch direction and amount,
echo / reverb tails, low cut, compressor, gate, level meters and speed.
The config test checks save / load of the new fields and loading of files from older builds.
