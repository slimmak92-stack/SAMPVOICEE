/*
    This is a SampVoice project file
    Developer: CyberMor <cyber.mor.2020@gmail.ru>

    See more here https://github.com/CyberMor/sampvoice

    Copyright (c) Daniel (CyberMor) 2020 All rights reserved
*/

#include "MicroIcon.h"

#include <algorithm>
#include <cmath>

#include <util/Logger.h>
#include <util/GameUtil.h>
#include <util/Render.h>
#include <util/ImGuiUtil.h>
#include <imgui/imgui.h>

#include "PluginConfig.h"
// mic state comes from Plugin.cpp (not included here: Plugin.h pulls WinSock2.h after windows.h)
extern "C" int SvGetMicState();

bool MicroIcon::Init(IDirect3DDevice9* const pDevice, const Resource& rPassiveIcon,
    const Resource& rActiveIcon, const Resource& rMutedIcon) noexcept
{
    if (pDevice == nullptr)
        return false;

    if (MicroIcon::initStatus)
        return false;

    try
    {
        MicroIcon::tPassiveIcon = MakeTexture(pDevice, rPassiveIcon);
        MicroIcon::tActiveIcon = MakeTexture(pDevice, rActiveIcon);
        MicroIcon::tMutedIcon = MakeTexture(pDevice, rMutedIcon);
    }
    catch (const std::exception& exception)
    {
        Logger::LogToFile("[sv:err:microicon:init] : failed to create icons");
        MicroIcon::tPassiveIcon.reset();
        MicroIcon::tActiveIcon.reset();
        MicroIcon::tMutedIcon.reset();
        return false;
    }

    MicroIcon::SetPassiveIcon();

    if (!PluginConfig::IsMicroLoaded())
    {
        PluginConfig::SetMicroLoaded(true);
        MicroIcon::ResetConfigs();
    }

    MicroIcon::initStatus = true;
    MicroIcon::SyncConfigs();

    Logger::LogToFile("[sv:dbg:microicon:init] : icons created");

    return true;
}

void MicroIcon::Free() noexcept
{
    if (!MicroIcon::initStatus)
        return;

    MicroIcon::tPassiveIcon.reset();
    MicroIcon::tActiveIcon.reset();
    MicroIcon::tMutedIcon.reset();

    MicroIcon::initStatus = false;
}

void MicroIcon::Show() noexcept
{
    MicroIcon::showStatus = true;
}

bool MicroIcon::IsShowed() noexcept
{
    return MicroIcon::showStatus;
}

void MicroIcon::Hide() noexcept
{
    MicroIcon::showStatus = false;
}

void MicroIcon::SetPassiveIcon() noexcept
{
    MicroIcon::alphaLevelActiveIcon = 0;
    MicroIcon::alphaLevelDeviationActiveIcon = 0;
    MicroIcon::alphaLevelMutedIcon = 0;
    MicroIcon::alphaLevelDeviationMutedIcon = 0;

    MicroIcon::alphaLevelPassiveIcon = 255;
    MicroIcon::alphaLevelDeviationPassiveIcon = 0;
}

void MicroIcon::SetActiveIcon() noexcept
{
    MicroIcon::alphaLevelPassiveIcon = 0;
    MicroIcon::alphaLevelDeviationPassiveIcon = 0;
    MicroIcon::alphaLevelMutedIcon = 0;
    MicroIcon::alphaLevelDeviationMutedIcon = 0;

    MicroIcon::alphaLevelActiveIcon = 255;
    MicroIcon::alphaLevelDeviationActiveIcon = 0;
}

void MicroIcon::SetMutedIcon() noexcept
{
    MicroIcon::alphaLevelPassiveIcon = 0;
    MicroIcon::alphaLevelDeviationPassiveIcon = 0;
    MicroIcon::alphaLevelActiveIcon = 0;
    MicroIcon::alphaLevelDeviationActiveIcon = 0;

    MicroIcon::alphaLevelMutedIcon = 255;
    MicroIcon::alphaLevelDeviationMutedIcon = 0;
}

