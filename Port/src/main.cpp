#define NOMINMAX
#include <Windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "../../opal/backend/dx11_backend.h"
#include "../../opal/glass/glass_renderer.h"
#include "../fonts/FontsGenerated.h"

#include "../fonts/FontsGenerated.h"           // kept as a fallback


#include "Theme.h"
#include "Ui.h"
#include "Wallpaper.h"
#include "IosSettings.h"
#include "IosControls.h"
#include "NavTransition.h"
#include "SheetTransition.h"
#include "FlowScreen.h"
#include "SfMasks.h"
#include "Symbols.h"
#include "GlassPass.h"
#include "../../Opal/SwiftUiOpal.h"   // compile-check the host bridge

#pragma comment(lib, "windowscodecs.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

using namespace sw;
namespace backend = opal::backend;

static opal::backend::D3D11Context g_dx;
static opal::glass::GlassRenderer g_glass;
static ID3D11ShaderResourceView* g_wallpaper = nullptr;
static bool g_running = true;
static bool g_iosMode = false;
static bool g_darkMode = false;
static int  g_iosScreen = 0;   // 0 = Settings root, 1 = control gallery
static SfIcons g_icons;
static SfMasks g_rowMasks;
static GlassPass g_glassPass;
static std::vector<GlassPanel> g_panels;
static IosGalleryState g_gallery;
static IosSettingsState g_iosSettings;
static IosGlassState g_glassDemo;
static IosRefractionState g_refraction;
static nav::State g_navPush;
static sheet::State g_sheet;
static sheet::SheetHostState g_sheetHost;
static flow::State g_flow;
static bool g_autoFlow = false;
static bool g_flowSheetOnly = false;
static bool g_autoSheet = false;
static std::string g_sheetLogPath;
static FILE* g_sheetLog = nullptr;
static float g_sheetStart = 0.0f;
static int g_sheetStage = 0;
static bool g_autoPush = false;
static bool g_pushed = false;
static float g_pushTime = 0.0f;
static std::string g_navLogPath;
static FILE* g_navLog = nullptr;
static bool g_glassReady = false;
static bool g_autoSlider = false;   // drag the slider's thumb
static std::string g_typeText;   // characters typed into a focused field
static bool g_autoScroll = false;  // synthesized vertical list drag
static bool g_autoSwipe = false;   // synthesized left-edge pop drag
static float g_clickX = -1.0f, g_clickY = -1.0f;
static float g_clickAt = 1.60f;   // when the synthesized click lands   // one synthesized press, for capture runs
static float g_autoDrag = 0.0f;   // synthesized grabber drag, in pt, for capture runs
static int g_hoverRow = -1;   // screenshot automation: hold a row highlighted
static bool g_bridgeSelftest = false;   // construct the Opal bridge and report
static bool g_opalBridge = false;       // reserved: render a page through the bridge   // run the Opal bridge once and report
static bool g_useOpalGlass = false;   // desktop host frame: Opal owns the backdrop/blur
// Opal renders at the size it was initialised with, so its frame lives in a 1x
// target that the 3x frame blits in as the backdrop; the panels then sample Opal's
// blur. That keeps Opal's renderer untouched and the ported UI at 3x.
static ID3D11Texture2D* g_opalTex = nullptr;
static ID3D11RenderTargetView* g_opalRTV = nullptr;
static ID3D11ShaderResourceView* g_opalSRV = nullptr;
static bool CreateOpalTarget(ID3D11Device* dev, int w, int h) {
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = (UINT)w; td.Height = (UINT)h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &g_opalTex))) return false;
    if (FAILED(dev->CreateRenderTargetView(g_opalTex, nullptr, &g_opalRTV))) return false;
    if (FAILED(dev->CreateShaderResourceView(g_opalTex, nullptr, &g_opalSRV))) return false;
    return true;
}
static int g_width = 1440, g_height = 900;
static std::string g_shotPath;
static bool g_autoToggle = false;
static bool g_showBlur = false;
static bool g_showScene = false;
static bool g_noBlur = false;
static float g_shotAt = 2.0f;
static bool g_toggled = false;
static std::string g_animLogPath;
static float g_toggleTime = 0.0f;
static FILE* g_animLog = nullptr;

static float g_slider = 0.62f;
static SliderState g_sliderState;
static bool g_wifi = true;
static bool g_bluetooth = true;
static bool g_lowPower = false;
static SwitchState g_wifiState, g_bluetoothState, g_lowPowerState;
static float g_volume = 0.68f;
static KnobState g_knobState;
static float g_track = 0.34f;
static SliderState g_trackState;

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return 1;
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// 3x supersampling.
//
// The device this UI is measured against rasterises at 3x and the captures we
// diff against are downsampled from that. So a 1 pt hairline, the stem of a
// 17 pt glyph and the rim of a glass panel in the reference are each the
// average of nine device pixels. Drawing the port at 1x makes every one of
// them a pixel too dark and every glyph a weight too heavy â€” which is exactly
// what the diff has been telling us for twelve rounds.
//
// So the visible frame is rasterised at 3x (io.DisplayFramebufferScale) into an
// offscreen target and box-downsampled on the way to the swap chain: the same
// pipeline the device runs, and the reason the port can look "rendered" rather
// than "drawn".
static const int kSuperSample = 3;
static ID3D11Texture2D* g_ssTex = nullptr;
static ID3D11RenderTargetView* g_ssRTV = nullptr;
static ID3D11ShaderResourceView* g_ssSRV = nullptr;
static ID3D11VertexShader* g_ssVS = nullptr;
static ID3D11PixelShader* g_ssPS = nullptr;
static ID3D11SamplerState* g_ssSampler = nullptr;
static ID3D11RasterizerState* g_ssRaster = nullptr;

static const char* kDownsampleHLSL = R"(
Texture2D src : register(t0);
SamplerState smp : register(s0);
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
VSOut VSMain(uint id : SV_VertexID) {
    VSOut o;
    float2 p = float2((float)((id << 1) & 2), (float)(id & 2));
    o.pos = float4(p * 2.0 - 1.0, 0.0, 1.0);
    o.uv  = float2(p.x * 0.5, 1.0 - p.y * 0.5);
    return o;
}
float4 PSMain(VSOut i) : SV_Target {
    int2 base = int2(i.pos.xy) * SS;
    float4 c = float4(0, 0, 0, 0);
    [unroll] for (int j = 0; j < SS; ++j)
        [unroll] for (int q = 0; q < SS; ++q)
            c += src.Load(int3(base + int2(q, j), 0));
    return c * (1.0 / (float)(SS * SS));
}
)";

static bool CreateSuperSample(ID3D11Device* dev, int w, int h) {
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = (UINT)w * kSuperSample;
    td.Height = (UINT)h * kSuperSample;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &g_ssTex))) return false;
    if (FAILED(dev->CreateRenderTargetView(g_ssTex, nullptr, &g_ssRTV))) return false;
    if (FAILED(dev->CreateShaderResourceView(g_ssTex, nullptr, &g_ssSRV))) return false;

    std::string hlsl = std::string("#define SS ") + std::to_string(kSuperSample) + "\n" + kDownsampleHLSL;
    ID3DBlob* vsb = nullptr;
    ID3DBlob* psb = nullptr;
    ID3DBlob* err = nullptr;
    if (FAILED(D3DCompile(hlsl.c_str(), hlsl.size(), "downsample", nullptr, nullptr,
                          "VSMain", "vs_4_0", 0, 0, &vsb, &err))) return false;
    if (FAILED(D3DCompile(hlsl.c_str(), hlsl.size(), "downsample", nullptr, nullptr,
                          "PSMain", "ps_4_0", 0, 0, &psb, &err))) return false;
    dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &g_ssVS);
    dev->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &g_ssPS);
    vsb->Release();
    psb->Release();

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    dev->CreateSamplerState(&sd, &g_ssSampler);
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    dev->CreateRasterizerState(&rd, &g_ssRaster);
    return true;
}

static void ReleaseSuperSample() {
    auto rel = [](auto*& p) { if (p) { p->Release(); p = nullptr; } };
    rel(g_ssRaster); rel(g_ssSampler); rel(g_ssPS); rel(g_ssVS);
    rel(g_ssSRV); rel(g_ssRTV); rel(g_ssTex);
}

// Box-downsample the 3x target onto the swap chain's back buffer.
static void BlitSuperSample(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* dst, int w, int h) {    if (!g_ssSRV || !g_ssVS || !g_ssPS) return;
    D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f };
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetScissorRects(0, nullptr);
    ctx->RSSetState(g_ssRaster);
    ctx->OMSetRenderTargets(1, &dst, nullptr);
    ID3D11ShaderResourceView* srv = g_ssSRV;
    ctx->PSSetShaderResources(0, 1, &srv);
    ctx->PSSetSamplers(0, 1, &g_ssSampler);
    ctx->VSSetShader(g_ssVS, nullptr, 0);
    ctx->PSSetShader(g_ssPS, nullptr, 0);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* nullsrv = nullptr;
    ctx->PSSetShaderResources(0, 1, &nullsrv);
}

