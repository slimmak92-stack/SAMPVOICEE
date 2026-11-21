/*
    This is a SampVoice project file
    Developer: CyberMor <cyber.mor.2020@gmail.ru>

    See more here https://github.com/CyberMor/sampvoice

    Copyright (c) Daniel (CyberMor) 2020 All rights reserved
*/

#include "Record.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include <util/Logger.h>

#include "PluginConfig.h"

static_assert(voice_dsp::EffectCount == PluginConfig::kVoiceEffectCount, "voice effect count mismatch");

bool Record::Init(const DWORD bitrate) noexcept
{
    if (Record::initStatus)
        return false;

    if (BASS_IsStarted() == FALSE)
        return false;

    Logger::LogToFile("[sv:dbg:record:init] : module initializing...");

    Record::deviceNamesList.clear();
    Record::deviceNumbersList.clear();

    {
        BASS_DEVICEINFO devInfo {};

        for (int devNumber { 0 }; BASS_RecordGetDeviceInfo(devNumber, &devInfo); ++devNumber)
        {
            const bool deviceEnabled = devInfo.flags & BASS_DEVICE_ENABLED;
            const bool deviceLoopback = devInfo.flags & BASS_DEVICE_LOOPBACK;
            const DWORD deviceType = devInfo.flags & BASS_DEVICE_TYPE_MASK;

            Logger::LogToFile("[sv:dbg:record:init] : device detect "
                "[ id(%d) enabled(%hhu) loopback(%hhu) name(%s) type(0x%x) ]",
                devNumber, deviceEnabled, deviceLoopback, devInfo.name != nullptr
                ? devInfo.name : "none", deviceType);

            if (deviceEnabled && !deviceLoopback && devInfo.name != nullptr)
            {
                try
                {
                    Record::deviceNumbersList.emplace_back(devNumber);
                    Record::deviceNamesList.emplace_back(devInfo.name);
                }
                catch (const std::exception& exception)
                {
                    Logger::LogToFile("[sv:err:record:init] : failed to add device");
                    return false;
                }
            }
        }
    }

    Memory::ScopeExit deviceListsResetScope { [] { Record::usedDeviceIndex = -1;
                                                   Record::deviceNamesList.clear();
                                                   Record::deviceNumbersList.clear(); } };

    if (Record::deviceNamesList.empty() || Record::deviceNumbersList.empty())
    {
        Logger::LogToFile("[sv:inf:record:init] : failed to find microphone");
        return false;
    }

    {
        int opusErrorCode { -1 };

        Record::encoder = opus_encoder_create(SV::kFrequency, 1,
            OPUS_APPLICATION_VOIP, &opusErrorCode);

        if (Record::encoder == nullptr || opusErrorCode < 0)
        {
            Logger::LogToFile("[sv:err:record:init] : failed to "
                "create encoder (code:%d)", opusErrorCode);
            return false;
        }
    }

    Memory::ScopeExit encoderResetScope { [] { opus_encoder_destroy(Record::encoder); } };

    if (const auto error = opus_encoder_ctl(Record::encoder,
        OPUS_SET_BITRATE(bitrate)); error < 0)
    {
        Logger::LogToFile("[sv:err:record:init] : failed to "
            "set bitrate for encoder (code:%d)", error);
        return false;
    }

    if (const auto error = opus_encoder_ctl(Record::encoder,
        OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE)); error < 0)
    {
        Logger::LogToFile("[sv:err:record:init] : failed to "
            "set audiosignal type for encoder (code:%d)", error);
        return false;
    }

    if (const auto error = opus_encoder_ctl(Record::encoder,
        OPUS_SET_COMPLEXITY(10)); error < 0)
    {
        Logger::LogToFile("[sv:err:record:init] : failed to "
            "set complexity for encoder (code:%d)", error);
        return false;
    }

    if (const auto error = opus_encoder_ctl(Record::encoder,
        OPUS_SET_PREDICTION_DISABLED(FALSE)); error < 0)
    {
        Logger::LogToFile("[sv:err:record:init] : failed to "
            "enable prediction for encoder (code:%d)", error);
        return false;
    }

    if (const auto error = opus_encoder_ctl(Record::encoder,
        OPUS_SET_LSB_DEPTH(16)); error < 0)
    {
        Logger::LogToFile("[sv:err:record:init] : failed to "
            "set lsb depth for encoder (code:%d)", error);
        return false;
    }

    if (const auto error = opus_encoder_ctl(Record::encoder,
        OPUS_SET_FORCE_CHANNELS(TRUE)); error < 0)
    {
        Logger::LogToFile("[sv:err:record:init] : failed to "
            "set count channels for encoder (code:%d)", error);
        return false;
    }

    if (const auto error = opus_encoder_ctl(Record::encoder,
        OPUS_SET_DTX(FALSE)); error < 0)
    {
        Logger::LogToFile("[sv:err:record:init] : failed to "
            "set dtx for encoder (code:%d)", error);
        return false;
    }

    if (const auto error = opus_encoder_ctl(Record::encoder,
        OPUS_SET_INBAND_FEC(TRUE)); error < 0)
    {
        Logger::LogToFile("[sv:err:record:init] : failed to "
            "set inband fec for encoder (code:%d)", error);
        return false;
    }

    if (const auto error = opus_encoder_ctl(Record::encoder,
        OPUS_SET_PACKET_LOSS_PERC(10)); error < 0)
    {
        Logger::LogToFile("[sv:err:record:init] : failed to set "
            "packet loss percent for encoder (code:%d)", error);
        return false;
    }

    Record::usedDeviceIndex = -1;

    if (PluginConfig::IsRecordLoaded() && !PluginConfig::GetDeviceName().empty())
    {
        for (std::size_t i { 0 }; i < Record::deviceNamesList.size(); ++i)
        {
            if (Record::deviceNamesList[i] == PluginConfig::GetDeviceName())
            {
                Record::usedDeviceIndex = i;
                break;
            }
        }
    }

    bool initRecordStatus = BASS_RecordInit(Record::usedDeviceIndex != -1 ?
                            Record::deviceNumbersList[Record::usedDeviceIndex] : -1);

    if (!initRecordStatus && Record::usedDeviceIndex != -1)
    {
        initRecordStatus = BASS_RecordInit(Record::usedDeviceIndex = -1);
    }

    if (initRecordStatus && Record::usedDeviceIndex == -1)
    {
        for (std::size_t i { 0 }; i < Record::deviceNumbersList.size(); ++i)
        {
            if (Record::deviceNumbersList[i] == BASS_RecordGetDevice())
            {
                Record::usedDeviceIndex = i;
                break;
            }
        }
    }

    Memory::ScopeExit recordResetScope { [] { BASS_RecordFree(); } };

    if (!initRecordStatus || Record::usedDeviceIndex == -1)
    {
        Logger::LogToFile("[sv:err:record:init] : failed to "
            "init device (code:%d)", BASS_ErrorGetCode());
        return false;
    }

    if (!PluginConfig::IsRecordLoaded())
    {
        PluginConfig::SetDeviceName(Record::deviceNamesList[Record::usedDeviceIndex]);
    }

    Record::recordChannel = BASS_RecordStart(SV::kFrequency, 1,
        BASS_RECORD_PAUSE, nullptr, nullptr);

    if (Record::recordChannel == NULL)
    {
        Logger::LogToFile("[sv:err:record:init] : failed to create record "
            "stream (code:%d)", BASS_ErrorGetCode());
        return false;
    }

    Memory::ScopeExit channelResetScope { [] { BASS_ChannelStop(Record::recordChannel); } };

    Record::checkChannel = BASS_StreamCreate(SV::kFrequency, 1,
        NULL, STREAMPROC_PUSH, nullptr);

    if (Record::checkChannel == NULL)
    {
        Logger::LogToFile("[sv:err:record:init] : failed to create "
            "check stream (code:%d)", BASS_ErrorGetCode());
        return false;
    }

    BASS_ChannelSetAttribute(Record::checkChannel, BASS_ATTRIB_VOL, 4.f);

    if (!PluginConfig::IsRecordLoaded())
    {
        PluginConfig::SetRecordLoaded(true);
        Record::ResetConfigs();
    }

    deviceListsResetScope.Release();
    encoderResetScope.Release();
    recordResetScope.Release();
    channelResetScope.Release();

    Logger::LogToFile("[sv:dbg:record:init] : module initialized");

    Record::voiceDsp.Initialize(static_cast<float>(SV::kFrequency));

    Record::microAvailable = true;
    Record::nextPollTick = GetTickCount() + 1500;
    Record::initStatus = true;
    Record::SyncConfigs();

    return true;
}

