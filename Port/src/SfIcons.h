#pragma once
#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <cstdint>
#include "Theme.h"

// The port's icon pipeline: Apple's real SF Symbols outlines, rasterised once
// offline into Reference/SFSymbols/SfSymbolAtlas.h, uploaded here as one small
// texture per symbol. Tinting happens at draw time (see Symbols.h).
#include "../../Reference/SFSymbols/SfSymbolAtlas.h"

namespace sw {

enum class SfIcon {
    Gear, Figure, Bolt, HalfDisc, Camera, Phone, Magnifier, Waveform,
    Display, Person, AppleLogo, Chevron, Mic,
    Refresh, Wifi, Bell, Lock, PersonFill, Speaker, Airplane, HandRaised,
    Eye, TextFormat, Keyboard, ChevronLeft, MagnifierCircle, ArrowClockwise,
    Moon, RectanglePortrait,
    GearOutline, CameraOutline, PhoneOutline, Accessibility, ArrowTurnRightUp,
    ArrowRightToLine, ArrowUturnRight, ButtonProgrammable,
    Count
};

struct SfTexture {
    ID3D11ShaderResourceView* srv = nullptr;
    int w = 0, h = 0;
};

class SfIcons {
public:
    bool Init(ID3D11Device* device) {
        bool ok = true;
        ok &= Upload(device, SfIcon::Gear, sfsym::kGearshapeFillRGBA, sfsym::kGearshapeFillW, sfsym::kGearshapeFillH);
        ok &= Upload(device, SfIcon::Figure, sfsym::kFigureArmsOpenRGBA, sfsym::kFigureArmsOpenW, sfsym::kFigureArmsOpenH);
        ok &= Upload(device, SfIcon::Bolt, sfsym::kBoltFillRGBA, sfsym::kBoltFillW, sfsym::kBoltFillH);
        ok &= Upload(device, SfIcon::HalfDisc, sfsym::kCircleLefthalfFilledRGBA, sfsym::kCircleLefthalfFilledW, sfsym::kCircleLefthalfFilledH);
        ok &= Upload(device, SfIcon::Camera, sfsym::kCameraFillRGBA, sfsym::kCameraFillW, sfsym::kCameraFillH);
        ok &= Upload(device, SfIcon::Phone, sfsym::kIphoneGen1RGBA, sfsym::kIphoneGen1W, sfsym::kIphoneGen1H);
        ok &= Upload(device, SfIcon::Magnifier, sfsym::kMagnifyingglassRGBA, sfsym::kMagnifyingglassW, sfsym::kMagnifyingglassH);
        ok &= Upload(device, SfIcon::Waveform, sfsym::kWaveformRGBA, sfsym::kWaveformW, sfsym::kWaveformH);
        ok &= Upload(device, SfIcon::Display, sfsym::kDisplayRGBA, sfsym::kDisplayW, sfsym::kDisplayH);
        ok &= Upload(device, SfIcon::Person, sfsym::kPersonCropCircleFillRGBA, sfsym::kPersonCropCircleFillW, sfsym::kPersonCropCircleFillH);
        ok &= Upload(device, SfIcon::AppleLogo, sfsym::kAppleLogoRGBA, sfsym::kAppleLogoW, sfsym::kAppleLogoH);
        ok &= Upload(device, SfIcon::Chevron, sfsym::kChevronForwardRGBA, sfsym::kChevronForwardW, sfsym::kChevronForwardH);
        ok &= Upload(device, SfIcon::Mic, sfsym::kMicFillRGBA, sfsym::kMicFillW, sfsym::kMicFillH);
        ok &= Upload(device, SfIcon::Refresh, sfsym::kRefreshRGBA, sfsym::kRefreshW, sfsym::kRefreshH);
        ok &= Upload(device, SfIcon::Wifi, sfsym::kWifiRGBA, sfsym::kWifiW, sfsym::kWifiH);
        ok &= Upload(device, SfIcon::Bell, sfsym::kBellFillRGBA, sfsym::kBellFillW, sfsym::kBellFillH);
        ok &= Upload(device, SfIcon::Lock, sfsym::kLockFillRGBA, sfsym::kLockFillW, sfsym::kLockFillH);
        ok &= Upload(device, SfIcon::PersonFill, sfsym::kPersonFillRGBA, sfsym::kPersonFillW, sfsym::kPersonFillH);
        ok &= Upload(device, SfIcon::Speaker, sfsym::kSpeakerFillRGBA, sfsym::kSpeakerFillW, sfsym::kSpeakerFillH);
        ok &= Upload(device, SfIcon::Airplane, sfsym::kAirplaneRGBA, sfsym::kAirplaneW, sfsym::kAirplaneH);
        ok &= Upload(device, SfIcon::HandRaised, sfsym::kHandRaisedFillRGBA, sfsym::kHandRaisedFillW, sfsym::kHandRaisedFillH);
        ok &= Upload(device, SfIcon::Eye, sfsym::kEyeFillRGBA, sfsym::kEyeFillW, sfsym::kEyeFillH);
        ok &= Upload(device, SfIcon::TextFormat, sfsym::kTextFormatSizeRGBA, sfsym::kTextFormatSizeW, sfsym::kTextFormatSizeH);
        ok &= Upload(device, SfIcon::Keyboard, sfsym::kKeyboardRGBA, sfsym::kKeyboardW, sfsym::kKeyboardH);
        ok &= Upload(device, SfIcon::ChevronLeft, sfsym::kChevronLeftRGBA, sfsym::kChevronLeftW, sfsym::kChevronLeftH);
        ok &= Upload(device, SfIcon::MagnifierCircle, sfsym::kMagnifierCircleRGBA, sfsym::kMagnifierCircleW, sfsym::kMagnifierCircleH);
        ok &= Upload(device, SfIcon::ArrowClockwise, sfsym::kArrowClockwiseRGBA, sfsym::kArrowClockwiseW, sfsym::kArrowClockwiseH);
        ok &= Upload(device, SfIcon::Moon, sfsym::kMoonFillRGBA, sfsym::kMoonFillW, sfsym::kMoonFillH);
        ok &= Upload(device, SfIcon::RectanglePortrait, sfsym::kRectanglePortraitRGBA, sfsym::kRectanglePortraitW, sfsym::kRectanglePortraitH);
        ok &= Upload(device, SfIcon::GearOutline, sfsym::kGearshapeRGBA, sfsym::kGearshapeW, sfsym::kGearshapeH);
        ok &= Upload(device, SfIcon::CameraOutline, sfsym::kCameraRGBA, sfsym::kCameraW, sfsym::kCameraH);
        ok &= Upload(device, SfIcon::PhoneOutline, sfsym::kIphoneRGBA, sfsym::kIphoneW, sfsym::kIphoneH);
        ok &= Upload(device, SfIcon::Accessibility, sfsym::kAccessibilityRGBA, sfsym::kAccessibilityW, sfsym::kAccessibilityH);
        ok &= Upload(device, SfIcon::ArrowTurnRightUp, sfsym::kArrowTurnRightUpRGBA, sfsym::kArrowTurnRightUpW, sfsym::kArrowTurnRightUpH);
        ok &= Upload(device, SfIcon::ArrowRightToLine, sfsym::kArrowRightToLineRGBA, sfsym::kArrowRightToLineW, sfsym::kArrowRightToLineH);
        ok &= Upload(device, SfIcon::ArrowUturnRight, sfsym::kArrowUturnRightRGBA, sfsym::kArrowUturnRightW, sfsym::kArrowUturnRightH);
        ok &= Upload(device, SfIcon::ButtonProgrammable, sfsym::kButtonProgrammableRGBA, sfsym::kButtonProgrammableW, sfsym::kButtonProgrammableH);
        ready = ok;
        return ok;
    }

    void Shutdown() {
        for (auto& t : tex) {
            if (t.srv) { t.srv->Release(); t.srv = nullptr; }
        }
        ready = false;
    }

    const SfTexture& Get(SfIcon id) const { return tex[(int)id]; }
    bool Ready() const { return ready; }

private:
    SfTexture tex[(int)SfIcon::Count];
    bool ready = false;

    bool Upload(ID3D11Device* device, SfIcon id, const unsigned char* rgba, int w, int h) {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = (UINT)w;
        desc.Height = (UINT)h;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA init = {};
        init.pSysMem = rgba;
        init.SysMemPitch = (UINT)(w * 4);

        ID3D11Texture2D* texture = nullptr;
        if (FAILED(device->CreateTexture2D(&desc, &init, &texture))) return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        HRESULT hr = device->CreateShaderResourceView(texture, &srvDesc, &tex[(int)id].srv);
        texture->Release();
        if (FAILED(hr)) return false;
        tex[(int)id].w = w;
        tex[(int)id].h = h;
        return true;
    }
};

} // namespace sw
