/*
    This is a SampVoice project file
    Developer: CyberMor <cyber.mor.2020@gmail.ru>

    See more here https://github.com/CyberMor/sampvoice

    Copyright (c) Daniel (CyberMor) 2020 All rights reserved
*/

#include "PluginConfig.h"

#include <fstream>

#include <util/Render.h>

namespace { constexpr int kExtMagic = 0x31585445; constexpr int kExt2Magic = 0x32585445; }

bool PluginConfig::Load(const std::string& configFilePath)
{
    constexpr auto kFileMode = std::ios::in | std::ios::binary;

    std::ifstream configFile { configFilePath, kFileMode };

    if (!configFile || !configFile.is_open())
        return false;

    PluginConfig::Reset();

    if (int configVersion { -1 };
        !configFile.read(reinterpret_cast<char*>(&configVersion), sizeof(configVersion)) ||
        configVersion != kConfigVersion) return false;

    const bool loaded =

        configFile.read(reinterpret_cast<char*>(&playbackLoaded),     sizeof(playbackLoaded))     &&
        configFile.read(reinterpret_cast<char*>(&speakerLoaded),      sizeof(speakerLoaded))      &&
        configFile.read(reinterpret_cast<char*>(&recordLoaded),       sizeof(recordLoaded))       &&
        configFile.read(reinterpret_cast<char*>(&microLoaded),        sizeof(microLoaded))        &&

        configFile.read(reinterpret_cast<char*>(&soundEnable),        sizeof(soundEnable))        &&
        configFile.read(reinterpret_cast<char*>(&soundVolume),        sizeof(soundVolume))        &&
        configFile.read(reinterpret_cast<char*>(&soundBalancer),      sizeof(soundBalancer))      &&
        configFile.read(reinterpret_cast<char*>(&soundFilter),        sizeof(soundFilter))        &&

        configFile.read(reinterpret_cast<char*>(&speakerIconScale),   sizeof(speakerIconScale))   &&
        configFile.read(reinterpret_cast<char*>(&speakerIconOffsetX), sizeof(speakerIconOffsetX)) &&
        configFile.read(reinterpret_cast<char*>(&speakerIconOffsetY), sizeof(speakerIconOffsetY)) &&

        configFile.read(reinterpret_cast<char*>(&microEnable),        sizeof(microEnable))        &&
        configFile.read(reinterpret_cast<char*>(&microVolume),        sizeof(microVolume))        &&
        std::getline(configFile, deviceName, '\0')                                                &&

        configFile.read(reinterpret_cast<char*>(&microIconScale),     sizeof(microIconScale))     &&
        configFile.read(reinterpret_cast<char*>(&microIconPositionX), sizeof(microIconPositionX)) &&
        configFile.read(reinterpret_cast<char*>(&microIconPositionY), sizeof(microIconPositionY)) &&
        configFile.read(reinterpret_cast<char*>(&microIconColor),     sizeof(microIconColor))     &&
        configFile.read(reinterpret_cast<char*>(&microIconAngle),     sizeof(microIconAngle))     ;

    // Voice effect was appended later: configs saved by older builds simply don't have it
    if (loaded)
    {
        if (int effect { 0 }; configFile.read(reinterpret_cast<char*>(&effect), sizeof(effect)) &&
            effect >= 0 && effect < kVoiceEffectCount) PluginConfig::voiceEffect = effect;
    }

    // Extended block (noise gate, auto gain, roger beep, icon style...), also optional
    if (loaded)
    {
        int magic { 0 };
        int gs { 0 }, st { 0 }, fd { 0 };
        bool ge { true }, ag { false }, rb { false }, ar { true }, ib { true }, fe { false };
        D3DCOLOR ci { 0 }, ct { 0 }, cm { 0 };

        if (configFile.read(reinterpret_cast<char*>(&magic), sizeof(magic)) && magic == kExtMagic &&
            configFile.read(reinterpret_cast<char*>(&ge), sizeof(ge)) &&
            configFile.read(reinterpret_cast<char*>(&gs), sizeof(gs)) &&
            configFile.read(reinterpret_cast<char*>(&ag), sizeof(ag)) &&
            configFile.read(reinterpret_cast<char*>(&rb), sizeof(rb)) &&
            configFile.read(reinterpret_cast<char*>(&ar), sizeof(ar)) &&
            configFile.read(reinterpret_cast<char*>(&st), sizeof(st)) &&
            configFile.read(reinterpret_cast<char*>(&ib), sizeof(ib)) &&
            configFile.read(reinterpret_cast<char*>(&fe), sizeof(fe)) &&
            configFile.read(reinterpret_cast<char*>(&fd), sizeof(fd)) &&
            configFile.read(reinterpret_cast<char*>(&ci), sizeof(ci)) &&
            configFile.read(reinterpret_cast<char*>(&ct), sizeof(ct)) &&
            configFile.read(reinterpret_cast<char*>(&cm), sizeof(cm)))
        {
            PluginConfig::SetGateEnable(ge);
            PluginConfig::SetGateSensitivity(gs);
            PluginConfig::SetAutoGain(ag);
            PluginConfig::SetRogerBeep(rb);
            PluginConfig::SetAutoRecover(ar);
            PluginConfig::SetIconStyle(st);
            PluginConfig::SetIconBars(ib);
            PluginConfig::SetIconFade(fe);
            PluginConfig::SetIconFadeDelay(fd);
            PluginConfig::SetIconColorIdle(ci);
            PluginConfig::SetIconColorTalk(ct);
            PluginConfig::SetIconColorMuted(cm);
        }
    }

    // Second extension block (voice lab: low cut, compressor, custom pitch), optional as well
    if (loaded)
    {
        int magic2 { 0 }, pitch { 0 };
        bool lc { false }, cp { false };

        if (configFile.read(reinterpret_cast<char*>(&magic2), sizeof(magic2)) && magic2 == kExt2Magic &&
            configFile.read(reinterpret_cast<char*>(&lc), sizeof(lc)) &&
            configFile.read(reinterpret_cast<char*>(&cp), sizeof(cp)) &&
            configFile.read(reinterpret_cast<char*>(&pitch), sizeof(pitch)))
        {
            PluginConfig::SetLowCut(lc);
            PluginConfig::SetCompressor(cp);
            PluginConfig::SetCustomPitch(pitch);
        }
    }

    return PluginConfig::loadStatus = loaded;
}

