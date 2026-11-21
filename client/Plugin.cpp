/*
    This is a SampVoice project file
    Developer: CyberMor <cyber.mor.2020@gmail.ru>

    See more here https://github.com/CyberMor/sampvoice

    Copyright (c) Daniel (CyberMor) 2020 All rights reserved
*/

#include "Plugin.h"

#include <cassert>
#include <cstring>

#include <game/CRadar.h>
#include <game/CWorld.h>
#include <game/CSprite.h>
#include <samp/CScoreboard.h>
#include <util/Path.h>
#include <util/GameUtil.h>
#include <util/ImGuiUtil.h>
#include <util/KeyFilter.h>
#include <util/RakNet.h>
#include <util/Logger.h>
#include <util/Timer.h>

#include "Record.h"
#include "Playback.h"
#include "Network.h"
#include "BlackList.h"
#include "PluginConfig.h"
#include "MicroIcon.h"
#include "SpeakerList.h"
#include "PluginMenu.h"
#include "GlobalStream.h"
#include "StreamAtPoint.h"
#include "StreamAtVehicle.h"
#include "StreamAtPlayer.h"
#include "StreamAtObject.h"
#include "Header.h"

#define FontResource IDR_FONT1, RT_FONT
#define LogoIconResource IDB_PNG1, "PNG"
#define BlurShaderResource IDR_HLSL1, "HLSL"
#define PassiveMicroIconResource IDB_PNG4, "PNG"
#define ActiveMicroIconResource IDB_PNG2, "PNG"
#define MutedMicroIconResource IDB_PNG3, "PNG"
#define SpeakerIconResource IDB_PNG5, "PNG"

bool Plugin::OnPluginLoad(const HMODULE hModule) noexcept
{
    if (hModule == nullptr)
        return false;

    Plugin::pModuleHandle = hModule;

    if (!Logger::Init(Path() / SV::kLogFileName))
        return false;

    if (!Render::Init())
    {
        Logger::LogToFile("[sv:err:plugin] : failed to init render module");
        Logger::Free();
        return false;
    }

    Render::AddDeviceInitCallback(Plugin::OnDeviceInit);
    Render::AddBeforeResetCallback(Plugin::OnBeforeReset);
    Render::AddRenderCallback(Plugin::OnRender);
    Render::AddAfterResetCallback(Plugin::OnAfterReset);
    Render::AddDeviceFreeCallback(Plugin::OnDeviceFree);

    return true;
}

bool Plugin::OnSampLoad(const HMODULE hModule) noexcept
{
    Plugin::pAddresses = MakeAddressesBase(reinterpret_cast<DWORD>(hModule));

    if (!PluginConfig::Load(Path() / SV::kConfigFileName))
    {
        Logger::LogToFile("[sv:err:plugin] : failed to load configs");
    }

    if (!Samp::Init(*Plugin::pAddresses))
    {
        Logger::LogToFile("[sv:err:plugin] : failed to init samp");
        Render::Free();
        Logger::Free();
        return false;
    }

    Samp::AddLoadCallback(Plugin::OnInitGame);
    Samp::AddExitCallback(Plugin::OnExitGame);

    if (!Network::Init(*Plugin::pAddresses))
    {
        Logger::LogToFile("[sv:err:plugin] : failed to init network");
        Samp::Free();
        Render::Free();
        Logger::Free();
        return false;
    }

    Network::AddConnectCallback(Plugin::ConnectHandler);
    Network::AddSvConnectCallback(Plugin::PluginConnectHandler);
    Network::AddSvInitCallback(Plugin::PluginInitHandler);
    Network::AddDisconnectCallback(Plugin::DisconnectHandler);

    if (!Playback::Init(*Plugin::pAddresses))
    {
        Logger::LogToFile("[sv:err:plugin] : failed to init playback");
        Network::Free();
        Samp::Free();
        Render::Free();
        Logger::Free();
        return false;
    }

    return true;
}

void Plugin::OnInitGame() noexcept
{
    Plugin::drawRadarHook = MakeCallHook(0x58FC53, Plugin::DrawRadarHook);

    GameUtil::DisableAntiCheat(*Plugin::pAddresses);

    SpeakerList::Show();
    MicroIcon::Show();
}