void Record::Free() noexcept
{
    if (!Record::initStatus)
        return;

    Logger::LogToFile("[sv:dbg:record:free] : module releasing...");

    Record::StopRecording();
    BASS_ChannelStop(Record::recordChannel);
    BASS_RecordFree();

    Record::StopChecking();
    BASS_StreamFree(Record::checkChannel);

    opus_encoder_destroy(Record::encoder);

    Record::usedDeviceIndex = -1;
    Record::deviceNumbersList.clear();
    Record::deviceNamesList.clear();

    Logger::LogToFile("[sv:dbg:record:free] : module released");

    Record::microAvailable = false;
    Record::rogerLeft = 0;
    Record::initStatus = false;
}

void Record::Tick() noexcept
{
    Record::PollDevices();

    if (Record::initStatus && Record::checkStatus)
    {
        if (const auto bufferSize = BASS_ChannelGetData(Record::recordChannel,
            nullptr, BASS_DATA_AVAILABLE); bufferSize != -1 && bufferSize != 0)
        {
            if (const auto readDataSize = BASS_ChannelGetData(Record::recordChannel, Record::encBuffer.data(),
                std::clamp(bufferSize, 0ul, SV::kFrameSizeInBytes)); readDataSize != -1 && readDataSize != 0)
            {
                Record::ApplyVoiceDsp(Record::encBuffer.data(), readDataSize / sizeof(opus_int16));
                BASS_StreamPutData(Record::checkChannel, Record::encBuffer.data(), readDataSize);
            }
        }
    }
}