// srcTex == nullptr saves the swap chain's back buffer.
static bool SavePng(const std::string& path, ID3D11Device* device, ID3D11DeviceContext* ctx,
                    IDXGISwapChain* swap, int w, int h, ID3D11Texture2D* srcTex = nullptr) {
    ID3D11Texture2D* back = srcTex;
    if (!back) {
        if (FAILED(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back)) || !back) return false;
    }
    D3D11_TEXTURE2D_DESC desc = {};
    back->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    ID3D11Texture2D* staging = nullptr;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, &staging))) { back->Release(); return false; }
    ctx->CopyResource(staging, back);
    if (!srcTex) back->Release();
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(ctx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped))) { staging->Release(); return false; }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IWICImagingFactory* factory = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICStream* stream = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* props = nullptr;
    bool ok = false;
    if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&factory)))) {
        if (SUCCEEDED(factory->CreateStream(&stream)) &&
            SUCCEEDED(stream->InitializeFromFilename(std::wstring(path.begin(), path.end()).c_str(), GENERIC_WRITE)) &&
            SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
            SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache)) &&
            SUCCEEDED(encoder->CreateNewFrame(&frame, &props)) &&
            SUCCEEDED(frame->Initialize(props))) {
            // The non-composition swap chain is R8G8B8A8_UNORM while WIC's
            // well-supported PNG path here is BGRA. Swap R/B into a staging
            // copy before handing it to the encoder.
            std::vector<BYTE> pixels((size_t)mapped.RowPitch * h);
            memcpy(pixels.data(), mapped.pData, pixels.size());
            for (int y = 0; y < h; ++y) {
                BYTE* row = pixels.data() + (size_t)y * mapped.RowPitch;
                for (int x = 0; x < w; ++x) std::swap(row[x * 4 + 0], row[x * 4 + 2]);
            }
            WICPixelFormatGUID fmt = GUID_WICPixelFormat32bppBGRA;
            frame->SetSize(w, h);
            frame->SetPixelFormat(&fmt);
            if (fmt == GUID_WICPixelFormat32bppBGRA) {
                IWICBitmap* bitmap = nullptr;
                if (SUCCEEDED(factory->CreateBitmapFromMemory(w, h, GUID_WICPixelFormat32bppBGRA,
                        mapped.RowPitch, mapped.RowPitch * h, pixels.data(), &bitmap))) {
                    frame->WriteSource(bitmap, nullptr);
                    bitmap->Release();
                }
            }
            frame->Commit();
            encoder->Commit();
            ok = true;
        }
    }
    if (props) props->Release();
    if (frame) frame->Release();
    if (encoder) encoder->Release();
    if (stream) stream->Release();
    if (factory) factory->Release();
    ctx->Unmap(staging, 0);
    staging->Release();
    CoUninitialize();
    return ok;
}

// ---------------------------------------------------------------------------
// Software-rasterised device for `--shot`.
//
// D3D11 WARP needs no display adapter, so a capture cannot interact with the
// desktop compositor or the vendor driver at all. The 1x "back buffer" is an
// ordinary texture instead of a swap chain, so the render path stays identical
// (the 3x downsample still lands in g_dx.rtv) minus the present.
//
// Reason: this machine hard-hung twice while several hardware-accelerated
// instances were presenting, with a Kernel-Power 41 and no bug check. A capture
// does not need the GPU, so it no longer asks for it.
static ID3D11Texture2D* g_lowTex = nullptr;
static ID3D11RenderTargetView* g_lowRTV = nullptr;

static bool CreateSoftwareDevice(opal::backend::D3D11Context& dx, int w, int h) {
    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
                                         D3D_FEATURE_LEVEL_10_0 };
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 3,
                                 D3D11_SDK_VERSION, &dx.device, &fl, &dx.context)))
        return false;
    dx.width = w;
    dx.height = h;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = (UINT)w;
    td.Height = (UINT)h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (FAILED(dx.device->CreateTexture2D(&td, nullptr, &g_lowTex))) return false;
    if (FAILED(dx.device->CreateRenderTargetView(g_lowTex, nullptr, &g_lowRTV))) return false;
    dx.rtv = g_lowRTV;
    return true;
}

struct NavItem { const char* label; Icon icon; };
static NavItem g_nav[] = {
    { "Overview", Icon::Grid },
    { "Controls", Icon::Sliders },
    { "Appearance", Icon::Brush },
    { "Motion", Icon::Wave },
    { "Materials", Icon::Info },
};