void Plugin::OnExitGame() noexcept
{
    Network::Free();

    Plugin::streamTable.clear();

    Record::Free();
    Playback::Free();

    PluginConfig::Save(Path() / SV::kConfigFileName);
    BlackList::Free();

    Render::Free();
    Logger::Free();
}

void Plugin::MainLoop()
{
    if (!Samp::IsLoaded()) return;

    if (Plugin::gameStatus && !GameUtil::IsGameActive())
    {
        Logger::LogToFile("[sv:dbg:plugin] : game paused");

        for (const auto& stream : Plugin::streamTable)
            stream.second->Reset();

        KeyFilter::ReleaseAllKeys();
        Plugin::gameStatus = false;
    }
    else if (!Plugin::gameStatus && GameUtil::IsGameActive())
    {
        Logger::LogToFile("[sv:dbg:plugin] : game resumed");

        for (const auto& stream : Plugin::streamTable)
            stream.second->Reset();

        Plugin::gameStatus = true;
    }

    while (const auto controlPacket = Network::ReceiveControlPacket())
    {
        Plugin::ControlPacketHandler(*&*controlPacket);
    }

    while (const auto voicePacket = Network::ReceiveVoicePacket())
    {
        const auto& voicePacketRef = *voicePacket;

        if (BlackList::IsPlayerBlocked(voicePacketRef->sender))
            continue;

        const auto iter = Plugin::streamTable.find(voicePacketRef->stream);
        if (iter == Plugin::streamTable.end()) continue;

        iter->second->Push(*&voicePacketRef);
        Plugin::voiceTicks[voicePacketRef->stream] = GetTickCount();
    }

    for (const auto& stream : Plugin::streamTable)
        stream.second->Tick();

    Playback::Tick();
    Record::Tick();

    // microphone was missing when the server initialised the plugin: keep trying while connected
    if (Plugin::recordBitrate != 0 && !Record::IsInitialized() && PluginConfig::GetAutoRecover() &&
        static_cast<LONG>(GetTickCount() - Plugin::nextRecordInitTry) >= 0)
    {
        Plugin::nextRecordInitTry = GetTickCount() + 3000;

        if (Record::HasMicro() && Record::Init(Plugin::recordBitrate))
            Logger::LogToFile("[sv:inf:plugin] : microphone found, record module started");
    }

    KeyEvent keyEvent;

    while (KeyFilter::PopEvent(keyEvent))
    {
        if (keyEvent.isPressed)
        {
            Logger::LogToFile("[sv:dbg:plugin] : activation key(%hhx) pressed", keyEvent.keyId);

            if (!Record::GetMicroEnable()) continue;

            if (!Plugin::recordBusy && !Plugin::recordStatus && keyEvent.activeKeys == 1)
            {
                if (!Plugin::muteStatus)
                {
                    Plugin::recordStatus = true;
                    MicroIcon::SwitchToActiveIcon();
                    Record::CancelRoger();
                    Record::StartRecording();
                }
                else
                {
                    MicroIcon::SwitchToMutedIcon();
                }
            }

            if (Plugin::muteStatus) continue;

            SV::PressKeyPacket pressKeyPacket {};

            pressKeyPacket.keyId = keyEvent.keyId;

            if (!Network::SendControlPacket(SV::ControlPacketType::pressKey, &pressKeyPacket, sizeof(pressKeyPacket)))
                Logger::LogToFile("[sv:err:main:HookWndProc] : failed to send PressKey packet");
        }
        else
        {
            Logger::LogToFile("[sv:dbg:plugin] : activation key(%hhx) released", keyEvent.keyId);

            if (!Record::GetMicroEnable()) continue;

            if (!Plugin::recordBusy && Plugin::recordStatus && !keyEvent.activeKeys)
            {
                MicroIcon::SwitchToPassiveIcon();
                Plugin::recordStatus = false;
                Record::ArmRoger();
            }

            if (Plugin::muteStatus) continue;

            SV::ReleaseKeyPacket releaseKeyPacket {};

            releaseKeyPacket.keyId = keyEvent.keyId;

            if (!Network::SendControlPacket(SV::ControlPacketType::releaseKey, &releaseKeyPacket, sizeof(releaseKeyPacket)))
                Logger::LogToFile("[sv:err:main:HookWndProc] : failed to send ReleaseKey packet");
        }
    }

    BYTE frameBuffer[Network::kMaxVoiceDataSize];

    if (const auto frameSize = Record::GetFrame(frameBuffer, sizeof(frameBuffer)))
    {
        if (!Network::SendVoicePacket(frameBuffer, frameSize))
            Logger::LogToFile("[sv:err:plugin] : failed to send voice packet");

        if (!Plugin::recordStatus && !Record::IsRogerPending())
        {
            Record::StopRecording();
            Network::EndSequence();
        }
    }
}

