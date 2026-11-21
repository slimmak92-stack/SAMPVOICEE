/*
    This is a SampVoice project file
    Developer: CyberMor <cyber.mor.2020@gmail.ru>

    See more here https://github.com/CyberMor/sampvoice

    Copyright (c) Daniel (CyberMor) 2020 All rights reserved
*/

#pragma once

#include <array>
#include <vector>
#include <string>

#include <audio/bass.h>
#include <audio/opus.h>

#include "Header.h"
#include "VoiceDsp.h"

class Record {

    Record() = delete;
    ~Record() = delete;
    Record(const Record&) = delete;
    Record(Record&&) = delete;
    Record& operator=(const Record&) = delete;
    Record& operator=(Record&&) = delete;

public:

    static bool Init(DWORD bitrate) noexcept;
    static void Free() noexcept;

    static void Tick() noexcept;
    static bool HasMicro() noexcept;
    static bool IsInitialized() noexcept;
    static bool IsMicroAvailable() noexcept;   // cached, refreshed by the device poll

    // roger beep: a short tone appended after the last voice frame of a transmission
    static void ArmRoger() noexcept;
    static void CancelRoger() noexcept;
    static bool IsRogerPending() noexcept;

    static bool GetGateEnable() noexcept;
    static int GetGateSensitivity() noexcept;
    static bool GetAutoGain() noexcept;
    static bool GetRogerBeep() noexcept;
    static bool GetAutoRecover() noexcept;
    static bool GetLowCut() noexcept;
    static bool GetCompressor() noexcept;
    static int GetCustomPitch() noexcept;
    static float GetInputLevel() noexcept;
    static float GetOutputLevel() noexcept;
    static void SetGateEnable(bool enable) noexcept;
    static void SetGateSensitivity(int sensitivity) noexcept;
    static void SetAutoGain(bool enable) noexcept;
    static void SetRogerBeep(bool enable) noexcept;
    static void SetAutoRecover(bool enable) noexcept;
    static void SetLowCut(bool enable) noexcept;
    static void SetCompressor(bool enable) noexcept;
    static void SetCustomPitch(int semitones) noexcept;

    static bool StartRecording() noexcept;
    static bool IsRecording() noexcept;
    static void StopRecording() noexcept;

    static bool StartChecking() noexcept;
    static bool IsChecking() noexcept;
    static void StopChecking() noexcept;

    static DWORD GetFrame(BYTE* bufferPtr, DWORD bufferSize) noexcept;

    static bool GetMicroEnable() noexcept;
    static int GetMicroVolume() noexcept;
    static int GetMicroDevice() noexcept;
    static int GetVoiceEffect() noexcept;

    static void SetMicroEnable(bool microEnable) noexcept;
    static void SetMicroVolume(int microVolume) noexcept;
    static void SetMicroDevice(int deviceIndex) noexcept;
    static void SetVoiceEffect(int voiceEffect) noexcept;

    static void SyncConfigs() noexcept;
    static void ResetConfigs() noexcept;

    static const std::vector<std::string>& GetDeviceNamesList() noexcept;
    static const std::vector<int>& GetDeviceNumbersList() noexcept;

private:

    static void ApplyDspConfig() noexcept;
    static void PollDevices() noexcept;
    static bool ReopenDevice() noexcept;
    static void FillRogerBeep(opus_int16* samples, size_t count) noexcept;

    static bool microAvailable;
    static DWORD nextPollTick;
    static int rogerLeft;

    static bool initStatus;

    static bool checkStatus;
    static bool recordStatus;

    static HRECORD recordChannel;
    static OpusEncoder* encoder;
    static std::array<opus_int16, SV::kFrameSizeInSamples> encBuffer;
    static HSTREAM checkChannel;

    static voice_dsp::Processor voiceDsp;
    static std::array<float, SV::kFrameSizeInSamples> dspBuffer;
    static void ApplyVoiceDsp(opus_int16* samples, size_t count) noexcept;

    static int usedDeviceIndex;
    static std::vector<std::string> deviceNamesList;
    static std::vector<int> deviceNumbersList;

};