static void DrawWindow(UI& ui) {
    Rect win(120, 80, 1200, 740);
    ui.Shadow(win, 26, 38, 0.32f, 18.0f);
    ui.Glass(win, 26, Rgba(1, 1, 1, 0.56f), 1.0f, false);
    ui.StrokeRounded(Rect(win.x + 0.5f, win.y + 0.5f, win.w - 1, win.h - 1), 25.5f,
                     Rgba(1, 1, 1, 0.72f), 1.0f);

    // traffic lights
    float ty = win.y + 26;
    auto light = [&](float x, Rgba c) {
        ui.CircleShadow(x, ty, 5.5f, 0.12f);
        ui.dl->AddCircleFilled(ImVec2(x, ty), 6.0f, c.U32(), 24);
        ui.dl->AddCircle(ImVec2(x, ty), 5.5f, IM_COL32(0, 0, 0, 30), 24, 1.0f);
    };
    light(win.x + 20, Rgba(1.00f, 0.37f, 0.34f, 0.95f));
    light(win.x + 40, Rgba(0.99f, 0.74f, 0.18f, 0.95f));
    light(win.x + 60, Rgba(0.16f, 0.78f, 0.25f, 0.95f));
    ui.TextCentered(ui.fonts.semibold13, 13.0f, Rect(win.x, win.y + 14, win.w, 24), "Glass", ui.theme.secondary);

    // sidebar slab (a second material layer, low opacity)
    Rect sidebar(win.x, win.y + 50, 220, win.h - 50);
    ui.FillRounded(sidebar, 20, Rgba(1, 1, 1, 0.10f));
    ui.dl->AddLine(ImVec2(sidebar.x + sidebar.w, sidebar.y + 10),
                   ImVec2(sidebar.x + sidebar.w, sidebar.y + sidebar.h - 10),
                   Rgba(0, 0, 0, 0.07f).U32(), 1.0f);

    // app identity
    Rect icon(win.x + 20, win.y + 66, 28, 28);
    ui.FillRounded(icon, 8, Rgba(0.08f, 0.55f, 1.0f, 1.0f));
    ui.dl->AddRectFilledMultiColor(ImVec2(icon.x + 2, icon.y + 2), ImVec2(icon.x + icon.w - 2, icon.y + icon.h * 0.56f),
                                   IM_COL32(255, 255, 255, 70), IM_COL32(255, 255, 255, 30),
                                   IM_COL32(255, 255, 255, 0), IM_COL32(255, 255, 255, 0));
    ui.TextCentered(ui.fonts.semibold17, 17.0f, icon, "L", Rgba(1, 1, 1, 1));
    ui.Text(ui.fonts.semibold15, 15.0f, win.x + 58, win.y + 67, "Glass", ui.theme.text);
    ui.Text(ui.fonts.regular11, 11.0f, win.x + 58, win.y + 83, "Design System", ui.theme.secondary);

    // navigation
    int selected = 1;
    float navY = win.y + 128;
    ui.Text(ui.fonts.semibold11, 11.0f, win.x + 26, navY, "WORKSPACE", ui.theme.tertiary);
    navY += 20;
    for (int i = 0; i < 5; ++i) {
        Rect row(win.x + 12, navY + i * 32, 196, 28);
        if (i == selected) {
            ui.FillRounded(row, 7, ui.theme.dark ? Rgba(1, 1, 1, 0.14f) : Rgba(0, 0, 0, 0.075f));
            ui.StrokeRounded(row, 7, Rgba(0, 0, 0, 0.05f), 1.0f);
        } else if (row.Contains(ui.mouse.x, ui.mouse.y)) {
            ui.FillRounded(row, 7, Rgba(0, 0, 0, 0.035f));
        }
        DrawIcon(ui, g_nav[i].icon, Rect(row.x + 10, row.y + 7, 14, 14),
                 i == selected ? ui.theme.text : ui.theme.secondary);
        ui.Text(ui.fonts.regular13, 13.0f, row.x + 34, row.y + 6,
                g_nav[i].label, i == selected ? ui.theme.text : ui.theme.secondary);
    }

    // footer user
    Rect user(win.x + 16, win.y + win.h - 62, 188, 42);
    ui.FillRounded(user, 9, Rgba(1, 1, 1, 0.16f));
    ui.StrokeRounded(user, 9, Rgba(1, 1, 1, 0.20f), 1.0f);
    ui.dl->AddCircleFilled(ImVec2(user.x + 21, user.y + 21), 13.0f, Rgba(0.22f, 0.30f, 0.42f, 1).U32(), 24);
    ui.TextCentered(ui.fonts.semibold13, 13.0f, Rect(user.x + 8, user.y + 8, 26, 26), "P", Rgba(1, 1, 1, 1));
    ui.Text(ui.fonts.semibold13, 13.0f, user.x + 42, user.y + 7, "Preview user", ui.theme.text);
    ui.Text(ui.fonts.regular11, 11.0f, user.x + 42, user.y + 23, "macOS 27", ui.theme.secondary);

    // ------------------------------- content ------------------------------
    float cx = win.x + 220 + 34;
    float cy = win.y + 50 + 30;
    float contentW = win.w - 220 - 68;
    ui.Text(ui.fonts.semibold28, 28.0f, cx, cy, "Controls", ui.theme.text);
    ui.Text(ui.fonts.regular13, 13.0f, cx, cy + 36,
            "SwiftUI geometry and motion, rendered natively on Windows.", ui.theme.secondary);

    // theme segmented pinned top-right
    static int themeSel = 0;
    static SegState themeSeg;
    static const char* themeLabels[] = { "Light", "Dark" };
    DrawSegmented(ui, Rect(win.x + win.w - 190, win.y + 32, 110, 24), themeSel, 2, themeSeg,
                  themeLabels, ui.dt);

    float colLeft = cx;
    float leftW = 568;
    float rightX = cx + leftW + 28;
    float rightW = contentW - leftW - 28;
    float y = cy + 78;

    // slider card
    ui.Text(ui.fonts.semibold13, 13.0f, colLeft, y, "Slider", ui.theme.secondary);
    y += 20;
    Rect sliderCard(colLeft, y, leftW, 112);
    ui.Glass(sliderCard, 16, Rgba(1, 1, 1, 0.24f), 1.0f);
    ui.Text(ui.fonts.semibold13, 13.0f, sliderCard.x + 18, sliderCard.y + 16, "Volume", ui.theme.text);
    char val[16]; snprintf(val, sizeof(val), "%d%%", (int)std::lround(g_slider * 100));
    ui.TextRight(ui.fonts.regular13, 13.0f, Rect(sliderCard.x, sliderCard.y + 16, sliderCard.w - 18, 18), val, ui.theme.secondary);
    Rect sliderRow(sliderCard.x + 18, sliderCard.y + 48, sliderCard.w - 36, 24);
    DrawSlider(ui, sliderRow, g_slider, g_sliderState, ui.dt);
    ui.Text(ui.fonts.regular11, 11.0f, sliderCard.x + 18, sliderCard.y + 82,
            "140 pt kit width  Â·  6 pt track  Â·  2 pt lozenge", ui.theme.tertiary);
    y += sliderCard.h + 28;

    // switches
    ui.Text(ui.fonts.semibold13, 13.0f, colLeft, y, "Switches", ui.theme.secondary);
    y += 20;
    Rect switchCard(colLeft, y, leftW, 172);
    ui.Glass(switchCard, 16, Rgba(1, 1, 1, 0.24f), 1.0f);
    struct Row { const char* label; const char* sub; ControlSize size; bool* state; SwitchState* s; };
    Row rows[] = {
        { "Wi-Fi", "Home Network", ControlSize::Regular, &g_wifi, &g_wifiState },
        { "Bluetooth", "2 devices connected", ControlSize::Small, &g_bluetooth, &g_bluetoothState },
        { "Low Power Mode", "Automatically enabled at 20%", ControlSize::Mini, &g_lowPower, &g_lowPowerState },
    };
    for (int i = 0; i < 3; ++i) {
        float ry = switchCard.y + 12 + i * 52;
        if (i) ui.dl->AddLine(ImVec2(switchCard.x + 18, ry - 8), ImVec2(switchCard.x + switchCard.w - 18, ry - 8),
                              ui.theme.separator.U32(), 1.0f);
        ui.Text(ui.fonts.regular13, 13.0f, switchCard.x + 18, ry + 2, rows[i].label, ui.theme.text);
        ui.Text(ui.fonts.regular11, 11.0f, switchCard.x + 18, ry + 20, rows[i].sub, ui.theme.secondary);
        DrawSwitch(ui, Rect(switchCard.x + switchCard.w - 80, ry, 62, 38), *rows[i].state,
                   *rows[i].s, ui.dt);
    }
    y += switchCard.h + 28;

    // buttons + segmented
    ui.Text(ui.fonts.semibold13, 13.0f, colLeft, y, "Buttons and segmented controls", ui.theme.secondary);
    y += 20;
    Rect bottomCard(colLeft, y, leftW, 132);
    ui.Glass(bottomCard, 16, Rgba(1, 1, 1, 0.24f), 1.0f);
    static ButtonState b1, b2, b3;
    DrawButton(ui, Rect(bottomCard.x + 18, bottomCard.y + 18, 88, 24), "Continue", ButtonKind::Prominent, b1, ui.dt);
    DrawButton(ui, Rect(bottomCard.x + 116, bottomCard.y + 18, 82, 24), "Cancel", ButtonKind::Bordered, b2, ui.dt);
    DrawButton(ui, Rect(bottomCard.x + 208, bottomCard.y + 18, 82, 24), "Learn More", ButtonKind::Plain, b3, ui.dt);
    static int seg2 = 1;
    static SegState s2;
    static const char* segLabels[] = { "Day", "Week", "Month" };
    ui.Text(ui.fonts.regular11, 11.0f, bottomCard.x + 18, bottomCard.y + 60, "Segmented Â· 90 Ã— 24", ui.theme.tertiary);
    DrawSegmented(ui, Rect(bottomCard.x + 18, bottomCard.y + 78, 90, 24), seg2, 3, s2, segLabels, ui.dt);
    ui.Text(ui.fonts.regular11, 11.0f, bottomCard.x + 132, bottomCard.y + 60, "Text field", ui.theme.tertiary);
    Rect field(bottomCard.x + 132, bottomCard.y + 78, 190, 24);
    ui.FillRounded(field, 6, Rgba(1, 1, 1, 0.72f));
    ui.StrokeRounded(field, 6, Rgba(0, 0, 0, 0.12f), 1.0f);
    ui.Text(ui.fonts.regular13, 13.0f, field.x + 9, field.y + 4, "Preview user", ui.theme.text);

    // right column dial
    ui.Text(ui.fonts.semibold13, 13.0f, rightX, cy + 78 - 20, "Dial", ui.theme.secondary);
    Rect dialCard(rightX, cy + 78, rightW, 268);
    ui.Glass(dialCard, 18, Rgba(1, 1, 1, 0.24f), 1.0f);
    ui.Text(ui.fonts.semibold13, 13.0f, dialCard.x + 18, dialCard.y + 16, "Output", ui.theme.text);
    ui.TextRight(ui.fonts.regular11, 11.0f, Rect(dialCard.x, dialCard.y + 17, dialCard.w - 18, 16), "Studio Display", ui.theme.secondary);
    DrawKnob(ui, Rect(dialCard.x + 20, dialCard.y + 38, dialCard.w - 40, 190), g_volume, g_knobState, ui.dt);

    // right column now playing
    Rect playCard(rightX, dialCard.y + dialCard.h + 24, rightW, 176);
    ui.Glass(playCard, 18, Rgba(1, 1, 1, 0.24f), 1.0f);
    Rect art(playCard.x + 16, playCard.y + 16, 112, 112);
    ui.FillRounded(art, 10, Rgba(0.20f, 0.16f, 0.28f, 1));
    ui.dl->AddRectFilledMultiColor(ImVec2(art.x + 2, art.y + 2), ImVec2(art.x + art.w - 2, art.y + art.h - 2),
                                   IM_COL32(238, 166, 123, 255), IM_COL32(166, 108, 133, 255),
                                   IM_COL32(54, 50, 92, 255), IM_COL32(42, 45, 79, 255));
    ui.dl->AddCircleFilled(ImVec2(art.x + 68, art.y + 48), 26.0f, IM_COL32(255, 238, 211, 90), 40);
    ui.Text(ui.fonts.semibold15, 15.0f, playCard.x + 144, playCard.y + 28, "Velvet Sky", ui.theme.text);
    ui.Text(ui.fonts.regular11, 11.0f, playCard.x + 144, playCard.y + 48, "Glass Audio", ui.theme.secondary);
    ui.dl->AddCircleFilled(ImVec2(playCard.x + 168, playCard.y + 100), 18.0f, ui.theme.accent.U32(), 32);
    ui.dl->AddTriangleFilled(ImVec2(playCard.x + 163, playCard.y + 91),
                             ImVec2(playCard.x + 163, playCard.y + 109),
                             ImVec2(playCard.x + 177, playCard.y + 100), IM_COL32(255, 255, 255, 255));
    DrawSlider(ui, Rect(playCard.x + 196, playCard.y + 90, playCard.w - 216, 24), g_track,
               g_trackState, ui.dt);

    // footer
    ui.Text(ui.fonts.regular11, 11.0f, cx, win.y + win.h - 34,
            "Apple macOS 27 UI Kit metrics  Â·  native 24 pt control heights  Â·  SwiftUI analytic springs",
            ui.theme.tertiary);
}

