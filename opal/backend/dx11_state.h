#pragma once
#include <d3d11.h>

namespace opal::backend {

struct DX11StateBackup {
    ID3D11RenderTargetView*   rtv[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    ID3D11DepthStencilView*   dsv            = nullptr;
    ID3D11VertexShader*       vs             = nullptr;
    ID3D11PixelShader*        ps             = nullptr;
    ID3D11GeometryShader*     gs             = nullptr;
    // ROUND 42 — full-stage coverage (the old set omitted the entire VS
    // stage + GS resources + HS/DS/CS + predication: ImGui binds its own VS
    // constant buffer every frame, and the game's next draws inherited it —
    // raid-only black-stripe corruption once ESP data flowed).
    ID3D11Buffer*             vsConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
    ID3D11ShaderResourceView* vsSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
    ID3D11SamplerState*       vsSamplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {};
    ID3D11Buffer*             gsConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
    ID3D11ShaderResourceView* gsSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
    ID3D11SamplerState*       gsSamplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {};
    ID3D11HullShader*         hs             = nullptr;
    ID3D11DomainShader*       ds             = nullptr;
    ID3D11ComputeShader*      cs             = nullptr;
    ID3D11Buffer*             csConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
    ID3D11ShaderResourceView* csSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
    ID3D11SamplerState*       csSamplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {};
    ID3D11UnorderedAccessView* csUAVs[D3D11_PS_CS_UAV_REGISTER_COUNT] = {};
    UINT                      csUAVInitialCounts[D3D11_PS_CS_UAV_REGISTER_COUNT] = {};
    ID3D11Predicate*          predicate      = nullptr;
    BOOL                      predValue      = FALSE;
    ID3D11InputLayout*        inputLayout    = nullptr;
    ID3D11Buffer*             ib             = nullptr;
    DXGI_FORMAT               ibFormat       = DXGI_FORMAT_UNKNOWN;
    UINT                      ibOffset       = 0;
    ID3D11Buffer*             vb             = nullptr;
    UINT                      vbStride       = 0;
    UINT                      vbOffset       = 0;
    D3D11_PRIMITIVE_TOPOLOGY  topology       = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    D3D11_VIEWPORT            viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
    UINT                      numViewports   = 0;
    ID3D11BlendState*         blendState     = nullptr;
    float                     blendFactor[4] = {};
    UINT                      sampleMask     = 0;
    ID3D11DepthStencilState*  depthStencilState = nullptr;
    UINT                      stencilRef     = 0;
    ID3D11RasterizerState*    rasterizerState = nullptr;
    ID3D11SamplerState*       psSamplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT] = {};
    ID3D11ShaderResourceView* psSRVs[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
    ID3D11Buffer*             psConstantBuffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT] = {};
    UINT                      numScissorRects = 0;
    D3D11_RECT                scissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
};

inline void BackupDX11State(ID3D11DeviceContext* ctx, DX11StateBackup& bk) {
    if (!ctx) return;
    ctx->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, bk.rtv, &bk.dsv);
    ctx->VSGetShader(&bk.vs, nullptr, nullptr);
    ctx->PSGetShader(&bk.ps, nullptr, nullptr);
    ctx->GSGetShader(&bk.gs, nullptr, nullptr);
    ctx->IAGetInputLayout(&bk.inputLayout);
    ctx->IAGetIndexBuffer(&bk.ib, &bk.ibFormat, &bk.ibOffset);
    ctx->IAGetVertexBuffers(0, 1, &bk.vb, &bk.vbStride, &bk.vbOffset);
    ctx->IAGetPrimitiveTopology(&bk.topology);
    bk.numViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    ctx->RSGetViewports(&bk.numViewports, bk.viewports);
    ctx->OMGetBlendState(&bk.blendState, bk.blendFactor, &bk.sampleMask);
    ctx->OMGetDepthStencilState(&bk.depthStencilState, &bk.stencilRef);
    ctx->RSGetState(&bk.rasterizerState);
    ctx->PSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, bk.psSamplers);
    ctx->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, bk.psSRVs);
    ctx->PSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, bk.psConstantBuffers);
    ctx->VSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, bk.vsConstantBuffers);
    ctx->VSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, bk.vsSRVs);
    ctx->VSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, bk.vsSamplers);
    ctx->GSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, bk.gsConstantBuffers);
    ctx->GSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, bk.gsSRVs);
    ctx->GSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, bk.gsSamplers);
    ctx->HSGetShader(&bk.hs, nullptr, nullptr);
    ctx->DSGetShader(&bk.ds, nullptr, nullptr);
    ctx->CSGetShader(&bk.cs, nullptr, nullptr);
    ctx->CSGetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, bk.csConstantBuffers);
    ctx->CSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, bk.csSRVs);
    ctx->CSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, bk.csSamplers);
    ctx->CSGetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, bk.csUAVs);
    // NOTE: CSGetUnorderedAccessViews does not return the append/consume
    // initial counts (they are write-only at bind time); restore passes the
    // zeroed array, which preserves standard (non-append) UAV bindings. An
    // append/consume UAV bound by the game across Present is vanishingly
    // rare (compute dispatches complete before Present); documented limit.
    ctx->GetPredication(&bk.predicate, &bk.predValue);
    bk.numScissorRects = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    ctx->RSGetScissorRects(&bk.numScissorRects, bk.scissorRects);
}