void Plugin::ConnectHandler(const std::string& serverIp, const WORD serverPort)
{
    Plugin::blacklistFilePath = static_cast<const std::string&>(Path() /
        "svblacklist_" + serverIp + "_" + std::to_string(serverPort) + ".txt");

    if (!BlackList::Load(Plugin::blacklistFilePath))
        Logger::LogToFile("[sv:err:plugin] : failed to open blacklist file");
    if (!BlackList::Init(*Plugin::pAddresses))
        Logger::LogToFile("[sv:err:plugin] : failed to init blacklist");
}

void Plugin::PluginConnectHandler(SV::ConnectPacket& connectStruct)
{
    connectStruct.signature = SV::kSignature;
    connectStruct.version = SV::kVersion;
    connectStruct.micro = Record::HasMicro();
}

bool Plugin::PluginInitHandler(const SV::PluginInitPacket& initPacket)
{
    Plugin::muteStatus = initPacket.mute;
    Plugin::recordBitrate = initPacket.bitrate;
    Plugin::nextRecordInitTry = GetTickCount() + 3000;

    if (!Record::Init(initPacket.bitrate))
    {
        Logger::LogToFile("[sv:inf:plugin:packet:init] : failed init record");
    }

    return true;
}

void Plugin::ControlPacketHandler(const ControlPacket& controlPacket)
{
    switch (controlPacket.packet)
    {
        case SV::ControlPacketType::muteEnable:
        {
            if (controlPacket.length != 0) break;

            Logger::LogToFile("[sv:dbg:plugin:muteenable]");

            Plugin::muteStatus = true;
            Plugin::recordStatus = false;
            Plugin::recordBusy = false;

            KeyFilter::ReleaseAllKeys();
        } break;
        case SV::ControlPacketType::muteDisable:
        {
            if (controlPacket.length != 0) break;

            Logger::LogToFile("[sv:dbg:plugin:mutedisable]");

            Plugin::muteStatus = false;
        } break;
        case SV::ControlPacketType::startRecord:
        {
            if (controlPacket.length != 0) break;

            Logger::LogToFile("[sv:dbg:plugin:startrecord]");

            if (Plugin::muteStatus) break;

            Plugin::recordBusy = true;
            Plugin::recordStatus = true;

            Record::StartRecording();
        } break;
        case SV::ControlPacketType::stopRecord:
        {
            if (controlPacket.length != 0) break;

            Logger::LogToFile("[sv:dbg:plugin:stoprecord]");

            if (Plugin::muteStatus) break;

            Plugin::recordStatus = false;
            Plugin::recordBusy = false;
        } break;
        case SV::ControlPacketType::addKey:
        {
            const auto& stData = *reinterpret_cast<const SV::AddKeyPacket*>(controlPacket.data);
            if (controlPacket.length != sizeof(stData)) break;

            Logger::LogToFile("[sv:dbg:plugin:addkey] : keyid(0x%hhx)", stData.keyId);

            KeyFilter::AddKey(stData.keyId);
        } break;
        case SV::ControlPacketType::removeKey:
        {
            const auto& stData = *reinterpret_cast<const SV::RemoveKeyPacket*>(controlPacket.data);
            if (controlPacket.length != sizeof(stData)) break;

            Logger::LogToFile("[sv:dbg:plugin:removekey] : keyid(0x%hhx)", stData.keyId);

            KeyFilter::RemoveKey(stData.keyId);
        } break;
        case SV::ControlPacketType::removeAllKeys:
        {
            if (controlPacket.length) break;

            Logger::LogToFile("[sv:dbg:plugin:removeallkeys]");

            KeyFilter::RemoveAllKeys();
        } break;
        case SV::ControlPacketType::createGStream:
        {
            const auto& stData = *reinterpret_cast<const SV::CreateGStreamPacket*>(controlPacket.data);
            if (controlPacket.length < sizeof(stData)) break;

            Logger::LogToFile("[sv:dbg:plugin:creategstream] : stream(%p), color(0x%x), name(%s)",
                stData.stream, stData.color, stData.color ? stData.name : "");

            const auto& streamPtr = Plugin::streamTable[stData.stream] =
                MakeGlobalStream(stData.color, stData.name);

            streamPtr->AddPlayCallback(SpeakerList::OnSpeakerPlay);
            streamPtr->AddStopCallback(SpeakerList::OnSpeakerStop);
        } break;
        case SV::ControlPacketType::createLPStream:
        {
            const auto& stData = *reinterpret_cast<const SV::CreateLPStreamPacket*>(controlPacket.data);
            if (controlPacket.length < sizeof(stData)) break;

            Logger::LogToFile("[sv:dbg:plugin:createlpstream] : "
                "stream(%p), dist(%.2f), pos(%.2f;%.2f;%.2f), color(0x%x), name(%s)",
                stData.stream, stData.distance, stData.position.x, stData.position.y, stData.position.z,
                stData.color, stData.color ? stData.name : "");

            const auto& streamPtr = Plugin::streamTable[stData.stream] =
                MakeStreamAtPoint(stData.color, stData.name, stData.distance, stData.position);

            streamPtr->AddPlayCallback(SpeakerList::OnSpeakerPlay);
            streamPtr->AddStopCallback(SpeakerList::OnSpeakerStop);
        } break;
        case SV::ControlPacketType::createLStreamAtVehicle:
        {
            const auto& stData = *reinterpret_cast<const SV::CreateLStreamAtPacket*>(controlPacket.data);
            if (controlPacket.length < sizeof(stData)) break;

            Logger::LogToFile("[sv:dbg:plugin:createlstreamatvehicle] : "
                "stream(%p), dist(%.2f), vehicle(%hu), color(0x%x), name(%s)",
                stData.stream, stData.distance, stData.target,
                stData.color, stData.color ? stData.name : "");

            const auto& streamPtr = Plugin::streamTable[stData.stream] =
                MakeStreamAtVehicle(stData.color, stData.name, stData.distance, stData.target);

            streamPtr->AddPlayCallback(SpeakerList::OnSpeakerPlay);
            streamPtr->AddStopCallback(SpeakerList::OnSpeakerStop);
        } break;
        case SV::ControlPacketType::createLStreamAtPlayer:
        {
            const auto& stData = *reinterpret_cast<const SV::CreateLStreamAtPacket*>(controlPacket.data);
            if (controlPacket.length < sizeof(stData)) break;

            Logger::LogToFile("[sv:dbg:plugin:createlstreamatplayer] : "
                "stream(%p), dist(%.2f), player(%hu), color(0x%x), name(%s)",
                stData.stream, stData.distance, stData.target,
                stData.color, stData.color ? stData.name : "");

            const auto& streamPtr = Plugin::streamTable[stData.stream] =
                MakeStreamAtPlayer(stData.color, stData.name, stData.distance, stData.target);

            streamPtr->AddPlayCallback(SpeakerList::OnSpeakerPlay);
            streamPtr->AddStopCallback(SpeakerList::OnSpeakerStop);
        } break;
        case SV::ControlPacketType::createLStreamAtObject:
        {
            const auto& stData = *reinterpret_cast<const SV::CreateLStreamAtPacket*>(controlPacket.data);
            if (controlPacket.length < sizeof(stData)) break;

            Logger::LogToFile("[sv:dbg:plugin:createlstreamatobject] : "
                "stream(%p), dist(%.2f), object(%hu), color(0x%x), name(%s)",
                stData.stream, stData.distance, stData.target,
                stData.color, stData.color ? stData.name : "");

            const auto& streamPtr = Plugin::streamTable[stData.stream] =
                MakeStreamAtObject(stData.color, stData.name, stData.distance, stData.target);

            streamPtr->AddPlayCallback(SpeakerList::OnSpeakerPlay);
            streamPtr->AddStopCallback(SpeakerList::OnSpeakerStop);
        } break;
        case SV::ControlPacketType::updateLStreamDistance:
        {
            const auto& stData = *reinterpret_cast<const SV::UpdateLStreamDistancePacket*>(controlPacket.data);
            if (controlPacket.length != sizeof(stData)) break;

            Logger::LogToFile("[sv:dbg:plugin:updatelpstreamdistance] : stream(%p), dist(%.2f)",
                stData.stream, stData.distance);

            const auto iter = Plugin::streamTable.find(stData.stream);
            if (iter == Plugin::streamTable.end()) break;

            static_cast<LocalStream*>(iter->second.get())->SetDistance(stData.distance);
        } break;
        case SV::ControlPacketType::updateLPStreamPosition:
        {
            const auto& stData = *reinterpret_cast<const SV::UpdateLPStreamPositionPacket*>(controlPacket.data);
            if (controlPacket.length != sizeof(stData)) break;

            Logger::LogToFile("[sv:dbg:plugin:updatelpstreamcoords] : stream(%p), pos(%.2f;%.2f;%.2f)",
                stData.stream, stData.position.x, stData.position.y, stData.position.z);

            const auto iter = Plugin::streamTable.find(stData.stream);
            if (iter == Plugin::streamTable.end()) break;

            static_cast<StreamAtPoint*>(iter->second.get())->SetPosition(stData.position);
        } break;
        case SV::ControlPacketType::deleteStream:
        {
            const auto& stData = *reinterpret_cast<const SV::DeleteStreamPacket*>(controlPacket.data);
            if (controlPacket.length != sizeof(stData)) break;

            Logger::LogToFile("[sv:dbg:plugin:deletestream] : stream(%p)", stData.stream);

            Plugin::streamTable.erase(stData.stream);
        } break;
        case SV::ControlPacketType::setStreamParameter:
        {
            const auto& stData = *reinterpret_cast<const SV::SetStreamParameterPacket*>(controlPacket.data);
            if (controlPacket.length != sizeof(stData)) break;

            Logger::LogToFile("[sv:dbg:plugin:streamsetparameter] : stream(%p), parameter(%hhu), value(%.2f)",
                stData.stream, stData.parameter, stData.value);

            const auto iter = Plugin::streamTable.find(stData.stream);
            if (iter == Plugin::streamTable.end()) break;

            iter->second->SetParameter(stData.parameter, stData.value);
        } break;
        case SV::ControlPacketType::slideStreamParameter:
        {
            const auto& stData = *reinterpret_cast<const SV::SlideStreamParameterPacket*>(controlPacket.data);
            if (controlPacket.length != sizeof(stData)) break;

            Logger::LogToFile("[sv:dbg:plugin:streamslideparameter] : "
                "stream(%p), parameter(%hhu), startvalue(%.2f), endvalue(%.2f), time(%u)",
                stData.stream, stData.parameter, stData.startvalue, stData.endvalue, stData.time);

            const auto iter = Plugin::streamTable.find(stData.stream);
            if (iter == Plugin::streamTable.end()) break;

            iter->second->SlideParameter(stData.parameter, stData.startvalue, stData.endvalue, stData.time);
        } break;
        case SV::ControlPacketType::createEffect:
        {
            const auto& stData = *reinterpret_cast<const SV::CreateEffectPacket*>(controlPacket.data);
            if (controlPacket.length < sizeof(stData)) break;

            Logger::LogToFile("[sv:dbg:plugin:effectcreate] : "
                "stream(%p), effect(%p), number(%hhu), priority(%d)",
                stData.stream, stData.effect, stData.number, stData.priority);

            const auto iter = Plugin::streamTable.find(stData.stream);
            if (iter == Plugin::streamTable.end()) break;

            iter->second->EffectCreate(stData.effect, stData.number, stData.priority,
                stData.params, controlPacket.length - sizeof(stData));
        } break;
        case SV::ControlPacketType::deleteEffect:
        {
            const auto& stData = *reinterpret_cast<const SV::DeleteEffectPacket*>(controlPacket.data);
            if (controlPacket.length != sizeof(stData)) break;

            Logger::LogToFile("[sv:dbg:plugin:effectdelete] : stream(%p), effect(%p)",
                stData.stream, stData.effect);

            const auto iter = Plugin::streamTable.find(stData.stream);
            if (iter == Plugin::streamTable.end()) break;

            iter->second->EffectDelete(stData.effect);
        } break;
    }
}