// ---------------------------------------------------------------------------
// Desktop host: the ported iOS 27 page inside a macOS 27 window.
//
// There is no macOS 27 hardware here to capture from, so this is not a port of an
// Apple *screen* â€” it is the integration view: the same ported page, drawn by the
// same code, hosted in a window that uses the measured material, over a backdrop
// calm enough to judge it on.
//
// Two earlier versions of this page are why it looks like this. The first was a
// four-corner gradient, which averages to grey in the middle and made every glass
// panel read as flat smoke. The second replaced it with saturated colour blobs plus
// fake text lines, which was worse: the material had no structure to refract and
// the whole thing read as a poster. A calm two-axis gradient keeps the window's
// frost and rim legible, which is the point of showing it.
//
// The window chrome follows macOS 26/27's glass treatment: frosted, rounded, rim
// lit, with the title bar sharing the same surface rather than sitting on an opaque
// strip.
static void DrawSettings(UI& ui) {
    const float W = ui.screenW, H = ui.screenH;

    // ---- backdrop -------------------------------------------------------
    {
        struct Stop { float y; ImU32 l, r; };
        const Stop stops[] = {
            { 0.00f * H, IM_COL32(18, 20, 44, 255), IM_COL32(28, 28, 66, 255) },
            { 0.30f * H, IM_COL32(34, 32, 78, 255), IM_COL32(52, 38, 104, 255) },
            { 0.62f * H, IM_COL32(40, 30, 86, 255), IM_COL32(44, 34, 92, 255) },
            { 1.00f * H, IM_COL32(16, 16, 38, 255), IM_COL32(24, 20, 50, 255) },
        };
        const int n = (int)(sizeof(stops) / sizeof(stops[0]));
        auto ch = [](ImU32 c, int s) { return (int)((c >> s) & 0xFF); };
        for (int y = 0; y < (int)H; ++y) {
            int i = 0;
            while (i + 2 < n && stops[i + 1].y < (float)y) ++i;
            float span = stops[i + 1].y - stops[i].y;
            float f = span > 0.0f ? ((float)y - stops[i].y) / span : 0.0f;
            auto mix = [&](int sh, bool right) {
                int a = ch(right ? stops[i].r : stops[i].l, sh);
                int b = ch(right ? stops[i + 1].r : stops[i + 1].l, sh);
                return a + (int)((b - a) * f);
            };
            ImU32 l = IM_COL32(mix(0, false), mix(8, false), mix(16, false), 255);
            ImU32 r = IM_COL32(mix(0, true), mix(8, true), mix(16, true), 255);
            ui.dl->AddRectFilledMultiColor(ImVec2(0, (float)y), ImVec2(W, (float)y + 1),
                                           l, r, r, l);
        }
        // One soft highlight, well off-centre, so the frost has a gradient to pick
        // up instead of an even field.
        for (int k = 6; k >= 1; --k) {
            ui.dl->AddCircleFilled(ImVec2(W * 0.28f, H * 0.22f), 40.0f * k,
                                   IM_COL32(120, 140, 255, 8), 64);
        }
    }

    // ---- window ---------------------------------------------------------
    Rect win(24, 24, W - 48, H - 48);
    ui.Shadow(win, 30.0f, 22.0f, 0.42f, 12.0f);
    // Frosted window: the measured material with a light lift, so the backdrop's
    // structure survives but is defocused and lifted the way the runtime does it.
    const Rgba windowLift(0.52f, 0.52f, 0.55f);
    ui.Glass(win, 30.0f, Rgba(1, 1, 1, 1), 1.0f, false, nullptr, &windowLift);
    ui.StrokeRounded(Rect(win.x + 0.5f, win.y + 0.5f, win.w - 1, win.h - 1), 29.5f,
                     Rgba(1, 1, 1, 0.34f), 1.0f);

    // ---- title bar ------------------------------------------------------
    {
        const float ty = win.y + 26.0f;
        const Rgba red(1.00f, 0.37f, 0.34f), amber(1.00f, 0.74f, 0.18f), green(0.16f, 0.78f, 0.25f);
        float x = win.x + 22.0f;
        for (int i = 0; i < 3; ++i) {
            ui.dl->AddCircleFilled(ImVec2(x, ty), 6.5f,
                                   i == 0 ? red.U32() : i == 1 ? amber.U32() : green.U32(), 24);
            x += 20.0f;
        }
        ui.TextCentered(ui.fonts.semibold13, 13.0f, Rect(win.x, win.y + 14, win.w, 24),
                        "Settings Â· SwiftUI 27 port", Rgba(0.15f, 0.15f, 0.17f, 0.72f));
        ui.Line(win.x + 1, win.y + 50.5f, win.x + win.w - 1, win.y + 50.5f,
                Rgba(0, 0, 0, 0.10f), 1.0f);
    }

    // ---- the ported page, hosted ---------------------------------------
    const float pageW = 402.0f, pageH = 874.0f;
    const float px = win.x + (win.w - pageW) * 0.5f;   // 16 pt margin with the tighter frame
    const float py = win.y + 64.0f;
    {
        const int mark = ui.VtxMark();
        // The page is drawn in its own coordinates. Translating the vertices it
        // produced moves everything it drew, icon atlas blits included, and the
        // page origin tells the lens pass where its panels landed (UI::Glass).
        ui.originX = px;
        ui.originY = py;
        // The page hit-tests in its own coordinates, so the pointer has to arrive in them.
        // Without this the page was hosted but inert: every control compared the screen
        // pointer against page coordinates and nothing inside the window ever responded.
        const ImVec2 savedMouse = ui.mouse;
        ui.mouse = ImVec2(savedMouse.x - px, savedMouse.y - py);
        ui.clipOffsetX = px;
        ui.clipOffsetY = py;
        IosSettings(ui, g_iosSettings);
        ui.mouse = savedMouse;
        ui.clipOffsetX = 0.0f;
        ui.clipOffsetY = 0.0f;
        ui.originX = 0.0f;
        ui.originY = 0.0f;
        ui.VtxTranslate(mark, px, py);
        // window's rounded rim read as a panel rather than as a mistake.
        ui.dl->AddRect(ImVec2(px - 0.5f, py - 0.5f), ImVec2(px + pageW + 0.5f, py + pageH + 0.5f),
                       IM_COL32(0, 0, 0, 26), 0.0f, 0, 1.0f);
    }

    ui.TextCentered(ui.fonts.regular11, 11.0f,
                    Rect(win.x, win.y + win.h - 26, win.w, 18),
                    "Ported from the iOS 27 runtime: measured, not guessed",
                    Rgba(0.15f, 0.15f, 0.17f, 0.42f));
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    // Parse --shot path.
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        for (int i = 0; i < argc; ++i) {
            if (wcscmp(argv[i], L"--shot") == 0 && i + 1 < argc) {
                char buf[MAX_PATH * 2] = {};
                WideCharToMultiByte(CP_UTF8, 0, argv[i + 1], -1, buf, sizeof(buf), nullptr, nullptr);
                g_shotPath = buf;
            }
            if (wcscmp(argv[i], L"--ios") == 0) g_iosMode = true;
            if (wcscmp(argv[i], L"--dark") == 0) g_darkMode = true;
            if (wcscmp(argv[i], L"--auto-toggle") == 0) g_autoToggle = true;
            if (wcscmp(argv[i], L"--auto-push") == 0) g_autoPush = true;
            if (wcscmp(argv[i], L"--auto-sheet") == 0) g_autoSheet = true;
            if (wcscmp(argv[i], L"--auto-flow") == 0) g_autoFlow = true;
            if (wcscmp(argv[i], L"--hover-row") == 0 && i + 1 < argc) g_hoverRow = _wtoi(argv[i + 1]);
            if (wcscmp(argv[i], L"--auto-drag") == 0 && i + 1 < argc) g_autoDrag = (float)_wtof(argv[i + 1]);
            if (wcscmp(argv[i], L"--auto-swipe") == 0) g_autoSwipe = true;
            if (wcscmp(argv[i], L"--bridge-selftest") == 0) g_bridgeSelftest = true;
            if (wcscmp(argv[i], L"--opal-bridge") == 0) g_bridgeSelftest = true;  // render path not ready
            if (wcscmp(argv[i], L"--auto-scroll") == 0) g_autoScroll = true;
            if (wcscmp(argv[i], L"--auto-slider") == 0) g_autoSlider = true;
            if (wcscmp(argv[i], L"--type") == 0 && i + 1 < argc) {
                char buf[128] = {};
                WideCharToMultiByte(CP_UTF8, 0, argv[i + 1], -1, buf, sizeof(buf) - 1, nullptr, nullptr);
                g_typeText = buf;
            }
            if (wcscmp(argv[i], L"--click-at") == 0 && i + 1 < argc) g_clickAt = (float)_wtof(argv[i + 1]);
            if (wcscmp(argv[i], L"--click") == 0 && i + 2 < argc) {
                g_clickX = (float)_wtof(argv[i + 1]);
                g_clickY = (float)_wtof(argv[i + 2]);
            }
            if (wcscmp(argv[i], L"--flow-sheet-only") == 0) g_flowSheetOnly = true;
            if (wcscmp(argv[i], L"--sheet-log") == 0 && i + 1 < argc) {
                char b4[MAX_PATH * 2] = {};
                WideCharToMultiByte(CP_UTF8, 0, argv[i + 1], -1, b4, sizeof(b4), nullptr, nullptr);
                g_sheetLogPath = b4;
            }
            if (wcscmp(argv[i], L"--nav-log") == 0 && i + 1 < argc) {
                char b3[MAX_PATH * 2] = {};
                WideCharToMultiByte(CP_UTF8, 0, argv[i + 1], -1, b3, sizeof(b3), nullptr, nullptr);
                g_navLogPath = b3;
            }
            if (wcscmp(argv[i], L"--show-blur") == 0) g_showBlur = true;
            if (wcscmp(argv[i], L"--show-scene") == 0) g_showScene = true;
            if (wcscmp(argv[i], L"--no-blur") == 0) g_noBlur = true;
            if (wcscmp(argv[i], L"--anim-log") == 0 && i + 1 < argc) {
                char b2[MAX_PATH * 2] = {};
                WideCharToMultiByte(CP_UTF8, 0, argv[i + 1], -1, b2, sizeof(b2), nullptr, nullptr);
                g_animLogPath = b2;
            }
            if (wcscmp(argv[i], L"--shot-at") == 0 && i + 1 < argc) g_shotAt = (float)_wtof(argv[i + 1]);
            if (wcscmp(argv[i], L"--screen") == 0 && i + 1 < argc) {
                if (wcscmp(argv[i + 1], L"controls") == 0) g_iosScreen = 1;
                if (wcscmp(argv[i + 1], L"glass") == 0) g_iosScreen = 2;
                if (wcscmp(argv[i + 1], L"refraction") == 0) g_iosScreen = 3;
                if (wcscmp(argv[i + 1], L"nav") == 0) g_iosScreen = 4;
                if (wcscmp(argv[i + 1], L"sheet") == 0) g_iosScreen = 5;
                if (wcscmp(argv[i + 1], L"flow") == 0) g_iosScreen = 6;
            }
        }
        LocalFree(argv);
    }
    if (g_iosMode) { g_width = 402; g_height = 874; }
    else { g_width = 482; g_height = 992; }   // host window: the page plus a 16 pt margin

    WNDCLASSEXW wc = { sizeof(wc), CS_OWNDC, WndProc, 0, 0, inst,
        LoadIcon(nullptr, IDI_APPLICATION), LoadCursor(nullptr, IDC_ARROW),
        nullptr, nullptr, L"SwiftUiPort", nullptr };
    RegisterClassExW(&wc);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"SwiftUI Port",
        WS_POPUP, 40, 40, g_width, g_height, nullptr, nullptr, inst, nullptr);
    // Screenshot runs render on WARP and never touch a swap chain or the display
    // driver: the box this is developed on has proven able to hang the desktop
    // when several hardware-accelerated instances present at once, and a capture
    // does not need the GPU anyway. Interactive runs still use the hardware path.
    if (!g_shotPath.empty()) {
        if (!CreateSoftwareDevice(g_dx, g_width, g_height)) return 1;
    } else if (!backend::InitD3D11(g_dx, hwnd, g_width, g_height, false)) {
        return 1;
    }
    if (!CreateSuperSample(g_dx.device, g_width, g_height))
        fprintf(stderr, "supersample targets unavailable, falling back to 1x\n");
    if (g_bridgeSelftest) {
        // The the framework host bridge, driven end to end: a whole ported page is drawn
        // through it with Opal's blurred backdrop as the material's frost, then saved so
        // the bridge's output can be compared against the port's direct path.
        //
        // The ImGui context has to exist first - the first attempt called NewFrame()
        // before CreateContext() and faulted immediately.
        ImGui::CreateContext();
        ImGuiIO& bio = ImGui::GetIO();
        bio.IniFilename = nullptr;
        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(g_dx.device, g_dx.context);

        // The bridge is sized to the *page*, not the host window: the ported pages are laid
        // out for a 402 x 874 screen, and a 520 x 1020 bridge only captured a crop of them.
        const int kPageW = 402, kPageH = 874;
        swui::Bridge bridge;
        const bool ok = bridge.Init(g_dx.device, kPageW, kPageH);
        fprintf(stderr, "bridge: init=%d %dx%d\n", (int)ok, kPageW, kPageH);
        if (!ok) return 3;

        ImFontConfig cfg2; cfg2.FontDataOwnedByAtlas = false;
        auto addFont = [&](const unsigned int* data, unsigned int size, float px) {
            ImFontConfig fc = cfg2;
            fc.GlyphExtraAdvanceX = 0.5f * (px / 17.0f) * kTextScale;
            return bio.Fonts->AddFontFromMemoryTTF((void*)data, size, px * kTextScale, &fc);
        };
        sw::Fonts fonts;
        fonts.regular11 = addFont(sfpro_font_data, sfpro_font_size, 11.0f);
        fonts.regular12 = addFont(sfpro_font_data, sfpro_font_size, 12.0f);
        fonts.regular13 = addFont(sfpro_font_data, sfpro_font_size, 13.0f);
        fonts.regular14 = addFont(sfpro_font_data, sfpro_font_size, 14.0f);
        fonts.regular15 = addFont(sfpro_font_data, sfpro_font_size, 15.0f);
        fonts.regular17 = addFont(sfpro_font_data, sfpro_font_size, 17.0f);
        fonts.semibold11 = addFont(sfpro_semibold_data, sfpro_semibold_size, 11.0f);
        fonts.semibold13 = addFont(sfpro_semibold_data, sfpro_semibold_size, 13.0f);
        fonts.semibold15 = addFont(sfpro_semibold_data, sfpro_semibold_size, 15.0f);
        fonts.semibold17 = addFont(sfpro_semibold_data, sfpro_semibold_size, 17.0f);
        fonts.semibold22 = addFont(sfpro_semibold_data, sfpro_semibold_size, 22.0f);
        fonts.semibold22tight = fonts.semibold22;
        fonts.semibold28 = addFont(sfpro_semibold_data, sfpro_semibold_size, 28.0f);
        fonts.semibold34 = addFont(sfpro_semibold_data, sfpro_semibold_size, 34.0f);
        // The six role faces, without which the page draws through null fonts (this is
        // exactly what crashed the first end-to-end attempt, inside the account card).
        {
            ImFontConfig fr = cfg2;
            fr.GlyphExtraAdvanceX = -0.44f;
            fonts.rowLabel17 = bio.Fonts->AddFontFromMemoryTTF((void*)sfp_text_regular_data,
                sfp_text_regular_size, 17.0f * kTextScale * 1.0085f, &fr);
            ImFontConfig ft = cfg2;
            ft.GlyphExtraAdvanceX = 0.0f;
            fonts.titleBold34 = bio.Fonts->AddFontFromMemoryTTF((void*)sfp_display_bold_data,
                sfp_display_bold_size, 34.0f * kTextScale * 1.0085f, &ft);
            ImFontConfig f13 = cfg2;
            f13.GlyphExtraAdvanceX = -0.44f * (13.0f / 17.0f);
            fonts.segLabel13 = bio.Fonts->AddFontFromMemoryTTF((void*)sfp_text_semibold_data,
                sfp_text_semibold_size, 13.0f * kTextScale * 1.0085f, &f13);
            ImFontConfig fat = cfg2;
            fat.GlyphExtraAdvanceX = -0.6f;
            fonts.accountTitle20 = bio.Fonts->AddFontFromMemoryTTF((void*)sfp_text_bold_data,
                sfp_text_bold_size, 20.0f * kTextScale, &fat);
            ImFontConfig fs14 = cfg2;
            fs14.GlyphExtraAdvanceX = -0.36f;
            fonts.subLabel14 = bio.Fonts->AddFontFromMemoryTTF((void*)sfp_text_regular_data,
                sfp_text_regular_size, 14.0f * kTextScale * 1.0085f, &fs14);
        }
        bio.Fonts->Build();

        bridge.UseFonts(&fonts);
        bridge.UseTheme(sw::Theme::Light());
        if (!g_rowMasks.Init(g_dx.device)) fprintf(stderr, "bridge: row masks unavailable");

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        // A target the size of the page at 3x, so the saved frame is directly comparable
        // with the iOS captures (1206 x 2622).
        D3D11_TEXTURE2D_DESC pd = {};
        pd.Width = (UINT)(kPageW * kSuperSample);
        pd.Height = (UINT)(kPageH * kSuperSample);
        pd.MipLevels = 1;
        pd.ArraySize = 1;
        pd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        pd.SampleDesc.Count = 1;
        pd.Usage = D3D11_USAGE_DEFAULT;
        pd.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        ID3D11Texture2D* pageTex = nullptr;
        ID3D11RenderTargetView* pageRTV = nullptr;
        g_dx.device->CreateTexture2D(&pd, nullptr, &pageTex);
        if (pageTex) g_dx.device->CreateRenderTargetView(pageTex, nullptr, &pageRTV);

        bio.DisplayFramebufferScale = ImVec2((float)kSuperSample, (float)kSuperSample);
        ImGui::NewFrame();
        bridge.BeginFrame(g_dx.context, nullptr, kPageW, kPageH, 1.0f / 60.0f,
                          ImVec2(-100.0f, -100.0f), false, false, false);
        bridge.ui().masks = &g_rowMasks;
        sw::IosSettings(bridge.ui(), g_iosSettings);
        ImGui::Render();
        const float clear2[4] = { 0.949f, 0.949f, 0.969f, 1.0f };
        if (pageRTV) {
            g_dx.context->OMSetRenderTargets(1, &pageRTV, nullptr);
            D3D11_VIEWPORT pv = { 0, 0, (float)(kPageW * kSuperSample),
                                  (float)(kPageH * kSuperSample), 0, 1 };
            g_dx.context->RSSetViewports(1, &pv);
            g_dx.context->ClearRenderTargetView(pageRTV, clear2);
        }
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        bridge.EndFrame(clear2);
        if (!g_shotPath.empty() && pageTex) {
            std::string hi = g_shotPath;
            size_t dot = hi.find_last_of('.');
            hi = (dot == std::string::npos) ? hi + "@3x" : hi.substr(0, dot) + "@3x" + hi.substr(dot);
            SavePng(hi, g_dx.device, g_dx.context, g_dx.swapChain,
                    kPageW * kSuperSample, kPageH * kSuperSample, pageTex);
        }
        if (pageTex) pageTex->Release();
        if (pageRTV) pageRTV->Release();
        bridge.Shutdown();
        return 0;
    }
    // ---- ImGui + the calibrated font set ---------------------------------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_dx.device, g_dx.context);

    ImFontConfig cfg; cfg.FontDataOwnedByAtlas = false;
    // GlyphExtraAdvanceX is the build-time equivalent of letter spacing: the runtime
    // applies its tracking table at layout time, which ImGui cannot read.
    auto add = [&](const unsigned int* data, unsigned int size, float px) {
        ImFontConfig fc = cfg;
        fc.GlyphExtraAdvanceX = 0.5f * (px / 17.0f) * kTextScale;
        return io.Fonts->AddFontFromMemoryTTF((void*)data, size, px * kTextScale, &fc);
    };
    Fonts fonts;
    fonts.regular11 = add(sfpro_font_data, sfpro_font_size, 11.0f);
    fonts.regular12 = add(sfpro_font_data, sfpro_font_size, 12.0f);
    fonts.regular13 = add(sfpro_font_data, sfpro_font_size, 13.0f);
    fonts.regular14 = add(sfpro_font_data, sfpro_font_size, 14.0f);
    fonts.regular15 = add(sfpro_font_data, sfpro_font_size, 15.0f);
    fonts.regular17 = add(sfpro_font_data, sfpro_font_size, 17.0f);
    fonts.semibold11 = add(sfpro_semibold_data, sfpro_semibold_size, 11.0f);
    fonts.semibold13 = add(sfpro_semibold_data, sfpro_semibold_size, 13.0f);
    fonts.semibold15 = add(sfpro_semibold_data, sfpro_semibold_size, 15.0f);
    fonts.semibold17 = add(sfpro_semibold_data, sfpro_semibold_size, 17.0f);
    fonts.semibold22 = add(sfpro_semibold_data, sfpro_semibold_size, 22.0f);
    {
        ImFontConfig fc2 = cfg;
        fc2.GlyphExtraAdvanceX = 0.0f;   // SF Pro Display carries no tracking
        fonts.semibold22tight = io.Fonts->AddFontFromMemoryTTF(
            (void*)sfpro_semibold_data, sfpro_semibold_size, 22.0f * kTextScale, &fc2);
    }
    fonts.semibold28 = add(sfpro_semibold_data, sfpro_semibold_size, 28.0f);
    fonts.semibold34 = add(sfpro_semibold_data, sfpro_semibold_size, 34.0f);
    {
        // The row labels are the one place the family switch is a pure win: they are
        // left-aligned, so SF Pro Text's different advances shift no origin, and its
        // glyphs match the runtime measurably better (rows 2.544 -> 2.391). Centred
        // strings keep the calibrated face, where a width difference would move them.
        ImFontConfig fr = cfg;
        // -0.44 pt/glyph. The longest label measures 207.0 against the runtime's 209.67,
        // but loosening to -0.33 to match it measured rows 2.139 -> 4.630: the tracking
        // shifts every glyph in the run, so tuning it for one label breaks the others.
        fr.GlyphExtraAdvanceX = -0.44f;
        fonts.rowLabel17 = io.Fonts->AddFontFromMemoryTTF((void*)sfp_text_regular_data,
            sfp_text_regular_size, 17.0f * kTextScale * 1.0085f, &fr);
        // The 34 pt titles with the correct family (SF Pro Display Semibold) but the
        // synthetic weight the fitted offsets expect.
        // Identified by Tools/_font_ident.py: SF Pro Display Bold at 34 pt with no
        // tracking matches the runtime's title advance (162.00 against 161.67) and
        // weight (0.4147 against 0.4135); semibold plus a strike matched neither.
        ImFontConfig ft = cfg;
        ft.GlyphExtraAdvanceX = 0.0f;
        fonts.titleBold34 = io.Fonts->AddFontFromMemoryTTF((void*)sfp_display_bold_data,
            sfp_display_bold_size, 34.0f * kTextScale * 1.0085f, &ft);
        fonts.titleSemibold34 = io.Fonts->AddFontFromMemoryTTF((void*)sfp_display_semibold_data,
            sfp_display_semibold_size, 34.0f * kTextScale * 1.0085f, &ft);
        ImFontConfig f13 = cfg;
        f13.GlyphExtraAdvanceX = -0.44f * (13.0f / 17.0f);
        fonts.segLabel13 = io.Fonts->AddFontFromMemoryTTF((void*)sfp_text_semibold_data,
            sfp_text_semibold_size, 13.0f * kTextScale * 1.0085f, &f13);
        // Identified from the ink, not guessed: the account block's 20 pt title matches
        // SF Pro Text Bold at 20 pt with -0.8 pt tracking on both advance (136.33 against
        // 136.67) and stroke fill (0.3301 against 0.3307).
        ImFontConfig fat = cfg;
        fat.GlyphExtraAdvanceX = -0.6f;
        fonts.accountTitle20 = io.Fonts->AddFontFromMemoryTTF((void*)sfp_text_bold_data,
            sfp_text_bold_size, 20.0f * kTextScale, &fat);
        ImFontConfig fs14 = cfg;
        fs14.GlyphExtraAdvanceX = -0.36f;
        fonts.subLabel14 = io.Fonts->AddFontFromMemoryTTF((void*)sfp_text_regular_data,
            sfp_text_regular_size, 14.0f * kTextScale * 1.0085f, &fs14);
    }
    io.Fonts->Build();
    if (!g_icons.Init(g_dx.device)) { /* icons are optional, the port still runs */ }
    if (!g_rowMasks.Init(g_dx.device))
        fprintf(stderr, "row glyph masks unavailable, falling back to atlas symbols\n");
    {
        // The scene capture runs at frame density (3x) so a clear glass material can sample a
// sharp backdrop; the blur targets stay at 1/3, which is resolution-independent because
// the blur shader works in UV space.
bool glassOk = g_glassPass.Init(g_dx.device, g_width, g_height, 3, kSuperSample);
        FILE* gf = nullptr;
        if (fopen_s(&gf, "glass_status.txt", "w") == 0 && gf) {
            fprintf(gf, "init=%d ready=%d lensSRV=%p blurSRV=%p\n", (int)glassOk,
                    (int)g_glassPass.Ready(), (void*)g_glassPass.LensSRV(),
                    (void*)g_glassPass.BlurSRV());
            fclose(gf);
        }
    }
    ImGui_ImplDX11_CreateDeviceObjects();

    if (!g_iosMode) {
        g_wallpaper = CreateWallpaperSRV(g_dx.device, g_width, g_height);
        g_glass.SetBgMode(8);
        g_glass.SetBackgroundImage(g_wallpaper);
        g_glass.SetForceOpaqueBackground(true);
        g_glass.SetPresentBlurredBackground(false);
        g_glass.SetDownsampleBlur(false);
        g_glass.SetBlurPasses(2);
        g_glass.SetBlurRadius(26.0f);
        // Opal renders at the size it is initialised with, so it keeps the window's 1x
        // size and its frame is blitted into the 3x frame as the backdrop; the ported
        // panels then sample its blur. Initialising it at 3x instead left the blur SRV
        // null and every panel vanished (PROGRESS rounds 24-25).
        g_glassReady = g_glass.Init(g_dx.device, g_width, g_height);
        if (g_glassReady && !CreateOpalTarget(g_dx.device, g_width, g_height)) g_glassReady = false;
        g_useOpalGlass = g_glassReady;
        if (!g_glassReady) return 1;
    }

    if (!g_lowTex) ShowWindow(hwnd, SW_SHOW);   // capture runs stay invisible
    auto start = std::chrono::high_resolution_clock::now();
    float elapsed = 0.0f;
    int frame = 0;
    MSG msg = {};
    while (g_running) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) g_running = false;
        }
        if (!g_running) break;
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration<float>(now - start).count();
        start = now;
        if (dt > 0.05f) dt = 0.016f;
        // A capture advances at a fixed 60 Hz: the run must not depend on how fast
        // the software rasteriser happens to be, and the animation timeline has to
        // be reproducible to diff against the runtime's samples.
        if (g_lowTex) dt = 1.0f / 60.0f;
        if (++frame > 20000) break;
        elapsed += dt;
        if (g_autoToggle && !g_toggled && elapsed >= 1.0f) {
            g_gallery.on = !g_gallery.on;   // same spring the click drives
            g_toggled = true;
            g_toggleTime = elapsed;
            if (!g_animLogPath.empty() && !g_animLog) {
                fopen_s(&g_animLog, g_animLogPath.c_str(), "w");
                if (g_animLog) fprintf(g_animLog, "t,width,x,spring,lead,trail\n");
            }
        }

        if (g_autoPush && !g_pushed && elapsed >= 1.0f) {
            g_navPush.Push();
            g_pushed = true;
            g_pushTime = elapsed;
            if (!g_navLogPath.empty() && !g_navLog) {
                fopen_s(&g_navLog, g_navLogPath.c_str(), "w");
                if (g_navLog) fprintf(g_navLog, "t,progress,dx_root,dx_detail\n");
            }
        }
        // The port's own transition curve, in the same shape the runtime samples
        // produce, so the two can be fitted with the same tool.
        if (g_navLog && g_pushed) {
            float t = elapsed - g_pushTime;
            float p = g_navPush.progress();
            fprintf(g_navLog, "%.4f,%.4f,%.3f,%.3f\n", t, p,
                    -nav::Metrics::kWidth * nav::Metrics::kParallax * p,
                    nav::Metrics::kWidth * (1.0f - p));
            fflush(g_navLog);
        }

        if (g_autoFlow && g_iosScreen == 6) flow::Automate(elapsed, g_navPush, g_sheet, g_flow, g_flowSheetOnly);
        if (g_autoSheet && g_iosScreen == 5) {
            if (g_sheetStage == 0 && elapsed >= 1.0f) {
                g_sheet.Present();
                g_sheetStage = 1;
                g_sheetStart = elapsed;
                if (!g_sheetLogPath.empty() && !g_sheetLog) {
                    fopen_s(&g_sheetLog, g_sheetLogPath.c_str(), "w");
                    if (g_sheetLog) fprintf(g_sheetLog, "stage,t,progress,dy\n");
                }
            } else if (g_sheetStage == 1 && elapsed >= 2.4f) {
                g_sheet.Dismiss();
                g_sheetStage = 2;
                g_sheetStart = elapsed;
            }
        }
        // The port's own sheet curve, in the shape the runtime samples produce, so the
        // two can be fitted by the same tool.
        if (g_sheetLog && g_sheetStage >= 1) {
            const float p = g_sheet.progress();
            fprintf(g_sheetLog, "%d,%.4f,%.4f,%.3f\n", g_sheetStage, elapsed - g_sheetStart, p,
                    (874.0f - sheet::Metrics::kTop) * (1.0f - p));
            fflush(g_sheetLog);
        }
        // ---- glass frame: two UI passes -------------------------------------
        // Pass 1 renders the scene into an offscreen target (the blur source).
        // Pass 2 renders the visible frame, where each glass panel samples the
        // blurred scene. Springs and input only advance in pass 1, so the two
        // passes always draw identical state.
        auto buildScene = [&](UI& ui, bool interactive, ID3D11ShaderResourceView* blur,
                              float passDt) {
            ImGuiIO& in = ImGui::GetIO();
            ui.dl = ImGui::GetBackgroundDrawList();
            ui.fonts = fonts;
            ui.theme = g_darkMode ? Theme::Dark() : Theme::Light();
            ui.icons = &g_icons;
            ui.masks = &g_rowMasks;
            ui.blurSRV = blur;
            ui.suppressGlass = (blur == nullptr);
            ui.panelSink = interactive ? &g_panels : nullptr;
            ui.lensSRV = (interactive || g_useOpalGlass) ? nullptr : g_glassPass.LensSRV();
            ui.sharpSRV = g_useOpalGlass ? nullptr : g_glassPass.SceneSRV();
            ui.screenW = (float)g_width;
            ui.screenH = (float)g_height;
            ui.time = elapsed;
            ui.dt = passDt;
            ui.mouse = in.MousePos;
            ui.mouseDown = (interactive || g_autoDrag > 0.0f || g_autoSwipe || g_autoScroll || g_autoSlider || g_clickX >= 0.0f) && ImGui::IsMouseDown(0);
            ui.mousePressed = (interactive || g_autoDrag > 0.0f || g_autoSwipe || g_autoScroll || g_autoSlider || g_clickX >= 0.0f) && ImGui::IsMouseClicked(0);
            ui.mouseReleased = (interactive || g_autoDrag > 0.0f || g_autoSwipe || g_autoScroll || g_autoSlider || g_clickX >= 0.0f) && ImGui::IsMouseReleased(0);
            // A capture run has no pointer, so the row to hold highlighted comes from
            // the command line instead of the mouse.
            HoverRowOverride() = g_hoverRow;
            if (g_iosMode) {
                if (g_iosScreen == 1) IosControls(ui, g_gallery);
                else if (g_iosScreen == 2) IosGlassDemo(ui, g_glassDemo);
                else if (g_iosScreen == 3) IosRefractionChart(ui, g_refraction);
                else if (g_iosScreen == 4) nav::Screen(ui, g_navPush, g_iosSettings, passDt);
                else if (g_iosScreen == 5) sheet::Screen(
                    ui, g_sheet, passDt, [&](UI& u) {
                        bool want = false;
                        DrawSheetHost(u, g_sheetHost, want);
                        if (want) g_sheet.Present();
                    });
                else if (g_iosScreen == 6) flow::Screen(ui, g_navPush, g_sheet, g_iosSettings, passDt);
                else IosSettings(ui, g_iosSettings);
            } else {
                DrawSettings(ui);
            }
        };

        const float clear[4] = { 0.949f, 0.949f, 0.969f, 1.0f };

        g_panels.clear();
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        io.DisplayFramebufferScale = ImVec2((float)kSuperSample, (float)kSuperSample);   // pass 1 feeds the 3x scene capture
        ImGui::NewFrame();
        {
            UI ui;
            buildScene(ui, true, nullptr, dt);
        }
        // Sample the port's own control animation so it can be diffed against the
        // runtime's presentation curve exactly like the layout is diffed.
        if (g_animLog && g_toggled) {
            fprintf(g_animLog, "%.4f,%.3f,%.3f,%.4f\n", elapsed - g_toggleTime,
                    g_gallery.toggle.lastW, g_gallery.toggle.lastX, g_gallery.toggle.lastT);
            fflush(g_animLog);
        }
        ImGui::Render();
        g_glassPass.Begin(g_dx.context, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        if (!g_useOpalGlass) g_glassPass.Blur(g_dx.context, g_noBlur ? 0 : 2);

        // Lens pass: stamp every recorded panel with the SDF shader, so pass 2
        // composites a genuinely refracted, tinted, rim-lit panel.
        {
            FILE* gf = nullptr;
            if (fopen_s(&gf, "glass_status.txt", "w") == 0 && gf) {
                fprintf(gf, "panels=%zu lensSRV=%p ready=%d\n", g_panels.size(),
                        (void*)g_glassPass.LensSRV(), (int)g_glassPass.Ready());
                for (size_t i = 0; i < g_panels.size() && i < 6; ++i) {
                    const GlassPanel& p = g_panels[i];
                    fprintf(gf, "  panel %zu: %.1f,%.1f %.1fx%.1f r=%.1f a=%.2f\n",
                            i, p.r.x, p.r.y, p.r.w, p.r.h, p.radius, p.tint.a);
                }
                fclose(gf);
            }
        }
        if (!g_panels.empty()) {
            g_glassPass.BeginLens(g_dx.context);
            const Theme& th = g_darkMode ? Theme::Dark() : Theme::Light();
            for (const GlassPanel& p : g_panels) {
                if (p.radius < 1.0f) continue;
                // A clear panel refracts the sharp capture, a frosted one the blur.
                g_glassPass.BindLensSource(g_dx.context, p.clear);
                float tint[4] = { p.tint.r, p.tint.g, p.tint.b, p.tint.a * p.opacity };
                float lift[4] = { p.lift.r, p.lift.g, p.lift.b, 1.0f };
                // Measured: 5.3 pt at 3.5 pt inside a 40 pt-radius rim -> A = 0.135 r.
                float refract = p.radius * 0.135f * p.refractScale;
                // The rim's alpha, measured: 0.47 where the normal is vertical in light
                // and 0.22 in dark. The hairline's slope against |normal.y| is what makes
                // a horizontal edge's line vanish in dark appearance.
                float rim = (g_darkMode ? 0.22f : 0.47f) * p.rimScale;
                float shadow = p.shadow ? (g_darkMode ? 0.42f : 0.20f) : 0.0f;
                g_glassPass.DrawPanel(g_dx.context, p.r.x, p.r.y, p.r.w, p.r.h,
                                      p.radius, tint, refract, rim, shadow, 16.0f, 0.0f,
                                      p.liftMode ? lift : nullptr, p.coverage, p.ring, p.vibrancy, p.frost,
                                      g_darkMode ? 0.84f : 0.46f, p.edge, p.edgeInset);
            }
        }

        // the framework paints its backdrop and blur into its own 1x target *before* the
        // UI is built, so the scene can blit it and the panels can sample its blur.
        if (g_useOpalGlass && g_glassReady) g_glass.BeginFrame(g_dx.context, g_opalRTV, dt);

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        io.DisplayFramebufferScale = ImVec2((float)kSuperSample, (float)kSuperSample);
        // Capture automation: a synthesized grabber drag, the only way to exercise the
        // sheet's interactive path without a pointer.
        // One synthesized press at a point, for exercising the controls in a capture.
        if (!g_typeText.empty()) {
            // Typed characters, one per frame from 1.80 s, so the field's focus and caret
            // are exercised the way a person would exercise them rather than by writing
            // into its buffer directly.
            static size_t typed = 0;
            if (elapsed >= 1.80f) {
                const size_t want = (size_t)((elapsed - 1.80f) / 0.05f) + 1;
                if (typed < g_typeText.size() && typed < want) {
                    io.AddInputCharacter((unsigned int)g_typeText[typed]);
                    ++typed;
                }
            }
        }
        if (g_autoSlider) {
            // Grabs the thumb where it sits (value 0.62 puts its centre at x=236) and
            // drags it right, one press for the whole gesture.
            static bool sDown = false, sUp = false;
            const float t0 = 1.60f, dur = 0.30f;
            if (elapsed >= t0 && elapsed <= t0 + dur) {
                const float k = (elapsed - t0) / dur;
                io.AddMousePosEvent(236.0f + 70.0f * k, 379.0f);
                if (!sDown) { io.AddMouseButtonEvent(0, true); sDown = true; }
            } else if (elapsed > t0 + dur && elapsed <= t0 + dur + 0.06f) {
                if (!sUp) { io.AddMouseButtonEvent(0, false); sUp = true; }
            }
        }
        if (g_autoScroll) {
            // Push lands at 1.0 s (--auto-flow), then a vertical drag scrolls the list.
            if (elapsed >= 1.60f && elapsed < 2.00f) {
                const float k = (elapsed - 1.60f) / 0.40f;
                io.AddMousePosEvent(200.0f, 600.0f - 120.0f * k);
                io.AddMouseButtonEvent(0, true);
            } else if (elapsed >= 2.00f && elapsed < 2.06f) {
                io.AddMousePosEvent(200.0f, 480.0f);
                io.AddMouseButtonEvent(0, false);
            }
        }
        if (g_autoSwipe) {
            // Push lands at 1.0 s (--auto-flow), then a left-edge drag scrubs the pop back.
            if (elapsed >= 1.60f && elapsed < 2.00f) {
                const float k = (elapsed - 1.60f) / 0.40f;
                io.AddMousePosEvent(10.0f + 130.0f * k, 420.0f);
                io.AddMouseButtonEvent(0, true);
            } else if (elapsed >= 2.00f && elapsed < 2.06f) {
                io.AddMousePosEvent(140.0f, 420.0f);
                io.AddMouseButtonEvent(0, false);
            }
        }
        if (g_clickX >= 0.0f) {
            // Press for three frames, release for three: a one-frame window skipped any
            // widget that commits on release (the switch does, as of this round), so the
            // capture harness could not see the interaction at all.
            if (elapsed >= g_clickAt && elapsed < g_clickAt + 0.05f) {
                io.AddMousePosEvent(g_clickX, g_clickY);
                io.AddMouseButtonEvent(0, true);
            } else if (elapsed >= g_clickAt + 0.05f && elapsed < g_clickAt + 0.10f) {
                io.AddMousePosEvent(g_clickX, g_clickY);
                io.AddMouseButtonEvent(0, false);
            }
        }
        if (g_autoDrag > 0.0f) {
            // The button event is sent once per phase, not every frame: re-adding a
            // button-down makes ImGui report a fresh click each frame, which re-ran the
            // grab and reset the drag offset to zero 60 times a second (the card moved
            // 17 pt instead of 114).
            static bool downSent = false, upSent = false;
            const float t0 = 2.0f, dur = 0.35f;
            if (elapsed >= t0 && elapsed <= t0 + dur) {
                const float k = (elapsed - t0) / dur;
                io.AddMousePosEvent(201.0f, 419.0f + g_autoDrag * k);
                if (!downSent) { io.AddMouseButtonEvent(0, true); downSent = true; }
            } else if (elapsed > t0 + dur && elapsed <= t0 + dur + 0.06f) {
                if (!upSent) { io.AddMouseButtonEvent(0, false); upSent = true; }
            }
        }
        ImGui::NewFrame();
        {
            UI ui;
            buildScene(ui, false, g_useOpalGlass ? g_glass.GetBlurTexture() : g_glassPass.BlurSRV(), 0.0f);
        }
        ImGui::Render();
        if (g_ssRTV) {
            g_dx.context->OMSetRenderTargets(1, &g_ssRTV, nullptr);
            g_dx.context->ClearRenderTargetView(g_ssRTV, clear);
        } else {
            g_dx.context->OMSetRenderTargets(1, &g_dx.rtv, nullptr);
            g_dx.context->ClearRenderTargetView(g_dx.rtv, clear);
        }
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        if (g_ssRTV) BlitSuperSample(g_dx.context, g_dx.rtv, g_width, g_height);
        if (g_dx.swapChain) g_dx.swapChain->Present(1, 0);

        if (!g_shotPath.empty() && elapsed >= g_shotAt) {
            SavePng(g_shotPath, g_dx.device, g_dx.context, g_dx.swapChain, g_width, g_height, g_lowTex);
            if (g_ssTex) {
                // Native-resolution capture, so it can be compared with the real
                // 3x screenshot through exactly the same downsample.
                std::string hi = g_shotPath;
                size_t dot = hi.find_last_of('.');
                hi = (dot == std::string::npos) ? hi + "@3x"
                                                : hi.substr(0, dot) + "@3x" + hi.substr(dot);
                SavePng(hi, g_dx.device, g_dx.context, g_dx.swapChain,
                        g_width * kSuperSample, g_height * kSuperSample, g_ssTex);
            }
            g_running = false;
        }
    }

    ReleaseSuperSample();
    // g_lowRTV is g_dx.rtv, which ShutdownD3D11 releases below.
    if (g_lowTex) { g_lowTex->Release(); g_lowTex = nullptr; }

    g_rowMasks.Release();
    g_icons.Shutdown();
    g_glassPass.Shutdown();
    if (g_glassReady) g_glass.Shutdown();
    backend::SafeRelease(g_wallpaper);
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    backend::ShutdownD3D11(g_dx);
    if (hwnd) DestroyWindow(hwnd);
    CoUninitialize();
    return 0;
}