DWORD Record::GetFrame(BYTE* const bufferPtr, const DWORD bufferSize) noexcept
{
    if (!Record::initStatus || !Record::recordStatus || Record::checkStatus)
        return NULL;

    const auto cBufferSize = BASS_ChannelGetData(Record::recordChannel, nullptr, BASS_DATA_AVAILABLE);
    if (cBufferSize == -1 || cBufferSize < SV::kFrameSizeInBytes) return NULL;

    if (BASS_ChannelGetData(Record::recordChannel, Record::encBuffer.data(),
        SV::kFrameSizeInBytes) != SV::kFrameSizeInBytes) return NULL;

    if (Record::rogerLeft == 1)
    {
        // roger beep replaces the (already discarded) microphone frame; the DSP is skipped on purpose
        Record::FillRogerBeep(Record::encBuffer.data(), SV::kFrameSizeInSamples);
        Record::rogerLeft = 0;
    }
    else
    {
        if (Record::rogerLeft == 2) Record::rogerLeft = 1;

        // noise gate -> auto gain -> voice effect -> limiter
        Record::ApplyVoiceDsp(Record::encBuffer.data(), SV::kFrameSizeInSamples);
    }

    const auto encDataLength = opus_encode(Record::encoder, Record::encBuffer.data(),
        SV::kFrameSizeInSamples, bufferPtr, bufferSize);

    return encDataLength > 0 ? static_cast<DWORD>(encDataLength) : NULL;
}

bool Record::HasMicro() noexcept
{
    BASS_DEVICEINFO devInfo {};

    for (DWORD devNumber { 0 }; BASS_RecordGetDeviceInfo(devNumber, &devInfo); ++devNumber)
    {
        const bool deviceEnabled = devInfo.flags & BASS_DEVICE_ENABLED;
        const bool deviceLoopback = devInfo.flags & BASS_DEVICE_LOOPBACK;

        if (deviceEnabled && !deviceLoopback && devInfo.name != nullptr)
            return true;
    }

    return false;
}

bool Record::StartRecording() noexcept
{
    if (!Record::initStatus || Record::recordStatus || Record::checkStatus)
        return false;

    if (!PluginConfig::GetMicroEnable())
        return false;

    Logger::LogToFile("[sv:dbg:record:startrecording] : channel recording starting...");

    Record::rogerLeft = 0;
    Record::voiceDsp.RequestReset();
    BASS_ChannelPlay(Record::recordChannel, FALSE);
    Record::recordStatus = true;

    return true;
}

bool Record::IsRecording() noexcept
{
    return Record::recordStatus;
}