bool PluginConfig::Save(const std::string& configFilePath)
{
    constexpr auto kFileMode = std::ios::out | std::ios::binary | std::ios::trunc;

    std::ofstream configFile { configFilePath, kFileMode };

    if (!configFile || !configFile.is_open())
        return false;

    const auto configVersion { kConfigVersion };

    return configFile.write(reinterpret_cast<const char*>(&configVersion),      sizeof(configVersion))      &&

           configFile.write(reinterpret_cast<const char*>(&playbackLoaded),     sizeof(playbackLoaded))     &&
           configFile.write(reinterpret_cast<const char*>(&speakerLoaded),      sizeof(speakerLoaded))      &&
           configFile.write(reinterpret_cast<const char*>(&recordLoaded),       sizeof(recordLoaded))       &&
           configFile.write(reinterpret_cast<const char*>(&microLoaded),        sizeof(microLoaded))        &&

           configFile.write(reinterpret_cast<const char*>(&soundEnable),        sizeof(soundEnable))        &&
           configFile.write(reinterpret_cast<const char*>(&soundVolume),        sizeof(soundVolume))        &&
           configFile.write(reinterpret_cast<const char*>(&soundBalancer),      sizeof(soundBalancer))      &&
           configFile.write(reinterpret_cast<const char*>(&soundFilter),        sizeof(soundFilter))        &&

           configFile.write(reinterpret_cast<const char*>(&speakerIconScale),   sizeof(speakerIconScale))   &&
           configFile.write(reinterpret_cast<const char*>(&speakerIconOffsetX), sizeof(speakerIconOffsetX)) &&
           configFile.write(reinterpret_cast<const char*>(&speakerIconOffsetY), sizeof(speakerIconOffsetY)) &&

           configFile.write(reinterpret_cast<const char*>(&microEnable),        sizeof(microEnable))        &&
           configFile.write(reinterpret_cast<const char*>(&microVolume),        sizeof(microVolume))        &&
           configFile.write(deviceName.data(),                                  deviceName.size() + 1)      &&

           configFile.write(reinterpret_cast<const char*>(&microIconScale),     sizeof(microIconScale))     &&
           configFile.write(reinterpret_cast<const char*>(&microIconPositionX), sizeof(microIconPositionX)) &&
           configFile.write(reinterpret_cast<const char*>(&microIconPositionY), sizeof(microIconPositionY)) &&
           configFile.write(reinterpret_cast<const char*>(&microIconColor),     sizeof(microIconColor))     &&
           configFile.write(reinterpret_cast<const char*>(&microIconAngle),     sizeof(microIconAngle))     &&
           configFile.write(reinterpret_cast<const char*>(&voiceEffect),        sizeof(voiceEffect))        &&

           configFile.write(reinterpret_cast<const char*>(&kExtMagic),          sizeof(kExtMagic))          &&
           configFile.write(reinterpret_cast<const char*>(&gateEnable),         sizeof(gateEnable))         &&
           configFile.write(reinterpret_cast<const char*>(&gateSensitivity),    sizeof(gateSensitivity))    &&
           configFile.write(reinterpret_cast<const char*>(&autoGain),           sizeof(autoGain))           &&
           configFile.write(reinterpret_cast<const char*>(&rogerBeep),          sizeof(rogerBeep))          &&
           configFile.write(reinterpret_cast<const char*>(&autoRecover),        sizeof(autoRecover))        &&
           configFile.write(reinterpret_cast<const char*>(&iconStyle),          sizeof(iconStyle))          &&
           configFile.write(reinterpret_cast<const char*>(&iconBars),           sizeof(iconBars))           &&
           configFile.write(reinterpret_cast<const char*>(&iconFade),           sizeof(iconFade))           &&
           configFile.write(reinterpret_cast<const char*>(&iconFadeDelay),      sizeof(iconFadeDelay))      &&
           configFile.write(reinterpret_cast<const char*>(&iconColorIdle),      sizeof(iconColorIdle))      &&
           configFile.write(reinterpret_cast<const char*>(&iconColorTalk),      sizeof(iconColorTalk))      &&
           configFile.write(reinterpret_cast<const char*>(&iconColorMuted),     sizeof(iconColorMuted))     &&

           configFile.write(reinterpret_cast<const char*>(&kExt2Magic),         sizeof(kExt2Magic))         &&
           configFile.write(reinterpret_cast<const char*>(&lowCut),             sizeof(lowCut))             &&
           configFile.write(reinterpret_cast<const char*>(&compressor),         sizeof(compressor))         &&
           configFile.write(reinterpret_cast<const char*>(&customPitch),        sizeof(customPitch))        ;
}