void MicroIcon::SwitchToPassiveIcon() noexcept
{
    MicroIcon::alphaLevelDeviationActiveIcon = kAlphaLevelDecrementDeviation;
    MicroIcon::alphaLevelDeviationMutedIcon = kAlphaLevelDecrementDeviation;

    MicroIcon::alphaLevelDeviationPassiveIcon = kAlphaLevelIncrementDeviation;
}

void MicroIcon::SwitchToActiveIcon() noexcept
{
    MicroIcon::alphaLevelDeviationPassiveIcon = kAlphaLevelDecrementDeviation;
    MicroIcon::alphaLevelDeviationMutedIcon = kAlphaLevelDecrementDeviation;

    MicroIcon::alphaLevelDeviationActiveIcon = kAlphaLevelIncrementDeviation;
}

void MicroIcon::SwitchToMutedIcon() noexcept
{
    MicroIcon::alphaLevelDeviationPassiveIcon = kAlphaLevelDecrementDeviation;
    MicroIcon::alphaLevelDeviationActiveIcon = kAlphaLevelDecrementDeviation;

    MicroIcon::alphaLevelDeviationMutedIcon = kAlphaLevelIncrementDeviation;
}

void MicroIcon::Render() noexcept
{
    if (!MicroIcon::initStatus)
        return;

    if (!MicroIcon::showStatus)
        return;

    float vIconSize { 0.f };

    if (!Render::ConvertBaseYValueToScreenYValue(kBaseIconSize, vIconSize))
        return;

    vIconSize *= PluginConfig::GetMicroIconScale();

    float vIconX = PluginConfig::GetMicroIconPositionX();
    float vIconY = PluginConfig::GetMicroIconPositionY();

    if (vIconX < 0.f || vIconY < 0.f)
    {
        CRect radarRect;

        if (!GameUtil::GetRadarRect(radarRect))
            return;

        float vIconPadding { 0.f };

        if (!Render::ConvertBaseYValueToScreenYValue(kBaseIconPadding, vIconPadding))
            return;

        vIconX = radarRect.left + (radarRect.right - radarRect.left) / 2.f;
        vIconY = radarRect.top - (vIconPadding + vIconSize / 2.f);
    }

    vIconX -= vIconSize / 2.f;
    vIconY -= vIconSize / 2.f;

    static bool firstRenderLogged { false };
    if (!firstRenderLogged)
    {
        firstRenderLogged = true;
        Logger::LogToFile("[sv:dbg:microicon:render] : first draw x=%.1f y=%.1f size=%.1f alpha(passive/active/muted)=%d/%d/%d",
            vIconX, vIconY, vIconSize, MicroIcon::alphaLevelPassiveIcon,
            MicroIcon::alphaLevelActiveIcon, MicroIcon::alphaLevelMutedIcon);
    }

    // vector microphone icon (same look as the server_info_widget.lua glyph), no PNG textures
    const int micState = SvGetMicState();
    const bool talking = (micState & 4) != 0;
    const bool muted = (micState & 2) != 0 || (micState & 1) == 0 || (micState & 8) == 0;

    static float talkLevel { 0.f };
    talkLevel += ((talking ? 1.f : 0.f) - talkLevel) * 0.25f;

    // fade when idle: dim after a few quiet seconds, wake up on speech / state change / open menu
    static int lastMicState { -1 };
    static DWORD lastActiveTick { GetTickCount() };
    static float fadeAlpha { 1.f };

    const DWORD nowTick = GetTickCount();
    if (talking || micState != lastMicState || MicroIcon::menuFrameShared)
        lastActiveTick = nowTick;
    lastMicState = micState;

    const bool faded = PluginConfig::GetIconFade() &&
        (nowTick - lastActiveTick) > static_cast<DWORD>(PluginConfig::GetIconFadeDelay()) * 1000u;
    fadeAlpha += ((faded ? 0.15f : 1.f) - fadeAlpha) * 0.12f;

    MicroIcon::pendingAlpha = fadeAlpha;
    MicroIcon::pendingX = vIconX;
    MicroIcon::pendingY = vIconY;
    MicroIcon::pendingSize = vIconSize;
    MicroIcon::pendingMuted = muted;
    MicroIcon::pendingLevel = talkLevel;

    // While the plugin menu is open it owns the ImGui frame: a second NewFrame in the same
    // tick would swallow mouse clicks, so the icon is drawn from inside the menu's frame instead
    if (MicroIcon::menuFrameShared)
    {
        MicroIcon::pendingValid = true;
        return;
    }

    if (!ImGuiUtil::BeginRender())
        return;

    MicroIcon::DrawShapes();

    ImGuiUtil::EndRender();
}