void Record::StopRecording() noexcept
{
    if (!Record::initStatus)
        return;

    Record::recordStatus = false;
    Record::rogerLeft = 0;

    if (Record::checkStatus)
        return;

    BASS_ChannelPause(Record::recordChannel);
    opus_encoder_ctl(Record::encoder, OPUS_RESET_STATE);

    Logger::LogToFile("[sv:dbg:record:stoprecording] : channel recording stoped");

    const auto bufferSize = BASS_ChannelGetData(Record::recordChannel, nullptr, BASS_DATA_AVAILABLE);
    if (bufferSize == -1 && bufferSize == 0) return;

    BASS_ChannelGetData(Record::recordChannel, nullptr, bufferSize);
}

bool Record::StartChecking() noexcept
{
    if (!Record::initStatus || Record::checkStatus)
        return false;

    if (!PluginConfig::GetMicroEnable())
        return false;

    Record::StopRecording();

    Logger::LogToFile("[sv:dbg:record:startchecking] : checking device starting...");

    BASS_ChannelPlay(Record::checkChannel, TRUE);
    Record::voiceDsp.RequestReset();
    BASS_ChannelPlay(Record::recordChannel, TRUE);
    Record::checkStatus = true;

    return true;
}

bool Record::IsChecking() noexcept
{
    return Record::checkStatus;
}

void Record::StopChecking() noexcept
{
    if (Record::initStatus && Record::checkStatus)
    {
        Logger::LogToFile("[sv:dbg:record:stopchecking] : checking device stoped");

        BASS_ChannelStop(Record::checkChannel);
        Record::checkStatus = false;
    }
}

bool Record::GetMicroEnable() noexcept
{
    return PluginConfig::GetMicroEnable();
}

int Record::GetMicroVolume() noexcept
{
    return PluginConfig::GetMicroVolume();
}

int Record::GetMicroDevice() noexcept
{
    return Record::usedDeviceIndex;
}

void Record::SetMicroEnable(const bool microEnable) noexcept
{
    if (!Record::initStatus)
        return;

    PluginConfig::SetMicroEnable(microEnable);

    if (!PluginConfig::GetMicroEnable())
    {
        Record::StopRecording();
        Record::StopChecking();
    }
}

void Record::SetMicroVolume(const int microVolume) noexcept
{
    if (!Record::initStatus)
        return;

    PluginConfig::SetMicroVolume(std::clamp(microVolume, 0, 100));

    BASS_RecordSetInput(-1, BASS_INPUT_ON, static_cast<float>
        (PluginConfig::GetMicroVolume()) / 100.f);
}

void Record::SetMicroDevice(const int deviceIndex) noexcept
{
    if (!Record::initStatus)
        return;

    const auto devNamesCount = Record::deviceNamesList.size();
    const auto devNumbersCount = Record::deviceNumbersList.size();

    if (deviceIndex < 0 || deviceIndex >= min(devNamesCount, devNumbersCount))
        return;

    if (deviceIndex == Record::usedDeviceIndex)
        return;

    BASS_ChannelStop(Record::recordChannel);
    BASS_RecordFree();

    const auto oldDevIndex = Record::usedDeviceIndex;

    if (BASS_RecordInit(Record::deviceNumbersList[Record::usedDeviceIndex = deviceIndex]) == FALSE &&
        BASS_RecordInit(Record::deviceNumbersList[Record::usedDeviceIndex = oldDevIndex]) == FALSE)
        return;

    Record::recordChannel = BASS_RecordStart(SV::kFrequency, 1, !Record::recordStatus &&
        !Record::checkStatus ? BASS_RECORD_PAUSE : NULL, nullptr, nullptr);

    PluginConfig::SetDeviceName(Record::deviceNamesList[Record::usedDeviceIndex]);
}

int Record::GetVoiceEffect() noexcept
{
    return Record::voiceDsp.GetEffect();
}

void Record::SetVoiceEffect(const int voiceEffect) noexcept
{
    Record::voiceDsp.SetEffect(voiceEffect);
    PluginConfig::SetVoiceEffect(Record::voiceDsp.GetEffect());
}