void PluginConfig::Reset() noexcept
{
    PluginConfig::loadStatus = false;

    PluginConfig::playbackLoaded = false;
    PluginConfig::speakerLoaded = false;
    PluginConfig::recordLoaded = false;
    PluginConfig::microLoaded = false;

    PluginConfig::soundEnable = kDefValSoundEnable;
    PluginConfig::soundVolume = kDefValSoundVolume;
    PluginConfig::soundBalancer = kDefValSoundBalancer;
    PluginConfig::soundFilter = kDefValSoundFilter;

    PluginConfig::speakerIconScale = kDefValSpeakerIconScale;
    PluginConfig::speakerIconOffsetX = kDefValSpeakerIconOffsetX;
    PluginConfig::speakerIconOffsetY = kDefValSpeakerIconOffsetY;

    PluginConfig::microEnable = kDefValMicroEnable;
    PluginConfig::microVolume = kDefValMicroVolume;
    PluginConfig::voiceEffect = kDefValVoiceEffect;
    PluginConfig::deviceName.clear();

    PluginConfig::microIconScale = kDefValMicroIconScale;
    PluginConfig::microIconPositionX = kDefValMicroIconPositionX;
    PluginConfig::microIconPositionY = kDefValMicroIconPositionY;
    PluginConfig::microIconColor = kDefValMicroIconColor;
    PluginConfig::microIconAngle = kDefValMicroIconAngle;

    PluginConfig::gateEnable = kDefValGateEnable;
    PluginConfig::gateSensitivity = kDefValGateSensitivity;
    PluginConfig::autoGain = kDefValAutoGain;
    PluginConfig::rogerBeep = kDefValRogerBeep;
    PluginConfig::autoRecover = kDefValAutoRecover;
    PluginConfig::iconStyle = kDefValIconStyle;
    PluginConfig::lowCut = kDefValLowCut;
    PluginConfig::compressor = kDefValCompressor;
    PluginConfig::customPitch = kDefValCustomPitch;
    PluginConfig::iconBars = kDefValIconBars;
    PluginConfig::iconFade = kDefValIconFade;
    PluginConfig::iconFadeDelay = kDefValIconFadeDelay;
    PluginConfig::iconColorIdle = kDefValIconColorIdle;
    PluginConfig::iconColorTalk = kDefValIconColorTalk;
    PluginConfig::iconColorMuted = kDefValIconColorMuted;
}