void MicroIcon::RenderOverlay() noexcept
{
    if (!MicroIcon::pendingValid)
        return;

    MicroIcon::pendingValid = false;
    MicroIcon::DrawShapes();
}

void MicroIcon::SetMenuFrameShared(const bool shared) noexcept
{
    MicroIcon::menuFrameShared = shared;
    if (!shared) MicroIcon::pendingValid = false;
}

void MicroIcon::DrawShapes() noexcept
{
    ImDrawList* const dl = ImGui::GetOverlayDrawList();

    const float vIconX = MicroIcon::pendingX;
    const float vIconY = MicroIcon::pendingY;
    const float vIconSize = MicroIcon::pendingSize;
    const bool muted = MicroIcon::pendingMuted;
    const float talkLevel = MicroIcon::pendingLevel;
    const float alphaScale = MicroIcon::pendingAlpha;

    const float S = vIconSize;
    const float cx = vIconX + S / 2.f;
    const float cy = vIconY + S / 2.f;

    // colors come from the menu: idle -> talking while transmitting, muted color while muted / no mic
    const auto channel = [](const D3DCOLOR c, const int shift) { return static_cast<float>((c >> shift) & 0xff); };

    const D3DCOLOR cIdle = PluginConfig::GetIconColorIdle();
    const D3DCOLOR cTalk = PluginConfig::GetIconColorTalk();
    const D3DCOLOR cMuted = PluginConfig::GetIconColorMuted();

    float rgb[3];
    for (int k = 0; k != 3; ++k)
    {
        const int shift = 16 - 8 * k;
        const float from = (muted && talkLevel < 0.01f) ? channel(cMuted, shift) : channel(cIdle, shift);
        const float to = channel(cTalk, shift);
        rgb[k] = (muted && talkLevel < 0.01f) ? from : from + (to - from) * talkLevel;
    }

    const int alpha = static_cast<int>(240.f * alphaScale);
    const ImU32 col = IM_COL32(static_cast<int>(rgb[0]), static_cast<int>(rgb[1]), static_cast<int>(rgb[2]), alpha);
    const float th = 0.075f * S;
    const int style = PluginConfig::GetIconStyle();

    const ImVec2 capMin(cx - 0.17f * S, cy - 0.44f * S);
    const ImVec2 capMax(cx + 0.17f * S, cy + 0.08f * S);

    // capsule: solid (Filled / Minimal) or outlined
    if (style == 1)
        dl->AddRect(capMin, capMax, col, 0.17f * S, ImDrawCornerFlags_All, th);
    else
        dl->AddRectFilled(capMin, capMax, col, 0.17f * S);

    // U shaped holder (Filled / Outline)
    if (style != 2)
    {
        dl->AddLine(ImVec2(cx - 0.29f * S, cy - 0.08f * S), ImVec2(cx - 0.29f * S, cy + 0.02f * S), col, th);
        dl->AddLine(ImVec2(cx + 0.29f * S, cy - 0.08f * S), ImVec2(cx + 0.29f * S, cy + 0.02f * S), col, th);
        dl->PathArcTo(ImVec2(cx, cy + 0.02f * S), 0.29f * S, 0.f, 3.14159265f, 20);
        dl->PathStroke(col, false, th);
    }

    // stem + base
    dl->AddLine(ImVec2(cx, cy + (style == 2 ? 0.14f : 0.31f) * S), ImVec2(cx, cy + 0.43f * S), col, th);
    dl->AddLine(ImVec2(cx - 0.15f * S, cy + 0.43f * S), ImVec2(cx + 0.15f * S, cy + 0.43f * S), col, th);

    // slash when muted
    if (muted)
        dl->AddLine(ImVec2(cx - 0.36f * S, cy - 0.40f * S), ImVec2(cx + 0.36f * S, cy + 0.40f * S), col, th);

    // equalizer bars left of the icon while transmitting
    if (PluginConfig::GetIconBars() && talkLevel > 0.03f)
    {
        const float s = S / kBaseIconSize;
        const float bx0 = vIconX - 30.f * s;
        const float t = static_cast<float>(GetTickCount() % 1000000) / 1000.f;
        const ImU32 barCol = IM_COL32(static_cast<int>(channel(cTalk, 16)), static_cast<int>(channel(cTalk, 8)),
            static_cast<int>(channel(cTalk, 0)), static_cast<int>(230.f * talkLevel * alphaScale));

        for (int i = 0; i < 4; ++i)
        {
            const float bh = (4.f + (0.5f + 0.5f * sinf(t * 9.f + i * 1.7f)) * 13.f) * s * talkLevel;
            const float bx = bx0 + i * 5.5f * s;
            dl->AddRectFilled(ImVec2(bx, cy - bh / 2.f), ImVec2(bx + 3.f * s, cy + bh / 2.f), barCol, 1.5f * s);
        }
    }
}