void Record::ApplyVoiceDsp(opus_int16* const samples, const size_t count) noexcept
{
    if (samples == nullptr || count == 0 || count > Record::dspBuffer.size())
        return;

    for (size_t i = 0; i != count; ++i)
        Record::dspBuffer[i] = static_cast<float>(samples[i]) / 32768.f;

    Record::voiceDsp.Process(Record::dspBuffer.data(), Record::dspBuffer.data(), count);

    for (size_t i = 0; i != count; ++i)
        samples[i] = static_cast<opus_int16>(std::clamp(Record::dspBuffer[i] * 32767.f, -32768.f, 32767.f));
}

void Record::SyncConfigs() noexcept
{
    Record::SetMicroEnable(PluginConfig::GetMicroEnable());
    Record::SetMicroVolume(PluginConfig::GetMicroVolume());
    Record::voiceDsp.SetEffect(PluginConfig::GetVoiceEffect());
    Record::ApplyDspConfig();
}

void Record::ResetConfigs() noexcept
{
    PluginConfig::SetGateEnable(PluginConfig::kDefValGateEnable);
    PluginConfig::SetGateSensitivity(PluginConfig::kDefValGateSensitivity);
    PluginConfig::SetAutoGain(PluginConfig::kDefValAutoGain);
    PluginConfig::SetRogerBeep(PluginConfig::kDefValRogerBeep);
    PluginConfig::SetAutoRecover(PluginConfig::kDefValAutoRecover);
    PluginConfig::SetMicroEnable(PluginConfig::kDefValMicroEnable);
    PluginConfig::SetMicroVolume(PluginConfig::kDefValMicroVolume);
    PluginConfig::SetVoiceEffect(PluginConfig::kDefValVoiceEffect);
    PluginConfig::SetLowCut(PluginConfig::kDefValLowCut);
    PluginConfig::SetCompressor(PluginConfig::kDefValCompressor);
    PluginConfig::SetCustomPitch(PluginConfig::kDefValCustomPitch);
}

bool Record::IsInitialized() noexcept
{
    return Record::initStatus;
}

bool Record::IsMicroAvailable() noexcept
{
    return Record::initStatus && Record::microAvailable;
}

void Record::ArmRoger() noexcept
{
    if (Record::initStatus && Record::recordStatus && !Record::checkStatus && PluginConfig::GetRogerBeep())
        Record::rogerLeft = 2; // one more normal frame (last words), then the beep
}

void Record::CancelRoger() noexcept
{
    Record::rogerLeft = 0;
}

bool Record::IsRogerPending() noexcept
{
    return Record::rogerLeft > 0;
}

bool Record::GetGateEnable() noexcept { return PluginConfig::GetGateEnable(); }
int Record::GetGateSensitivity() noexcept { return PluginConfig::GetGateSensitivity(); }
bool Record::GetAutoGain() noexcept { return PluginConfig::GetAutoGain(); }
bool Record::GetRogerBeep() noexcept { return PluginConfig::GetRogerBeep(); }
bool Record::GetAutoRecover() noexcept { return PluginConfig::GetAutoRecover(); }
bool Record::GetLowCut() noexcept { return PluginConfig::GetLowCut(); }
bool Record::GetCompressor() noexcept { return PluginConfig::GetCompressor(); }
int Record::GetCustomPitch() noexcept { return PluginConfig::GetCustomPitch(); }
float Record::GetInputLevel() noexcept { return Record::voiceDsp.GetInputLevel(); }
float Record::GetOutputLevel() noexcept { return Record::voiceDsp.GetOutputLevel(); }

void Record::SetGateEnable(const bool enable) noexcept { PluginConfig::SetGateEnable(enable); Record::ApplyDspConfig(); }
void Record::SetGateSensitivity(const int sensitivity) noexcept { PluginConfig::SetGateSensitivity(sensitivity); Record::ApplyDspConfig(); }
void Record::SetAutoGain(const bool enable) noexcept { PluginConfig::SetAutoGain(enable); Record::ApplyDspConfig(); }
void Record::SetRogerBeep(const bool enable) noexcept { PluginConfig::SetRogerBeep(enable); }
void Record::SetAutoRecover(const bool enable) noexcept { PluginConfig::SetAutoRecover(enable); }
void Record::SetLowCut(const bool enable) noexcept { PluginConfig::SetLowCut(enable); Record::ApplyDspConfig(); }
void Record::SetCompressor(const bool enable) noexcept { PluginConfig::SetCompressor(enable); Record::ApplyDspConfig(); }
void Record::SetCustomPitch(const int semitones) noexcept { PluginConfig::SetCustomPitch(semitones); Record::ApplyDspConfig(); }