bool PluginConfig::IsLoaded() noexcept
{
    return PluginConfig::loadStatus;
}

bool PluginConfig::IsPlaybackLoaded() noexcept
{
    return PluginConfig::playbackLoaded;
}

bool PluginConfig::IsSpeakerLoaded() noexcept
{
    return PluginConfig::speakerLoaded;
}

bool PluginConfig::IsRecordLoaded() noexcept
{
    return PluginConfig::recordLoaded;
}

bool PluginConfig::IsMicroLoaded() noexcept
{
    return PluginConfig::microLoaded;
}

void PluginConfig::SetPlaybackLoaded(const bool status) noexcept
{
    PluginConfig::playbackLoaded = status;
}

void PluginConfig::SetSpeakerLoaded(const bool status) noexcept
{
    PluginConfig::speakerLoaded = status;
}

void PluginConfig::SetRecordLoaded(const bool status) noexcept
{
    PluginConfig::recordLoaded = status;
}

void PluginConfig::SetMicroLoaded(const bool status) noexcept
{
    PluginConfig::microLoaded = status;
}

bool PluginConfig::GetSoundEnable() noexcept
{
    return PluginConfig::soundEnable;
}

int PluginConfig::GetSoundVolume() noexcept
{
    return PluginConfig::soundVolume;
}

bool PluginConfig::GetSoundBalancer() noexcept
{
    return PluginConfig::soundBalancer;
}

bool PluginConfig::GetSoundFilter() noexcept
{
    return PluginConfig::soundFilter;
}

float PluginConfig::GetSpeakerIconScale() noexcept
{
    return PluginConfig::speakerIconScale;
}

int PluginConfig::GetSpeakerIconOffsetX() noexcept
{
    float screenValue { 0.f };

    return Render::ConvertBaseXValueToScreenXValue(PluginConfig::speakerIconOffsetX, screenValue)
        ? std::roundf(screenValue) : kDefValSpeakerIconOffsetX;
}

int PluginConfig::GetSpeakerIconOffsetY() noexcept
{
    float screenValue { 0.f };

    return Render::ConvertBaseYValueToScreenYValue(PluginConfig::speakerIconOffsetY, screenValue)
        ? std::roundf(screenValue) : kDefValSpeakerIconOffsetY;
}

bool PluginConfig::GetMicroEnable() noexcept
{
    return PluginConfig::microEnable;
}

int PluginConfig::GetVoiceEffect() noexcept
{
    return PluginConfig::voiceEffect;
}

int PluginConfig::GetMicroVolume() noexcept
{
    return PluginConfig::microVolume;
}

const std::string& PluginConfig::GetDeviceName() noexcept
{
    return PluginConfig::deviceName;
}

float PluginConfig::GetMicroIconScale() noexcept
{
    return PluginConfig::microIconScale;
}

int PluginConfig::GetMicroIconPositionX() noexcept
{
    float screenValue { 0.f };

    return Render::ConvertBaseXValueToScreenXValue(PluginConfig::microIconPositionX, screenValue)
        ? std::roundf(screenValue) : kDefValMicroIconPositionX;
}

int PluginConfig::GetMicroIconPositionY() noexcept
{
    float screenValue { 0.f };

    return Render::ConvertBaseYValueToScreenYValue(PluginConfig::microIconPositionY, screenValue)
        ? std::roundf(screenValue) : kDefValMicroIconPositionY;
}

D3DCOLOR PluginConfig::GetMicroIconColor() noexcept
{
    return PluginConfig::microIconColor;
}

float PluginConfig::GetMicroIconAngle() noexcept
{
    return PluginConfig::microIconAngle;
}

void PluginConfig::SetSoundEnable(const bool soundEnable) noexcept
{
    PluginConfig::soundEnable = soundEnable;
}

void PluginConfig::SetSoundVolume(const int soundVolume) noexcept
{
    PluginConfig::soundVolume = soundVolume;
}

