#pragma once
#include <Windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <imgui.h>
#include <vector>
#include "Ui.h"
#include "SfTileMasks.h"

#pragma comment(lib, "windowscodecs.lib")

namespace sw {

using sftile::Glyph;

// ---------------------------------------------------------------------------
// Every small glyph the Settings page draws, straight from the runtime capture.
//
// The port could keep approximating them from the scraped SF Symbols set and its
// own ink boxes, but four of the nine row icons are iOS 27 redesigns that set does
// not contain, and the spotlight / search / account marks were hand-drawn. These
// are the runtime's own pixels — anti-aliasing included — stored as 8-bit coverage
// masks with the colour the runtime draws them in (Siri's orb is a colour image,
// since it is a rendered sphere and not a glyph at all).
//
// The masks are 3x the point size (84 px for a 28 pt tile), so at the port's 3x
// frame they land 1:1 with no resampling.
// ---------------------------------------------------------------------------
class SfMasks {
public:
    bool Init(ID3D11Device* dev) {
        device = dev;
        bool ok = true;
        for (const Entry& e : kEntries) ok &= Load(e);
        ready = ok;
        return ok;
    }

    bool Ready() const { return ready; }

    // Draw glyph `g` into `dst` (points). `offset` is the page translation.
    void Draw(ImDrawList* dl, Glyph g, const Rect& dst, float offset,
              const int* overrideRgb = nullptr) const {
        if (!dl) return;
        const int i = IndexOf(g);
        if (i < 0 || !tex[i].srv) return;
        const Entry& e = kEntries[i];
        // The per-glyph tint is measured from the light capture. In dark mode the same
        // glyph is often inverted (the search field's mic is white on a dark pill, not
        // black), so a caller may override the channel triple.
        const ImU32 tint = overrideRgb
            ? IM_COL32(overrideRgb[0], overrideRgb[1], overrideRgb[2], 255)
            : IM_COL32(e.r, e.g, e.b, 255);
        dl->AddImage((ImTextureID)tex[i].srv, ImVec2(dst.x + offset, dst.y),
                     ImVec2(dst.x + offset + dst.w, dst.y + dst.h),
                     ImVec2(0, 0), ImVec2(1, 1), tint);
    }

    void Release() {
        for (Tex& t : tex) {
            if (t.srv) { t.srv->Release(); t.srv = nullptr; }
            if (t.tex) { t.tex->Release(); t.tex = nullptr; }
        }
        device = nullptr;
        ready = false;
    }

private:
    struct Entry {
        Glyph id;
        const unsigned char* png;
        unsigned int size;
        bool colour;
        unsigned char r, g, b;
    };

    static const Entry kEntries[14];

    static int IndexOf(Glyph g) {
        for (int i = 0; i < 14; ++i) if (kEntries[i].id == g) return i;
        return -1;
    }

    struct Tex { ID3D11Texture2D* tex = nullptr; ID3D11ShaderResourceView* srv = nullptr; };
    Tex tex[14];
    ID3D11Device* device = nullptr;
    bool ready = false;

    // WIC decode from memory. Colour entries keep their pixels (BGRA -> RGBA);
    // masks become white with the coverage in alpha.
    static bool Decode(const unsigned char* png, unsigned int size, bool colour,
                       std::vector<unsigned char>& rgba, int& w, int& h) {
        IWICImagingFactory* factory = nullptr;
        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&factory)))) return false;
        IWICStream* stream = nullptr;
        IWICBitmapDecoder* dec = nullptr;
        IWICBitmapFrameDecode* frame = nullptr;
        IWICFormatConverter* conv = nullptr;
        bool ok = false;
        if (SUCCEEDED(factory->CreateStream(&stream)) &&
            SUCCEEDED(stream->InitializeFromMemory(const_cast<BYTE*>(png), size)) &&
            SUCCEEDED(factory->CreateDecoderFromStream(stream, nullptr,
                        WICDecodeMetadataCacheOnLoad, &dec)) &&
            SUCCEEDED(dec->GetFrame(0, &frame)) &&
            SUCCEEDED(factory->CreateFormatConverter(&conv)) &&
            SUCCEEDED(conv->Initialize(frame, GUID_WICPixelFormat32bppBGRA,
                        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom))) {
            UINT uw = 0, uh = 0;
            if (SUCCEEDED(conv->GetSize(&uw, &uh)) && uw && uh) {
                w = (int)uw;
                h = (int)uh;
                std::vector<unsigned char> bgra((size_t)w * h * 4);
                if (SUCCEEDED(conv->CopyPixels(nullptr, (UINT)w * 4, (UINT)bgra.size(), bgra.data()))) {
                    rgba.resize(bgra.size());
                    for (size_t i = 0; i < bgra.size(); i += 4) {
                        if (colour) {
                            rgba[i + 0] = bgra[i + 2];
                            rgba[i + 1] = bgra[i + 1];
                            rgba[i + 2] = bgra[i + 0];
                            rgba[i + 3] = bgra[i + 3];
                        } else {
                            rgba[i + 0] = 255;
                            rgba[i + 1] = 255;
                            rgba[i + 2] = 255;
                            rgba[i + 3] = bgra[i + 2];   // grey lands in B after the convert
                        }
                    }
                    ok = true;
                }
            }
        }
        if (conv) conv->Release();
        if (frame) frame->Release();
        if (dec) dec->Release();
        if (stream) stream->Release();
        if (factory) factory->Release();
        return ok;
    }

    bool Load(const Entry& e) {
        const int slot = IndexOf(e.id);
        if (slot < 0) return false;
        std::vector<unsigned char> rgba;
        int w = 0, h = 0;
        if (!Decode(e.png, e.size, e.colour, rgba, w, h)) return false;
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = (UINT)w;
        td.Height = (UINT)h;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA sd = {};
        sd.pSysMem = rgba.data();
        sd.SysMemPitch = (UINT)w * 4;
        if (FAILED(device->CreateTexture2D(&td, &sd, &tex[slot].tex))) return false;
        if (FAILED(device->CreateShaderResourceView(tex[slot].tex, nullptr, &tex[slot].srv)))
            return false;
        return true;
    }
};