inline void RestoreDX11State(ID3D11DeviceContext* ctx, DX11StateBackup& bk) {
    if (!ctx) return;
    ctx->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, bk.rtv, bk.dsv);
    ctx->VSSetShader(bk.vs, nullptr, 0);
    ctx->PSSetShader(bk.ps, nullptr, 0);
    ctx->GSSetShader(bk.gs, nullptr, 0);
    ctx->IASetInputLayout(bk.inputLayout);
    ctx->IASetIndexBuffer(bk.ib, bk.ibFormat, bk.ibOffset);
    ctx->IASetVertexBuffers(0, 1, &bk.vb, &bk.vbStride, &bk.vbOffset);
    ctx->IASetPrimitiveTopology(bk.topology);
    ctx->RSSetViewports(bk.numViewports, bk.viewports);
    ctx->OMSetBlendState(bk.blendState, bk.blendFactor, bk.sampleMask);
    ctx->OMSetDepthStencilState(bk.depthStencilState, bk.stencilRef);
    ctx->RSSetState(bk.rasterizerState);
    ctx->PSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, bk.psSamplers);
    ctx->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, bk.psSRVs);
    ctx->PSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, bk.psConstantBuffers);
    ctx->VSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, bk.vsConstantBuffers);
    ctx->VSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, bk.vsSRVs);
    ctx->VSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, bk.vsSamplers);
    ctx->GSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, bk.gsConstantBuffers);
    ctx->GSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, bk.gsSRVs);
    ctx->GSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, bk.gsSamplers);
    ctx->HSSetShader(bk.hs, nullptr, 0);
    ctx->DSSetShader(bk.ds, nullptr, 0);
    ctx->CSSetShader(bk.cs, nullptr, 0);
    ctx->CSSetConstantBuffers(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT, bk.csConstantBuffers);
    ctx->CSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, bk.csSRVs);
    ctx->CSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, bk.csSamplers);
    ctx->CSSetUnorderedAccessViews(0, D3D11_PS_CS_UAV_REGISTER_COUNT, bk.csUAVs, bk.csUAVInitialCounts);
    ctx->SetPredication(bk.predicate, bk.predValue);
    ctx->RSSetScissorRects(bk.numScissorRects, bk.scissorRects);

    auto release = [](IUnknown* p) { if (p) p->Release(); };
    for (auto& r : bk.rtv)            { release(r); r = nullptr; }
    release(bk.dsv);                  release(bk.vs);
    release(bk.ps);                   release(bk.gs);
    release(bk.hs);                   release(bk.ds);
    release(bk.cs);                   release(bk.predicate);
    release(bk.inputLayout);          release(bk.ib);
    release(bk.vb);                   release(bk.blendState);
    release(bk.depthStencilState);    release(bk.rasterizerState);
    for (auto& s : bk.psSamplers)     { release(s); s = nullptr; }
    for (auto& s : bk.psSRVs)         { release(s); s = nullptr; }
    for (auto& b : bk.psConstantBuffers) { release(b); b = nullptr; }
    for (auto& b : bk.vsConstantBuffers) { release(b); b = nullptr; }
    for (auto& s : bk.vsSRVs)         { release(s); s = nullptr; }
    for (auto& s : bk.vsSamplers)     { release(s); s = nullptr; }
    for (auto& b : bk.gsConstantBuffers) { release(b); b = nullptr; }
    for (auto& s : bk.gsSRVs)         { release(s); s = nullptr; }
    for (auto& s : bk.gsSamplers)     { release(s); s = nullptr; }
    for (auto& b : bk.csConstantBuffers) { release(b); b = nullptr; }
    for (auto& s : bk.csSRVs)         { release(s); s = nullptr; }
    for (auto& s : bk.csSamplers)     { release(s); s = nullptr; }
    for (auto& u : bk.csUAVs)         { release(u); u = nullptr; }
}

} // namespace opal::backend