void PluginConfig::SetSoundBalancer(const bool soundBalancer) noexcept
{
    PluginConfig::soundBalancer = soundBalancer;
}

void PluginConfig::SetSoundFilter(const bool soundFilter) noexcept
{
    PluginConfig::soundFilter = soundFilter;
}

void PluginConfig::SetSpeakerIconScale(const float speakerIconScale) noexcept
{
    PluginConfig::speakerIconScale = speakerIconScale;
}

void PluginConfig::SetSpeakerIconOffsetX(const int speakerIconOffsetX) noexcept
{
    float baseValue { 0.f };

    if (Render::ConvertScreenXValueToBaseXValue(speakerIconOffsetX, baseValue))
        PluginConfig::speakerIconOffsetX = std::roundf(baseValue);
}

void PluginConfig::SetSpeakerIconOffsetY(const int speakerIconOffsetY) noexcept
{
    float baseValue { 0.f };

    if (Render::ConvertScreenYValueToBaseYValue(speakerIconOffsetY, baseValue))
        PluginConfig::speakerIconOffsetY = std::roundf(baseValue);
}

void PluginConfig::SetMicroEnable(const bool microEnable) noexcept
{
    PluginConfig::microEnable = microEnable;
}

void PluginConfig::SetVoiceEffect(const int voiceEffect) noexcept
{
    PluginConfig::voiceEffect = voiceEffect;
}

void PluginConfig::SetMicroVolume(const int microVolume) noexcept
{
    PluginConfig::microVolume = microVolume;
}

void PluginConfig::SetDeviceName(std::string deviceName) noexcept
{
    PluginConfig::deviceName = std::move(deviceName);
}

void PluginConfig::SetMicroIconScale(const float microIconScale) noexcept
{
    PluginConfig::microIconScale = microIconScale;
}

void PluginConfig::SetMicroIconPositionX(const int microIconPositionX) noexcept
{
    float baseValue { 0.f };

    if (Render::ConvertScreenXValueToBaseXValue(microIconPositionX, baseValue))
        PluginConfig::microIconPositionX = std::roundf(baseValue);
}

void PluginConfig::SetMicroIconPositionY(const int microIconPositionY) noexcept
{
    float baseValue { 0.f };

    if (Render::ConvertScreenYValueToBaseYValue(microIconPositionY, baseValue))
        PluginConfig::microIconPositionY = std::roundf(baseValue);
}

void PluginConfig::SetMicroIconColor(const D3DCOLOR microIconColor) noexcept
{
    PluginConfig::microIconColor = microIconColor;
}

void PluginConfig::SetMicroIconAngle(const float microIconAngle) noexcept
{
    PluginConfig::microIconAngle = microIconAngle;
}

bool PluginConfig::loadStatus { false };

bool PluginConfig::playbackLoaded { false };
bool PluginConfig::speakerLoaded { false };
bool PluginConfig::recordLoaded { false };
bool PluginConfig::microLoaded { false };

bool PluginConfig::soundEnable { kDefValSoundEnable };
int PluginConfig::soundVolume { kDefValSoundVolume };
bool PluginConfig::soundBalancer { kDefValSoundBalancer };
bool PluginConfig::soundFilter { kDefValSoundFilter };

float PluginConfig::speakerIconScale { kDefValSpeakerIconScale };
int PluginConfig::speakerIconOffsetX { kDefValSpeakerIconOffsetX };
int PluginConfig::speakerIconOffsetY { kDefValSpeakerIconOffsetY };

bool PluginConfig::microEnable { kDefValMicroEnable };
int PluginConfig::microVolume { kDefValMicroVolume };
int PluginConfig::voiceEffect { kDefValVoiceEffect };
std::string PluginConfig::deviceName;

float PluginConfig::microIconScale { kDefValMicroIconScale };
int PluginConfig::microIconPositionX { kDefValMicroIconPositionX };
int PluginConfig::microIconPositionY { kDefValMicroIconPositionY };
D3DCOLOR PluginConfig::microIconColor { kDefValMicroIconColor };
float PluginConfig::microIconAngle { kDefValMicroIconAngle };