void Record::ApplyDspConfig() noexcept
{
    // sensitivity 0..100 -> RMS threshold 0.0008 .. 0.032 (logarithmic)
    const float threshold = 0.0008f * std::pow(40.f, static_cast<float>(PluginConfig::GetGateSensitivity()) / 100.f);

    Record::voiceDsp.SetGate(PluginConfig::GetGateEnable(), threshold);
    Record::voiceDsp.SetAutoGain(PluginConfig::GetAutoGain());
    Record::voiceDsp.SetLowCut(PluginConfig::GetLowCut());
    Record::voiceDsp.SetCompressor(PluginConfig::GetCompressor());
    Record::voiceDsp.SetCustomPitch(PluginConfig::GetCustomPitch());
}

void Record::FillRogerBeep(opus_int16* const samples, const size_t count) noexcept
{
    // two short tones (1400 Hz -> 1900 Hz), 80 ms in total, soft edges
    const float rate = static_cast<float>(SV::kFrequency);
    const size_t beepLength = static_cast<size_t>(0.080f * rate);
    const float fade = 0.008f * rate;

    float phase = 0.f;

    for (size_t i = 0; i != count; ++i)
    {
        float value = 0.f;

        if (i < beepLength)
        {
            const float freq = i < beepLength / 2 ? 1400.f : 1900.f;
            phase += 6.28318530718f * freq / rate;
            if (phase > 6.28318530718f) phase -= 6.28318530718f;

            float envelope = 1.f;
            const float fi = static_cast<float>(i);
            const float fo = static_cast<float>(beepLength - i);
            if (fi < fade) envelope = fi / fade;
            if (fo < fade && fo / fade < envelope) envelope = fo / fade;

            value = 0.30f * envelope * std::sin(phase);
        }

        samples[i] = static_cast<opus_int16>(std::clamp(value * 32767.f, -32768.f, 32767.f));
    }
}

// Re-enumerates the capture devices, and re-opens the microphone when it was unplugged,
// disappeared or the configured device came back
static bool EnumerateCaptureDevices(std::vector<std::string>& names, std::vector<int>& numbers) noexcept
{
    try
    {
        names.clear();
        numbers.clear();

        BASS_DEVICEINFO devInfo {};

        for (int devNumber { 0 }; BASS_RecordGetDeviceInfo(devNumber, &devInfo); ++devNumber)
        {
            const bool deviceEnabled = devInfo.flags & BASS_DEVICE_ENABLED;
            const bool deviceLoopback = devInfo.flags & BASS_DEVICE_LOOPBACK;

            if (deviceEnabled && !deviceLoopback && devInfo.name != nullptr)
            {
                numbers.emplace_back(devNumber);
                names.emplace_back(devInfo.name);
            }
        }

        return true;
    }
    catch (...)
    {
        return false;
    }
}

void Record::PollDevices() noexcept
{
    if (!Record::initStatus)
        return;

    const DWORD now = GetTickCount();

    if (static_cast<LONG>(now - Record::nextPollTick) < 0)
        return;

    Record::nextPollTick = now + 1500;

    std::vector<std::string> names;
    std::vector<int> numbers;

    if (!EnumerateCaptureDevices(names, numbers))
        return;

    Record::microAvailable = !names.empty();

    if (!PluginConfig::GetAutoRecover())
        return;

    const bool usedValid = Record::usedDeviceIndex >= 0 &&
        Record::usedDeviceIndex < static_cast<int>(Record::deviceNamesList.size());
    const std::string usedName = usedValid ? Record::deviceNamesList[Record::usedDeviceIndex] : std::string {};

    const auto contains = [&names](const std::string& name)
    {
        return std::find(names.begin(), names.end(), name) != names.end();
    };

    const std::string& savedName = PluginConfig::GetDeviceName();

    const bool channelDead = Record::recordChannel == NULL ||
        BASS_ChannelIsActive(Record::recordChannel) == BASS_ACTIVE_STOPPED;
    const bool usedGone = usedName.empty() || !contains(usedName);
    const bool savedCameBack = !savedName.empty() && savedName != usedName && contains(savedName);
    const bool listChanged = names != Record::deviceNamesList || numbers != Record::deviceNumbersList;

    bool reopen = false;

    if (Record::microAvailable)
        reopen = channelDead || usedGone || savedCameBack || listChanged;
    else
        reopen = Record::recordChannel != NULL; // everything unplugged: release the dead device

    if (!reopen)
        return;

    // do not interrupt a live transmission for harmless changes (another device plugged in)
    if (!channelDead && !usedGone && (Record::recordStatus || Record::checkStatus))
    {
        Record::nextPollTick = now + 500;
        return;
    }

    Logger::LogToFile("[sv:inf:record:poll] : microphone state changed (dead:%d gone:%d back:%d list:%d), reopening",
        channelDead, usedGone, savedCameBack, listChanged);

    Record::ReopenDevice();
}

