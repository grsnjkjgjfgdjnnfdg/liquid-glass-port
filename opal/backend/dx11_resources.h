#pragma once
#include <d3d11.h>
#include <wincodec.h>
#include <vector>

#ifdef _MSC_VER
#pragma comment(lib, "windowscodecs.lib")
#endif

namespace opal::backend {

inline ID3D11Texture2D* CreateTexture2D(ID3D11Device* device, int width, int height, DXGI_FORMAT format,
                                         UINT bindFlags, UINT miscFlags = 0, int mipLevels = 1,
                                         D3D11_USAGE usage = D3D11_USAGE_DEFAULT,
                                         UINT cpuAccess = 0) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width              = (UINT)width;
    desc.Height             = (UINT)height;
    desc.MipLevels          = (UINT)mipLevels;
    desc.ArraySize          = 1;
    desc.Format             = format;
    desc.SampleDesc.Count   = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage              = usage;
    desc.BindFlags          = bindFlags;
    desc.CPUAccessFlags     = cpuAccess;
    desc.MiscFlags          = miscFlags;

    ID3D11Texture2D* tex = nullptr;
    device->CreateTexture2D(&desc, nullptr, &tex);
    return tex;
}

inline ID3D11RenderTargetView* CreateRTV(ID3D11Device* device, ID3D11Texture2D* tex) {
    ID3D11RenderTargetView* rtv = nullptr;
    device->CreateRenderTargetView(tex, nullptr, &rtv);
    return rtv;
}

inline ID3D11ShaderResourceView* CreateSRV(ID3D11Device* device, ID3D11Texture2D* tex) {
    ID3D11ShaderResourceView* srv = nullptr;
    device->CreateShaderResourceView(tex, nullptr, &srv);
    return srv;
}

inline ID3D11SamplerState* CreateSampler(ID3D11Device* device, D3D11_FILTER filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT,
                                          D3D11_TEXTURE_ADDRESS_MODE address = D3D11_TEXTURE_ADDRESS_CLAMP) {
    D3D11_SAMPLER_DESC desc = {};
    desc.Filter         = filter;
    desc.AddressU       = address;
    desc.AddressV       = address;
    desc.AddressW       = address;
    desc.MaxAnisotropy  = 1;
    desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    desc.MaxLOD         = D3D11_FLOAT32_MAX;

    ID3D11SamplerState* sampler = nullptr;
    device->CreateSamplerState(&desc, &sampler);
    return sampler;
}

inline ID3D11Buffer* CreateConstantBuffer(ID3D11Device* device, size_t size) {
    D3D11_BUFFER_DESC desc = {};
    desc.ByteWidth      = (UINT)size;
    desc.Usage          = D3D11_USAGE_DYNAMIC;
    desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    ID3D11Buffer* cb = nullptr;
    device->CreateBuffer(&desc, nullptr, &cb);
    return cb;
}

inline void UpdateConstantBuffer(ID3D11DeviceContext* ctx, ID3D11Buffer* cb, const void* data, size_t size) {
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    ctx->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy(mapped.pData, data, size);
    ctx->Unmap(cb, 0);
}

inline void SafeRelease(IUnknown* p) { if (p) p->Release(); }

inline ID3D11ShaderResourceView* LoadTextureFromFile(ID3D11Device* device, const wchar_t* path) {
    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) return nullptr;

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromFilename(path, nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) { factory->Release(); return nullptr; }

    IWICBitmapFrameDecode* frame = nullptr;
    decoder->GetFrame(0, &frame);
    decoder->Release();
    if (!frame) { factory->Release(); return nullptr; }

    UINT w = 0, h = 0;
    frame->GetSize(&w, &h);

    IWICFormatConverter* converter = nullptr;
    factory->CreateFormatConverter(&converter);
    converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
    frame->Release();

    std::vector<uint32_t> pixels(w * h);
    converter->CopyPixels(nullptr, w * 4, w * h * 4, (BYTE*)pixels.data());
    converter->Release();
    factory->Release();

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = w;
    texDesc.Height = h;
    texDesc.MipLevels = 0;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ID3D11Texture2D* tex = nullptr;
    hr = device->CreateTexture2D(&texDesc, nullptr, &tex);
    if (FAILED(hr)) return nullptr;

    ID3D11DeviceContext* ctx = nullptr;
    device->GetImmediateContext(&ctx);
    ctx->UpdateSubresource(tex, 0, nullptr, pixels.data(), w * 4, 0);

    ID3D11ShaderResourceView* srv = CreateSRV(device, tex);
    ctx->GenerateMips(srv);
    ctx->Release();
    tex->Release();
    return srv;
}

inline ID3D11ShaderResourceView* LoadTextureFromMemory(ID3D11Device* device, const unsigned char* data, unsigned int size, bool forceOpaque = false) {
    IWICImagingFactory* factory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&factory));
    if (FAILED(hr) || !factory) return nullptr;

    IWICStream* stream = nullptr;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr)) { factory->Release(); return nullptr; }
    hr = stream->InitializeFromMemory(const_cast<BYTE*>(data), size);
    if (FAILED(hr)) { stream->Release(); factory->Release(); return nullptr; }

    IWICBitmapDecoder* decoder = nullptr;
    hr = factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
    stream->Release();
    if (FAILED(hr)) { factory->Release(); return nullptr; }

    IWICBitmapFrameDecode* frame = nullptr;
    decoder->GetFrame(0, &frame);
    decoder->Release();
    if (!frame) { factory->Release(); return nullptr; }

    UINT w = 0, h = 0;
    frame->GetSize(&w, &h);

    IWICFormatConverter* converter = nullptr;
    factory->CreateFormatConverter(&converter);
    converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
    frame->Release();

    std::vector<uint32_t> pixels(w * h);
    converter->CopyPixels(nullptr, w * 4, w * h * 4, (BYTE*)pixels.data());
    converter->Release();
    factory->Release();

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = w;
    texDesc.Height = h;
    texDesc.MipLevels = 0;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ID3D11Texture2D* tex = nullptr;
    hr = device->CreateTexture2D(&texDesc, nullptr, &tex);
    if (FAILED(hr)) return nullptr;

    ID3D11DeviceContext* ctx = nullptr;
    device->GetImmediateContext(&ctx);
    ctx->UpdateSubresource(tex, 0, nullptr, pixels.data(), w * 4, 0);

    ID3D11ShaderResourceView* srv = CreateSRV(device, tex);
    ctx->GenerateMips(srv);
    ctx->Release();
    tex->Release();
    return srv;
}

} // namespace opal::backend