void MicroIcon::Update() noexcept
{
    if (MicroIcon::alphaLevelDeviationPassiveIcon != 0)
    {
        MicroIcon::alphaLevelPassiveIcon += MicroIcon::alphaLevelDeviationPassiveIcon;

        if (MicroIcon::alphaLevelPassiveIcon < 0)
        {
            MicroIcon::alphaLevelDeviationPassiveIcon = 0;
            MicroIcon::alphaLevelPassiveIcon = 0;
        }
        else if (MicroIcon::alphaLevelPassiveIcon > 255)
        {
            MicroIcon::alphaLevelDeviationPassiveIcon = 0;
            MicroIcon::alphaLevelPassiveIcon = 255;
        }
    }

    if (MicroIcon::alphaLevelDeviationActiveIcon != 0)
    {
        MicroIcon::alphaLevelActiveIcon += MicroIcon::alphaLevelDeviationActiveIcon;

        if (MicroIcon::alphaLevelActiveIcon < 0)
        {
            MicroIcon::alphaLevelDeviationActiveIcon = 0;
            MicroIcon::alphaLevelActiveIcon = 0;
        }
        else if (MicroIcon::alphaLevelActiveIcon > 255)
        {
            MicroIcon::alphaLevelDeviationActiveIcon = 0;
            MicroIcon::alphaLevelActiveIcon = 255;
        }
    }

    if (MicroIcon::alphaLevelDeviationMutedIcon != 0)
    {
        MicroIcon::alphaLevelMutedIcon += MicroIcon::alphaLevelDeviationMutedIcon;

        if (MicroIcon::alphaLevelMutedIcon < 0)
        {
            MicroIcon::alphaLevelDeviationMutedIcon = 0;
            MicroIcon::alphaLevelMutedIcon = 0;
        }
        else if (MicroIcon::alphaLevelMutedIcon > 255)
        {
            MicroIcon::alphaLevelDeviationMutedIcon = 0;
            MicroIcon::alphaLevelMutedIcon = 255;
        }
    }
}

int MicroIcon::GetMicroIconPositionX() noexcept
{
    return PluginConfig::GetMicroIconPositionX();
}

int MicroIcon::GetMicroIconPositionY() noexcept
{
    return PluginConfig::GetMicroIconPositionY();
}

D3DCOLOR MicroIcon::GetMicroIconColor() noexcept
{
    return PluginConfig::GetMicroIconColor();
}