inline const SfMasks::Entry SfMasks::kEntries[14] = {
    { Glyph::General,    sftile::kGeneralPng,    sizeof(sftile::kGeneralPng),    sftile::kGeneralColour,
      sftile::kGeneralRGBA[0],    sftile::kGeneralRGBA[1],    sftile::kGeneralRGBA[2] },
    { Glyph::Access,     sftile::kAccessPng,     sizeof(sftile::kAccessPng),     sftile::kAccessColour,
      sftile::kAccessRGBA[0],     sftile::kAccessRGBA[1],     sftile::kAccessRGBA[2] },
    { Glyph::Action,     sftile::kActionPng,     sizeof(sftile::kActionPng),     sftile::kActionColour,
      sftile::kActionRGBA[0],     sftile::kActionRGBA[1],     sftile::kActionRGBA[2] },
    { Glyph::Appearance, sftile::kAppearancePng, sizeof(sftile::kAppearancePng), sftile::kAppearanceColour,
      sftile::kAppearanceRGBA[0], sftile::kAppearanceRGBA[1], sftile::kAppearanceRGBA[2] },
    { Glyph::Camera,     sftile::kCameraPng,     sizeof(sftile::kCameraPng),     sftile::kCameraColour,
      sftile::kCameraRGBA[0],     sftile::kCameraRGBA[1],     sftile::kCameraRGBA[2] },
    { Glyph::Home,       sftile::kHomePng,       sizeof(sftile::kHomePng),       sftile::kHomeColour,
      sftile::kHomeRGBA[0],       sftile::kHomeRGBA[1],       sftile::kHomeRGBA[2] },
    { Glyph::SearchRow,  sftile::kSearchRowPng,  sizeof(sftile::kSearchRowPng),  sftile::kSearchRowColour,
      sftile::kSearchRowRGBA[0],  sftile::kSearchRowRGBA[1],  sftile::kSearchRowRGBA[2] },
    { Glyph::Siri,       sftile::kSiriPng,       sizeof(sftile::kSiriPng),       sftile::kSiriColour,
      sftile::kSiriRGBA[0],       sftile::kSiriRGBA[1],       sftile::kSiriRGBA[2] },
    { Glyph::Standby,    sftile::kStandbyPng,    sizeof(sftile::kStandbyPng),    sftile::kStandbyColour,
      sftile::kStandbyRGBA[0],    sftile::kStandbyRGBA[1],    sftile::kStandbyRGBA[2] },
    { Glyph::Spotlight,  sftile::kSpotlightPng,  sizeof(sftile::kSpotlightPng),  sftile::kSpotlightColour,
      sftile::kSpotlightRGBA[0],  sftile::kSpotlightRGBA[1],  sftile::kSpotlightRGBA[2] },
    { Glyph::Magnifier,  sftile::kMagnifierPng,  sizeof(sftile::kMagnifierPng),  sftile::kMagnifierColour,
      sftile::kMagnifierRGBA[0],  sftile::kMagnifierRGBA[1],  sftile::kMagnifierRGBA[2] },
    { Glyph::Mic,        sftile::kMicPng,        sizeof(sftile::kMicPng),        sftile::kMicColour,
      sftile::kMicRGBA[0],        sftile::kMicRGBA[1],        sftile::kMicRGBA[2] },
    { Glyph::Apple,      sftile::kApplePng,      sizeof(sftile::kApplePng),      sftile::kAppleColour,
      sftile::kAppleRGBA[0],      sftile::kAppleRGBA[1],      sftile::kAppleRGBA[2] },
    { Glyph::Avatar,     sftile::kAvatarPng,     sizeof(sftile::kAvatarPng),     sftile::kAvatarColour,
      sftile::kAvatarRGBA[0],     sftile::kAvatarRGBA[1],     sftile::kAvatarRGBA[2] },
};

} // namespace sw