void Plugin::DisconnectHandler()
{
    Plugin::streamTable.clear();
    Plugin::voiceTicks.clear();
    Plugin::recordBitrate = 0;

    Plugin::muteStatus = false;
    Plugin::recordStatus = false;
    Plugin::recordBusy = false;

    if (!BlackList::Save(Plugin::blacklistFilePath))
        Logger::LogToFile("[sv:err:plugin] : failed to save blacklist file");

    BlackList::Free();
    Record::Free();
}

LRESULT CALLBACK Plugin::WindowProc(const HWND hWnd,
    const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    if (PluginMenu::WindowProc(hWnd, uMsg, wParam, lParam))
        return TRUE;

    switch (uMsg)
    {
        case WM_KEYDOWN: KeyFilter::PushPressEvent(static_cast<BYTE>(wParam)); break;
        case WM_KEYUP: KeyFilter::PushReleaseEvent(static_cast<BYTE>(wParam)); break;
    }

    return CallWindowProc(reinterpret_cast<WNDPROC>
        (Plugin::origWndProc), hWnd, uMsg, wParam, lParam);
}

void Plugin::OnDeviceInit(IDirect3D9* const pDirect,
                          IDirect3DDevice9* const pDevice,
                          const D3DPRESENT_PARAMETERS& dParameters)
{
    assert(pDirect);
    assert(pDevice);

    MicroIcon::Init(pDevice,
        Resource(Plugin::pModuleHandle, PassiveMicroIconResource),
        Resource(Plugin::pModuleHandle, ActiveMicroIconResource),
        Resource(Plugin::pModuleHandle, MutedMicroIconResource));

    ImGuiUtil::Init(pDevice);

    SpeakerList::Init(pDevice, *Plugin::pAddresses,
        Resource(Plugin::pModuleHandle, SpeakerIconResource),
        Resource(Plugin::pModuleHandle, FontResource));

    PluginMenu::Init(pDevice, *Plugin::pAddresses,
        Resource(Plugin::pModuleHandle, BlurShaderResource),
        Resource(Plugin::pModuleHandle, LogoIconResource),
        Resource(Plugin::pModuleHandle, FontResource));

    Plugin::origWndHandle = dParameters.hDeviceWindow;
    Plugin::origWndProc = GetWindowLong(Plugin::origWndHandle, GWL_WNDPROC);
    SetWindowLong(Plugin::origWndHandle, GWL_WNDPROC,
        reinterpret_cast<LONG>(&Plugin::WindowProc));

    Plugin::gameStatus = GameUtil::IsGameActive();
}