float MicroIcon::GetMicroIconAngle() noexcept
{
    return PluginConfig::GetMicroIconAngle();
}

float MicroIcon::GetMicroIconScale() noexcept
{
    return PluginConfig::GetMicroIconScale();
}

void MicroIcon::SetMicroIconPositionX(const int x) noexcept
{
    PluginConfig::SetMicroIconPositionX(x);
}

void MicroIcon::SetMicroIconPositionY(const int y) noexcept
{
    PluginConfig::SetMicroIconPositionY(y);
}

void MicroIcon::SetMicroIconPosition(const int x, const int y) noexcept
{
    PluginConfig::SetMicroIconPositionX(x);
    PluginConfig::SetMicroIconPositionY(y);
}

void MicroIcon::SetMicroIconColor(const D3DCOLOR color) noexcept
{
    PluginConfig::SetMicroIconColor(color);
}

void MicroIcon::SetMicroIconAngle(const float angle) noexcept
{
    PluginConfig::SetMicroIconAngle(angle);
}

void MicroIcon::SetMicroIconScale(const float scale) noexcept
{
    PluginConfig::SetMicroIconScale(std::clamp(scale, 0.2f, 2.f));
}

void MicroIcon::ResetConfigs() noexcept
{
    PluginConfig::SetMicroIconScale(PluginConfig::kDefValMicroIconScale);
    PluginConfig::SetMicroIconPositionX(PluginConfig::kDefValMicroIconPositionX);
    PluginConfig::SetMicroIconPositionY(PluginConfig::kDefValMicroIconPositionY);
    PluginConfig::SetMicroIconColor(PluginConfig::kDefValMicroIconColor);
    PluginConfig::SetMicroIconAngle(PluginConfig::kDefValMicroIconAngle);
    PluginConfig::SetIconStyle(PluginConfig::kDefValIconStyle);
    PluginConfig::SetIconBars(PluginConfig::kDefValIconBars);
    PluginConfig::SetIconFade(PluginConfig::kDefValIconFade);
    PluginConfig::SetIconFadeDelay(PluginConfig::kDefValIconFadeDelay);
    PluginConfig::SetIconColorIdle(PluginConfig::kDefValIconColorIdle);
    PluginConfig::SetIconColorTalk(PluginConfig::kDefValIconColorTalk);
    PluginConfig::SetIconColorMuted(PluginConfig::kDefValIconColorMuted);
}

void MicroIcon::SyncConfigs() noexcept
{
    MicroIcon::SetMicroIconScale(PluginConfig::GetMicroIconScale());
    MicroIcon::SetMicroIconPositionX(PluginConfig::GetMicroIconPositionX());
    MicroIcon::SetMicroIconPositionY(PluginConfig::GetMicroIconPositionY());
    MicroIcon::SetMicroIconColor(PluginConfig::GetMicroIconColor());
    MicroIcon::SetMicroIconAngle(PluginConfig::GetMicroIconAngle());
}

bool MicroIcon::initStatus { false };
bool MicroIcon::showStatus { false };

int MicroIcon::alphaLevelPassiveIcon { 0 };
int MicroIcon::alphaLevelDeviationPassiveIcon { 0 };
TexturePtr MicroIcon::tPassiveIcon { nullptr };

int MicroIcon::alphaLevelActiveIcon { 0 };
int MicroIcon::alphaLevelDeviationActiveIcon { 0 };
TexturePtr MicroIcon::tActiveIcon { nullptr };

int MicroIcon::alphaLevelMutedIcon { 0 };
int MicroIcon::alphaLevelDeviationMutedIcon { 0 };
TexturePtr MicroIcon::tMutedIcon { nullptr };

bool MicroIcon::menuFrameShared { false };
bool MicroIcon::pendingValid { false };
float MicroIcon::pendingX { 0.f };
float MicroIcon::pendingY { 0.f };
float MicroIcon::pendingSize { 0.f };
bool MicroIcon::pendingMuted { false };
float MicroIcon::pendingLevel { 0.f };
float MicroIcon::pendingAlpha { 1.f };
