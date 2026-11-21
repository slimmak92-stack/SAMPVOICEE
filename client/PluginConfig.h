/*
    This is a SampVoice project file
    Developer: CyberMor <cyber.mor.2020@gmail.ru>

    See more here https://github.com/CyberMor/sampvoice

    Copyright (c) Daniel (CyberMor) 2020 All rights reserved
*/

#pragma once

#include <string>

#include <d3d9.h>

class PluginConfig {

    PluginConfig() = delete;
    ~PluginConfig() = delete;
    PluginConfig(const PluginConfig&) = delete;
    PluginConfig(PluginConfig&&) = delete;
    PluginConfig& operator=(const PluginConfig&) = delete;
    PluginConfig& operator=(PluginConfig&&) = delete;

public:

    /*
        All settings related to the screen are stored in the base resolution (640x480) and,
        upon receipt of the value, are converted to the screen resolution by the CRender module
    */

    static constexpr int      kConfigVersion            = 2;
    static constexpr int      kVoiceEffectCount         = 14;   // must match voice_dsp::EffectCount
    static constexpr bool     kDefValSoundEnable        = true;
    static constexpr int      kDefValSoundVolume        = 100;
    static constexpr bool     kDefValSoundBalancer      = false;
    static constexpr bool     kDefValSoundFilter        = false;
    static constexpr float    kDefValSpeakerIconScale   = 1.f;
    static constexpr int      kDefValSpeakerIconOffsetX = 0;
    static constexpr int      kDefValSpeakerIconOffsetY = 0;
    static constexpr bool     kDefValMicroEnable        = true;
    static constexpr int      kDefValMicroVolume        = 75;
    static constexpr int      kDefValVoiceEffect        = 0;
    static constexpr float    kDefValMicroIconScale     = 1.f;
    static constexpr int      kDefValMicroIconPositionX = -100;
    static constexpr int      kDefValMicroIconPositionY = -100;
    static constexpr D3DCOLOR kDefValMicroIconColor     = 0x00ffffff;
    static constexpr float    kDefValMicroIconAngle     = 0.f;
    static constexpr bool     kDefValGateEnable         = true;
    static constexpr int      kDefValGateSensitivity    = 50;
    static constexpr bool     kDefValAutoGain           = false;
    static constexpr bool     kDefValRogerBeep          = false;
    static constexpr bool     kDefValAutoRecover        = true;
    static constexpr int      kDefValIconStyle          = 0;
    static constexpr bool     kDefValIconBars           = true;
    static constexpr bool     kDefValIconFade           = false;
    static constexpr int      kDefValIconFadeDelay      = 5;
    static constexpr D3DCOLOR kDefValIconColorIdle      = 0x00ffffff;
    static constexpr D3DCOLOR kDefValIconColorTalk      = 0x0050dc78;
    static constexpr D3DCOLOR kDefValIconColorMuted     = 0x00ff5a5a;
    static constexpr bool     kDefValLowCut             = false;
    static constexpr bool     kDefValCompressor         = false;
    static constexpr int      kDefValCustomPitch        = 0;

public:

    static bool Load(const std::string& configFilePath);
    static bool Save(const std::string& configFilePath);

    static void Reset() noexcept;
    static bool IsLoaded() noexcept;

    static bool IsPlaybackLoaded() noexcept;
    static bool IsSpeakerLoaded() noexcept;
    static bool IsRecordLoaded() noexcept;
    static bool IsMicroLoaded() noexcept;

    static void SetPlaybackLoaded(bool status) noexcept;
    static void SetSpeakerLoaded(bool status) noexcept;
    static void SetRecordLoaded(bool status) noexcept;
    static void SetMicroLoaded(bool status) noexcept;