void Plugin::OnBeforeReset()
{
    PluginMenu::Free();
    SpeakerList::Free();
    ImGuiUtil::Free();
    MicroIcon::Free();
}

void Plugin::OnRender()
{
    Timer::Tick();
    Plugin::MainLoop();
    MicroIcon::Update();
    PluginMenu::Update();

    // Drawn here (every frame) instead of from the radar hook: servers/HUD mods that hide the
    // game radar stop that hook from running, and the icon would never appear
    if (!GameUtil::IsMenuActive())
        MicroIcon::Render();

    PluginMenu::Render();
}

void Plugin::OnAfterReset(IDirect3DDevice9* const pDevice,
                          const D3DPRESENT_PARAMETERS& dParameters)
{
    assert(pDevice);

    MicroIcon::Init(pDevice,
        Resource(Plugin::pModuleHandle, PassiveMicroIconResource),
        Resource(Plugin::pModuleHandle, ActiveMicroIconResource),
        Resource(Plugin::pModuleHandle, MutedMicroIconResource));

    ImGuiUtil::Init(pDevice);

    SpeakerList::Init(pDevice, *Plugin::pAddresses,
        Resource(Plugin::pModuleHandle, SpeakerIconResource),
        Resource(Plugin::pModuleHandle, FontResource));

    PluginMenu::Init(pDevice, *Plugin::pAddresses,
        Resource(Plugin::pModuleHandle, BlurShaderResource),
        Resource(Plugin::pModuleHandle, LogoIconResource),
        Resource(Plugin::pModuleHandle, FontResource));

    Plugin::gameStatus = GameUtil::IsGameActive();
}

