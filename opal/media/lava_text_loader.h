// ============================================================================
//  LavaText sheet texture loading, shared by the loader EXE and the injected
//  overlay.
//
//  Both targets own an ID3D11Device and both include this framework, so the
//  decode-and-upload path lives here once. The sheet is embedded (see
//  Loader/src/LavaTextSheet.h), inflated with the vendored DEFLATE decoder, and
//  handed to WIC, which every Windows target already links.
//
//  Decoding PNGs into raw RGBA and creating the texture is a one-time cost of a
//  few milliseconds; the SRV is cached per device for the lifetime of the
//  process.
// ============================================================================
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>
#include <wincodec.h>
#include <cstdint>
#include <vector>

#include "lava_text_sheet.h"

// The embedded sheet itself, defined in Loader/src (the loader and the overlay
// projects both carry Loader/src on their include path). Including it here keeps
// this header self-contained: a caller that only meant to pick a texture up
// cannot accidentally get the declaration without the data behind it.
#include "LavaTextSheet.h"
// The tail layer: the frozen frame with its satellite sparks lifted out, plus
// those sparks on their own. See lava_text_sheet.h for why it exists.
#include "LavaTextTail.h"

namespace opal {
namespace media {

namespace detail {

inline bool DecodePngRgba(const uint8_t* png, size_t size, std::vector<uint8_t>& out,
                          UINT& width, UINT& height) {
    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) return false;

    IWICStream* stream = nullptr;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr)) { factory->Release(); return false; }

    hr = stream->InitializeFromMemory(const_cast<BYTE*>(png), (DWORD)size);
    if (FAILED(hr)) { stream->Release(); factory->Release(); return false; }

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
    stream->Release();
    if (FAILED(hr)) { factory->Release(); return false; }

    IWICBitmapFrameDecode* frame = nullptr;
    hr = decoder->GetFrame(0, &frame);
    decoder->Release();
    if (FAILED(hr) || !frame) { factory->Release(); return false; }

    hr = frame->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
        frame->Release();
        factory->Release();
        return false;
    }

    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) { frame->Release(); factory->Release(); return false; }

    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
                               nullptr, 0.0, WICBitmapPaletteTypeCustom);
    frame->Release();
    if (FAILED(hr)) { converter->Release(); factory->Release(); return false; }

    out.resize((size_t)width * height * 4);
    hr = converter->CopyPixels(nullptr, width * 4, (UINT)out.size(), out.data());
    converter->Release();
    factory->Release();
    if (FAILED(hr)) { out.clear(); return false; }
    return true;
}

}  // namespace detail

// Decodes one embedded PNG into an immutable shader resource view. Returns null
// on failure; callers treat a null SRV as "no logo" rather than as a fatal error
// so a bad asset can never take the menu down.
inline ID3D11ShaderResourceView* UploadEmbeddedPng(ID3D11Device* device,
                                                   bool (*inflate)(std::vector<uint8_t>&)) {
    if (!device || !inflate) return nullptr;

    std::vector<uint8_t> png;
    if (!inflate(png)) return nullptr;

    std::vector<uint8_t> pixels;
    UINT width = 0, height = 0;
    if (!detail::DecodePngRgba(png.data(), png.size(), pixels, width, height)) return nullptr;

    // Immutable: written once, never updated, so the driver can keep it in the
    // most convenient memory and there is no map/unmap bookkeeping.
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA sub = {};
    sub.pSysMem = pixels.data();
    sub.SysMemPitch = width * 4;
    sub.SysMemSlicePitch = 0;

    ID3D11Texture2D* texture = nullptr;
    HRESULT hr = device->CreateTexture2D(&desc, &sub, &texture);
    if (FAILED(hr) || !texture) return nullptr;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    ID3D11ShaderResourceView* srv = nullptr;
    hr = device->CreateShaderResourceView(texture, &srvDesc, &srv);
    texture->Release();
    if (FAILED(hr)) return nullptr;
    return srv;
}

// Builds the tail layer (base + sparks) and publishes it for the Title
// component. Cached alongside the sheet, for the same reason.
inline ID3D11ShaderResourceView* CreateLavaTextTail(ID3D11Device* device, bool force = false) {
    static ID3D11ShaderResourceView* s_cached = nullptr;
    if (!force && s_cached) {
        LavaTailSrv() = s_cached;
        return s_cached;
    }
    if (!device) return nullptr;
    s_cached = UploadEmbeddedPng(device, &embedded::LAVA_TAIL_Inflate);
    LavaTailSrv() = s_cached;
    return s_cached;
}

// Builds a shader resource view for the embedded LavaText sheet. Returns null on
// failure; callers treat a null SRV as "no logo" rather than as a fatal error so
// a bad asset can never take the menu down.
//
// This also loads and publishes the tail layer, because the two are one asset:
// the frozen frame the sheet ends on is the frame the tail layer removes the
// sparks from. Loading the sheet without it would leave the mark missing every
// satellite spark once it locks. Keeping the pair behind the one call every host
// already makes means no host can get that wrong.
//
// The result is cached per entry point: the two hosts initialise at most once,
// but a device reset (which recreates the swapchain and can re-enter the
// overlay's init) would otherwise upload the sheet again every time. Pass
// `force` to rebuild deliberately.
inline ID3D11ShaderResourceView* CreateLavaTextSheet(ID3D11Device* device, bool force = false) {
    static ID3D11ShaderResourceView* s_cached = nullptr;
    if (!force && s_cached) {
        CreateLavaTextTail(device, force);
        return s_cached;
    }
    if (!device) return nullptr;
    s_cached = UploadEmbeddedPng(device, &embedded::LAVA_SHEET_Inflate);
    CreateLavaTextTail(device, force);
    return s_cached;
}

}  // namespace media
}  // namespace opal
