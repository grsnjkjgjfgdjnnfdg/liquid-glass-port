#pragma once
#include <d3d11.h>

namespace opal::backend {

#include "../shaders/fullscreen_vs.h"
#include "../shaders/blur_h.h"
#include "../shaders/blur_v.h"
#include "../shaders/composite.h"
#include "../shaders/copy_ps.h"
#include "../shaders/gradient.h"
#include "../shaders/premultiply.h"
#include "../shaders/downsample.h"

inline ID3D11VertexShader* CreateFullscreenVS(ID3D11Device* device) {
    ID3D11VertexShader* shader = nullptr;
    device->CreateVertexShader(g_fullscreen_vs_bytecode, g_fullscreen_vs_size, nullptr, &shader);
    return shader;
}

inline ID3D11PixelShader* CreateBlurHPS(ID3D11Device* device) {
    ID3D11PixelShader* shader = nullptr;
    device->CreatePixelShader(g_blur_h_bytecode, g_blur_h_size, nullptr, &shader);
    return shader;
}

inline ID3D11PixelShader* CreateBlurVPS(ID3D11Device* device) {
    ID3D11PixelShader* shader = nullptr;
    device->CreatePixelShader(g_blur_v_bytecode, g_blur_v_size, nullptr, &shader);
    return shader;
}

inline ID3D11PixelShader* CreateCompositePS(ID3D11Device* device) {
    ID3D11PixelShader* shader = nullptr;
    device->CreatePixelShader(g_composite_bytecode, sizeof(g_composite_bytecode), nullptr, &shader);
    return shader;
}

inline ID3D11PixelShader* CreateCopyPS(ID3D11Device* device) {
    ID3D11PixelShader* shader = nullptr;
    device->CreatePixelShader(g_copy_ps_bytecode, sizeof(g_copy_ps_bytecode), nullptr, &shader);
    return shader;
}

inline ID3D11PixelShader* CreateGradientPS(ID3D11Device* device) {
    ID3D11PixelShader* shader = nullptr;
    device->CreatePixelShader(g_gradient_bytecode, g_gradient_size, nullptr, &shader);
    return shader;
}

inline ID3D11PixelShader* CreatePremultiplyPS(ID3D11Device* device) {
    ID3D11PixelShader* shader = nullptr;
    device->CreatePixelShader(g_premultiply_ps_bytecode, g_premultiply_ps_size, nullptr, &shader);
    return shader;
}

inline ID3D11PixelShader* CreateDownsamplePS(ID3D11Device* device) {
    ID3D11PixelShader* shader = nullptr;
    device->CreatePixelShader(g_downsample_bytecode, g_downsample_size, nullptr, &shader);
    return shader;
}

} // namespace opal::backend