// ---- extended settings (noise gate, auto gain, roger beep, mic recovery, icon style) ----
bool PluginConfig::GetGateEnable() noexcept { return PluginConfig::gateEnable; }
int PluginConfig::GetGateSensitivity() noexcept { return PluginConfig::gateSensitivity; }
bool PluginConfig::GetAutoGain() noexcept { return PluginConfig::autoGain; }
bool PluginConfig::GetRogerBeep() noexcept { return PluginConfig::rogerBeep; }
bool PluginConfig::GetAutoRecover() noexcept { return PluginConfig::autoRecover; }
int PluginConfig::GetIconStyle() noexcept { return PluginConfig::iconStyle; }
bool PluginConfig::GetLowCut() noexcept { return PluginConfig::lowCut; }
bool PluginConfig::GetCompressor() noexcept { return PluginConfig::compressor; }
int PluginConfig::GetCustomPitch() noexcept { return PluginConfig::customPitch; }
void PluginConfig::SetLowCut(const bool v) noexcept { PluginConfig::lowCut = v; }
void PluginConfig::SetCompressor(const bool v) noexcept { PluginConfig::compressor = v; }
void PluginConfig::SetCustomPitch(const int v) noexcept { PluginConfig::customPitch = v < -12 ? -12 : (v > 12 ? 12 : v); }
bool PluginConfig::GetIconBars() noexcept { return PluginConfig::iconBars; }
bool PluginConfig::GetIconFade() noexcept { return PluginConfig::iconFade; }
int PluginConfig::GetIconFadeDelay() noexcept { return PluginConfig::iconFadeDelay; }
D3DCOLOR PluginConfig::GetIconColorIdle() noexcept { return PluginConfig::iconColorIdle; }
D3DCOLOR PluginConfig::GetIconColorTalk() noexcept { return PluginConfig::iconColorTalk; }
D3DCOLOR PluginConfig::GetIconColorMuted() noexcept { return PluginConfig::iconColorMuted; }

void PluginConfig::SetGateEnable(const bool v) noexcept { PluginConfig::gateEnable = v; }
void PluginConfig::SetGateSensitivity(const int v) noexcept { PluginConfig::gateSensitivity = v < 0 ? 0 : (v > 100 ? 100 : v); }
void PluginConfig::SetAutoGain(const bool v) noexcept { PluginConfig::autoGain = v; }
void PluginConfig::SetRogerBeep(const bool v) noexcept { PluginConfig::rogerBeep = v; }
void PluginConfig::SetAutoRecover(const bool v) noexcept { PluginConfig::autoRecover = v; }
void PluginConfig::SetIconStyle(const int v) noexcept { PluginConfig::iconStyle = (v < 0 || v > 2) ? 0 : v; }
void PluginConfig::SetIconBars(const bool v) noexcept { PluginConfig::iconBars = v; }
void PluginConfig::SetIconFade(const bool v) noexcept { PluginConfig::iconFade = v; }
void PluginConfig::SetIconFadeDelay(const int v) noexcept { PluginConfig::iconFadeDelay = v < 1 ? 1 : (v > 60 ? 60 : v); }
void PluginConfig::SetIconColorIdle(const D3DCOLOR v) noexcept { PluginConfig::iconColorIdle = v & 0x00ffffff; }
void PluginConfig::SetIconColorTalk(const D3DCOLOR v) noexcept { PluginConfig::iconColorTalk = v & 0x00ffffff; }
void PluginConfig::SetIconColorMuted(const D3DCOLOR v) noexcept { PluginConfig::iconColorMuted = v & 0x00ffffff; }

bool PluginConfig::gateEnable { kDefValGateEnable };
int PluginConfig::gateSensitivity { kDefValGateSensitivity };
bool PluginConfig::autoGain { kDefValAutoGain };
bool PluginConfig::rogerBeep { kDefValRogerBeep };
bool PluginConfig::autoRecover { kDefValAutoRecover };
int PluginConfig::iconStyle { kDefValIconStyle };
bool PluginConfig::lowCut { kDefValLowCut };
bool PluginConfig::compressor { kDefValCompressor };
int PluginConfig::customPitch { kDefValCustomPitch };
bool PluginConfig::iconBars { kDefValIconBars };
bool PluginConfig::iconFade { kDefValIconFade };
int PluginConfig::iconFadeDelay { kDefValIconFadeDelay };
D3DCOLOR PluginConfig::iconColorIdle { kDefValIconColorIdle };
D3DCOLOR PluginConfig::iconColorTalk { kDefValIconColorTalk };
D3DCOLOR PluginConfig::iconColorMuted { kDefValIconColorMuted };