void Plugin::OnDeviceFree()
{
    SetWindowLong(Plugin::origWndHandle, GWL_WNDPROC, Plugin::origWndProc);

    PluginMenu::Free();
    SpeakerList::Free();
    ImGuiUtil::Free();
    MicroIcon::Free();
}

void Plugin::DrawRadarHook()
{
    static_cast<void(*)()>(Plugin::drawRadarHook->callFuncAddr)();

    SpeakerList::Render();
}

HMODULE Plugin::pModuleHandle { NULL };
AddressesBasePtr Plugin::pAddresses { nullptr };

bool Plugin::muteStatus { false };
bool Plugin::recordStatus { false };
bool Plugin::recordBusy { false };

std::map<DWORD, StreamPtr> Plugin::streamTable;
std::map<DWORD, DWORD> Plugin::voiceTicks;
DWORD Plugin::recordBitrate { 0 };
DWORD Plugin::nextRecordInitTry { 0 };
std::string Plugin::blacklistFilePath;
bool Plugin::gameStatus { false };

LONG Plugin::origWndProc { NULL };
HWND Plugin::origWndHandle { NULL };

Memory::CallHookPtr Plugin::drawRadarHook { nullptr };


// ---- exported API for Lua / other plugins (used by server_info_widget.lua) ----
// bit0 (1)  : microphone enabled in plugin settings
// bit1 (2)  : player muted by the server
// bit2 (4)  : currently transmitting (activation key held / server forced)
// bit3 (8)  : a microphone device is present
// bit4 (16) : transmission forced by the server (SvStartRecord)
int Plugin::GetExportedMicState() noexcept
{
    int state = 0;
    if (Record::GetMicroEnable()) state |= 1;
    if (Plugin::muteStatus)       state |= 2;
    if (Plugin::recordStatus)     state |= 4;
    if (Record::IsMicroAvailable()) state |= 8;
    if (Plugin::recordBusy)       state |= 16;
    return state;
}