    static bool GetSoundEnable() noexcept;
    static int GetSoundVolume() noexcept;
    static bool GetSoundBalancer() noexcept;
    static bool GetSoundFilter() noexcept;
    static float GetSpeakerIconScale() noexcept;
    static int GetSpeakerIconOffsetX() noexcept;
    static int GetSpeakerIconOffsetY() noexcept;
    static bool GetMicroEnable() noexcept;
    static int GetMicroVolume() noexcept;
    static int GetVoiceEffect() noexcept;
    static const std::string& GetDeviceName() noexcept;
    static float GetMicroIconScale() noexcept;
    static int GetMicroIconPositionX() noexcept;
    static int GetMicroIconPositionY() noexcept;
    static D3DCOLOR GetMicroIconColor() noexcept;
    static float GetMicroIconAngle() noexcept;
    static bool GetGateEnable() noexcept;
    static int GetGateSensitivity() noexcept;
    static bool GetAutoGain() noexcept;
    static bool GetRogerBeep() noexcept;
    static bool GetAutoRecover() noexcept;
    static int GetIconStyle() noexcept;
    static bool GetIconBars() noexcept;
    static bool GetIconFade() noexcept;
    static int GetIconFadeDelay() noexcept;
    static D3DCOLOR GetIconColorIdle() noexcept;
    static D3DCOLOR GetIconColorTalk() noexcept;
    static D3DCOLOR GetIconColorMuted() noexcept;
    static bool GetLowCut() noexcept;
    static bool GetCompressor() noexcept;
    static int GetCustomPitch() noexcept;

    static void SetSoundEnable(bool soundEnable) noexcept;
    static void SetSoundVolume(int soundVolume) noexcept;
    static void SetSoundBalancer(bool soundBalancer) noexcept;
    static void SetSoundFilter(bool soundFilter) noexcept;
    static void SetSpeakerIconScale(float speakerIconScale) noexcept;
    static void SetSpeakerIconOffsetX(int speakerIconOffsetX) noexcept;
    static void SetSpeakerIconOffsetY(int speakerIconOffsetY) noexcept;
    static void SetMicroEnable(bool microEnable) noexcept;
    static void SetMicroVolume(int microVolume) noexcept;
    static void SetVoiceEffect(int voiceEffect) noexcept;
    static void SetDeviceName(std::string deviceName) noexcept;
    static void SetMicroIconScale(float microIconScale) noexcept;
    static void SetMicroIconPositionX(int microIconPositionX) noexcept;
    static void SetMicroIconPositionY(int microIconPositionY) noexcept;
    static void SetMicroIconColor(D3DCOLOR microIconColor) noexcept;
    static void SetMicroIconAngle(float microIconAngle) noexcept;
    static void SetGateEnable(bool v) noexcept;
    static void SetGateSensitivity(int v) noexcept;
    static void SetAutoGain(bool v) noexcept;
    static void SetRogerBeep(bool v) noexcept;
    static void SetAutoRecover(bool v) noexcept;
    static void SetIconStyle(int v) noexcept;
    static void SetIconBars(bool v) noexcept;
    static void SetIconFade(bool v) noexcept;
    static void SetIconFadeDelay(int v) noexcept;
    static void SetIconColorIdle(D3DCOLOR v) noexcept;
    static void SetIconColorTalk(D3DCOLOR v) noexcept;
    static void SetIconColorMuted(D3DCOLOR v) noexcept;
    static void SetLowCut(bool v) noexcept;
    static void SetCompressor(bool v) noexcept;
    static void SetCustomPitch(int v) noexcept;

private:

    static bool loadStatus;

    static bool playbackLoaded;
    static bool speakerLoaded;
    static bool recordLoaded;
    static bool microLoaded;

    static bool soundEnable;
    static int soundVolume;
    static bool soundBalancer;
    static bool soundFilter;

    static float speakerIconScale;
    static int speakerIconOffsetX;
    static int speakerIconOffsetY;

    static bool microEnable;
    static int microVolume;
    static int voiceEffect;
    static std::string deviceName;

    static float microIconScale;
    static int microIconPositionX;
    static int microIconPositionY;
    static D3DCOLOR microIconColor;
    static float microIconAngle;

    static bool gateEnable;
    static int gateSensitivity;
    static bool autoGain;
    static bool rogerBeep;
    static bool autoRecover;
    static int iconStyle;
    static bool iconBars;
    static bool iconFade;
    static int iconFadeDelay;
    static D3DCOLOR iconColorIdle;
    static D3DCOLOR iconColorTalk;
    static D3DCOLOR iconColorMuted;
    static bool lowCut;
    static bool compressor;
    static int customPitch;

};