bool Record::ReopenDevice() noexcept
{
    const bool active = Record::recordStatus || Record::checkStatus;

    if (Record::recordChannel != NULL)
        BASS_ChannelStop(Record::recordChannel);

    BASS_RecordFree();
    Record::recordChannel = NULL;
    Record::usedDeviceIndex = -1;

    if (!EnumerateCaptureDevices(Record::deviceNamesList, Record::deviceNumbersList) ||
        Record::deviceNamesList.empty())
    {
        Record::microAvailable = false;
        Logger::LogToFile("[sv:inf:record:reopen] : no capture device available");
        return false;
    }

    int index { -1 };

    // prefer the device the user picked (it is never overwritten by the fallback)
    for (std::size_t i { 0 }; i < Record::deviceNamesList.size(); ++i)
    {
        if (Record::deviceNamesList[i] == PluginConfig::GetDeviceName())
        {
            index = static_cast<int>(i);
            break;
        }
    }

    bool initOk = BASS_RecordInit(index != -1 ? Record::deviceNumbersList[index] : -1);

    if (!initOk && index != -1)
    {
        index = -1;
        initOk = BASS_RecordInit(-1);
    }

    if (initOk && index == -1)
    {
        for (std::size_t i { 0 }; i < Record::deviceNumbersList.size(); ++i)
        {
            if (Record::deviceNumbersList[i] == BASS_RecordGetDevice())
            {
                index = static_cast<int>(i);
                break;
            }
        }
    }

    if (!initOk || index == -1)
    {
        if (initOk) BASS_RecordFree();
        Record::microAvailable = false;
        Logger::LogToFile("[sv:err:record:reopen] : failed to init device (code:%d)", BASS_ErrorGetCode());
        return false;
    }

    Record::usedDeviceIndex = index;

    Record::recordChannel = BASS_RecordStart(SV::kFrequency, 1, active ? NULL : BASS_RECORD_PAUSE, nullptr, nullptr);

    if (Record::recordChannel == NULL)
    {
        BASS_RecordFree();
        Record::usedDeviceIndex = -1;
        Record::microAvailable = false;
        Logger::LogToFile("[sv:err:record:reopen] : failed to start recording (code:%d)", BASS_ErrorGetCode());
        return false;
    }

    BASS_RecordSetInput(-1, BASS_INPUT_ON, static_cast<float>(PluginConfig::GetMicroVolume()) / 100.f);

    Record::voiceDsp.RequestReset();
    Record::microAvailable = true;

    Logger::LogToFile("[sv:dbg:record:reopen] : microphone ready (%s)",
        Record::deviceNamesList[Record::usedDeviceIndex].c_str());

    return true;
}

const std::vector<std::string>& Record::GetDeviceNamesList() noexcept
{
    return Record::deviceNamesList;
}

const std::vector<int>& Record::GetDeviceNumbersList() noexcept
{
    return Record::deviceNumbersList;
}

bool Record::initStatus { false };
bool Record::microAvailable { false };
DWORD Record::nextPollTick { 0 };
int Record::rogerLeft { 0 };

bool Record::checkStatus { false };
bool Record::recordStatus { false };

HRECORD Record::recordChannel { NULL };
OpusEncoder* Record::encoder { nullptr };

std::array<opus_int16, SV::kFrameSizeInSamples> Record::encBuffer {};

HSTREAM Record::checkChannel { NULL };

voice_dsp::Processor Record::voiceDsp;
std::array<float, SV::kFrameSizeInSamples> Record::dspBuffer {};

int Record::usedDeviceIndex { -1 };
std::vector<std::string> Record::deviceNamesList;
std::vector<int> Record::deviceNumbersList;