extern "C" __declspec(dllexport) int SvGetMicState()
{
    return Plugin::GetExportedMicState();
}

// Name of the voice channel the player is currently in (as set by the server through SvCreate*Stream):
// the stream that received voice most recently, else the first global stream, else any named stream.
// Returns the length written (0 = none). Used by server_info_widget.lua.
int Plugin::GetExportedChannelName(char* const buffer, const int size) noexcept
{
    if (buffer == nullptr || size <= 0)
        return 0;

    buffer[0] = '\0';

    try
    {
        const DWORD now = GetTickCount();

        const std::string* best { nullptr };
        const std::string* firstGlobal { nullptr };
        const std::string* firstNamed { nullptr };
        DWORD bestTick { 0 };

        for (const auto& stream : Plugin::streamTable)
        {
            if (!stream.second) continue;

            const auto& info = stream.second->GetInfo();
            const std::string& name = info.GetName();

            if (name.empty()) continue;

            if (firstNamed == nullptr) firstNamed = &name;
            if (firstGlobal == nullptr && info.GetType() == StreamType::GlobalStream) firstGlobal = &name;

            if (const auto tick = Plugin::voiceTicks.find(stream.first);
                tick != Plugin::voiceTicks.end() && now - tick->second < 8000 &&
                (best == nullptr || tick->second > bestTick))
            {
                best = &name;
                bestTick = tick->second;
            }
        }

        const std::string* pick = best != nullptr ? best : (firstGlobal != nullptr ? firstGlobal : firstNamed);
        if (pick == nullptr) return 0;

        const int length = static_cast<int>(pick->size()) < size - 1 ? static_cast<int>(pick->size()) : size - 1;
        std::memcpy(buffer, pick->data(), static_cast<size_t>(length));
        buffer[length] = '\0';

        return length;
    }
    catch (...)
    {
        return 0;
    }
}

extern "C" __declspec(dllexport) int SvGetChannelName(char* buffer, int size)
{
    return Plugin::GetExportedChannelName(buffer, size);
}
