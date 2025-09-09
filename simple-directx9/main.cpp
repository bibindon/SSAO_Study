#pragma comment(lib, "d3d9.lib")
#if defined(DEBUG) || defined(_DEBUG)
#pragma comment(lib, "d3dx9d.lib")
#else
#pragma comment(lib, "d3dx9.lib")
#endif

#include <d3d9.h>
#include <d3dx9.h>
#include <tchar.h>
#include <cassert>
#include <cstdio>

#define SAFE_RELEASE(p) do{ if(p){ (p)->Release(); (p)=NULL; } }while(0)

const int WIN_W = 1600;
const int WIN_H = 900;

enum ViewMode {
    MODE_0_GREEN = 0,       // 固定機能の緑
    MODE_1_SHOW_SCENE_FFP,  // RTT(Scene) を固定機能で表示
    MODE_2_SHOW_SCENE_EFX,  // RTT(Scene) を Effect(PS) で表示
    MODE_3_SHOW_DN_DIRECT,  // DepthNormal を“直接”描画（RTT経由しない）
    MODE_4_SSAO,            // SSAO 合成
    MODE_5_SHOW_DN_RTT,     // RTT から DepthNormal を表示
    MODE_6_SHOW_DEPTH_ONLY, // RTT から Depthのみ表示
};
static ViewMode g_ViewMode = MODE_0_GREEN;

// 調整パラメータ（ランタイム変更可）
static float g_Radius = 12.0f;  // Q/A で ±1
static float g_Intensity = 1.20f;  // W/S で ±0.1
static float g_BiasUser = 0.002f; // E/D で ±0.001（自動調整の下限として使う）

// D3D
LPDIRECT3D9        g_pD3D = NULL;
LPDIRECT3DDEVICE9  g_pd3d = NULL;
LPDIRECT3DSURFACE9 g_BackBuf = NULL;
LPDIRECT3DSURFACE9 g_DefaultZ = NULL;

// メッシュ
LPD3DXMESH         g_Mesh = NULL;
DWORD              g_SubsetCount = 0;

// エフェクト
LPD3DXEFFECT       g_Effect = NULL;

// RTT
LPDIRECT3DTEXTURE9 g_TexScene = NULL;
LPDIRECT3DSURFACE9 g_RTScene = NULL;

LPDIRECT3DTEXTURE9 g_TexDN = NULL;  // Depth+Normal（FP16推奨）
LPDIRECT3DSURFACE9 g_RTDN = NULL;

// RTT 用 Z
LPDIRECT3DSURFACE9 g_RTZ = NULL;

// DN が低精度（A8R8G8B8）にフォールバックしたか
static bool g_LowPrecisionDN = false;

static void UpdateTitle(HWND wnd)
{
    TCHAR buf[256];
    _stprintf_s(buf, _T("DX9 SSAO Minimal  [mode=%d]  R=%.1f  I=%.2f  B=%.4f%s"),
                (int)g_ViewMode, g_Radius, g_Intensity, g_BiasUser,
                g_LowPrecisionDN ? _T("  (DN=8bit)") : _T("  (DN=FP16)"));
    SetWindowText(wnd, buf);
}

static void DrawGreenQuadFFP()
{
    struct VtxRHWCol { float x, y, z, w; DWORD diffuse; };
    VtxRHWCol v[4] = {
        { 0.f,       0.f,       0,1, 0xFF00FF00 },
        { (float)WIN_W,0.f,     0,1, 0xFF00FF00 },
        { 0.f,       (float)WIN_H,0,1, 0xFF00FF00 },
        { (float)WIN_W,(float)WIN_H,0,1, 0xFF00FF00 },
    };
    g_pd3d->SetVertexShader(NULL);
    g_pd3d->SetPixelShader(NULL);
    g_pd3d->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    g_pd3d->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_pd3d->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_pd3d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(v[0]));
    g_pd3d->SetRenderState(D3DRS_ZENABLE, TRUE);
}

static void DrawTexQuadFFP(LPDIRECT3DTEXTURE9 tex)
{
    struct VtxRHWTex { float x, y, z, w; float u, v; };
    VtxRHWTex v[4] = {
        { 0.f,       0.f,       0,1, 0.f,0.f },
        { (float)WIN_W,0.f,     0,1, 1.f,0.f },
        { 0.f,       (float)WIN_H,0,1, 0.f,1.f },
        { (float)WIN_W,(float)WIN_H,0,1, 1.f,1.f },
    };
    g_pd3d->SetVertexShader(NULL);
    g_pd3d->SetPixelShader(NULL);
    g_pd3d->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
    g_pd3d->SetTexture(0, tex);
    g_pd3d->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    g_pd3d->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    g_pd3d->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    g_pd3d->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    g_pd3d->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_pd3d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(v[0]));
    g_pd3d->SetRenderState(D3DRS_ZENABLE, TRUE);
    g_pd3d->SetTexture(0, NULL);
}

// Effect 用：PS/サンプラを触らないクアッド
static void DrawQuadForEffect()
{
    struct VtxRHWTex { float x, y, z, w; float u, v; };
    VtxRHWTex v[4] = {
        { 0.f,       0.f,       0,1, 0.f,0.f },
        { (float)WIN_W,0.f,     0,1, 1.f,0.f },
        { 0.f,       (float)WIN_H,0,1, 0.f,1.f },
        { (float)WIN_W,(float)WIN_H,0,1, 1.f,1.f },
    };
    g_pd3d->SetVertexShader(NULL); // RHW
    g_pd3d->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
    g_pd3d->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_pd3d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(v[0]));
    g_pd3d->SetRenderState(D3DRS_ZENABLE, TRUE);
}

static bool DrawMeshWithTechnique(const char* tech,
                                  const D3DXMATRIX& W, const D3DXMATRIX& V, const D3DXMATRIX& P)
{
    D3DXMATRIX WVP = W * V * P;
    g_Effect->SetMatrix("g_matWorld", &W);
    g_Effect->SetMatrix("g_matView", &V);
    g_Effect->SetMatrix("g_matProj", &P);
    g_Effect->SetMatrix("g_matWorldViewProj", &WVP);

    if (FAILED(g_Effect->SetTechnique(tech))) return false;
    D3DXHANDLE hTech = g_Effect->GetTechniqueByName(tech);
    if (FAILED(g_Effect->ValidateTechnique(hTech))) return false;

    UINT np = 0;
    if (FAILED(g_Effect->Begin(&np, 0)) || np == 0) return false;
    for (UINT p = 0; p < np; ++p) {
        g_Effect->BeginPass(p);
        if (g_SubsetCount == 0) g_Mesh->DrawSubset(0);
        else for (DWORD i = 0; i < g_SubsetCount; ++i) g_Mesh->DrawSubset(i);
        g_Effect->EndPass();
    }
    g_Effect->End();
    return true;
}

static void CreateRTT()
{
    HRESULT hr;
    // Scene RTT
    hr = g_pd3d->CreateTexture(WIN_W, WIN_H, 1, D3DUSAGE_RENDERTARGET,
                               D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &g_TexScene, NULL);
    assert(SUCCEEDED(hr));
    g_TexScene->GetSurfaceLevel(0, &g_RTScene);

    // Depth+Normal RTT：A16B16G16R16F を試し、ダメなら A8R8G8B8
    hr = g_pd3d->CreateTexture(WIN_W, WIN_H, 1, D3DUSAGE_RENDERTARGET,
                               D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &g_TexDN, NULL);
    if (FAILED(hr)) {
        g_LowPrecisionDN = true;
        hr = g_pd3d->CreateTexture(WIN_W, WIN_H, 1, D3DUSAGE_RENDERTARGET,
                                   D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &g_TexDN, NULL);
    }
    assert(SUCCEEDED(hr));
    g_TexDN->GetSurfaceLevel(0, &g_RTDN);

    // RTT 用 Z
    hr = g_pd3d->CreateDepthStencilSurface(WIN_W, WIN_H, D3DFMT_D24S8,
                                           D3DMULTISAMPLE_NONE, 0, TRUE, &g_RTZ, NULL);
    assert(SUCCEEDED(hr));
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m)
    {
    case WM_KEYDOWN:
        if (w >= '0' && w <= '6') { g_ViewMode = (ViewMode)(w - '0'); UpdateTitle(h); }
        else if (w == 'Q') { g_Radius += 1.0f; UpdateTitle(h); }
        else if (w == 'A') { g_Radius = max(1.0f, g_Radius - 1.0f); UpdateTitle(h); }
        else if (w == 'W') { g_Intensity += 0.1f; UpdateTitle(h); }
        else if (w == 'S') { g_Intensity = max(0.1f, g_Intensity - 0.1f); UpdateTitle(h); }
        else if (w == 'E') { g_BiasUser += 0.001f; UpdateTitle(h); }
        else if (w == 'D') { g_BiasUser = max(0.0001f, g_BiasUser - 0.001f); UpdateTitle(h); }
        break;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(h, m, w, l);
}

static void InitD3D(HWND wnd)
{
    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    assert(g_pD3D);

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    pp.hDeviceWindow = wnd;

    HRESULT hr = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd,
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &g_pd3d);
    if (FAILED(hr)) {
        hr = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &g_pd3d);
        assert(SUCCEEDED(hr));
    }

    g_pd3d->GetRenderTarget(0, &g_BackBuf);
    g_pd3d->GetDepthStencilSurface(&g_DefaultZ);

    // メッシュ
    LPD3DXBUFFER mtrl = NULL;
    hr = D3DXLoadMeshFromX(_T("cube.x"), D3DXMESH_SYSTEMMEM, g_pd3d,
                           NULL, &mtrl, NULL, &g_SubsetCount, &g_Mesh);
    assert(SUCCEEDED(hr));
    SAFE_RELEASE(mtrl);

    if (!(g_Mesh->GetFVF() & D3DFVF_NORMAL)) {
        LPD3DXMESH tmp = NULL;
        g_Mesh->CloneMeshFVF(g_Mesh->GetOptions(),
                             g_Mesh->GetFVF() | D3DFVF_NORMAL, g_pd3d, &tmp);
        SAFE_RELEASE(g_Mesh);
        g_Mesh = tmp;
        D3DXComputeNormals(g_Mesh, NULL);
    }

    // エフェクト
    hr = D3DXCreateEffectFromFile(g_pd3d, _T("simple.fx"),
                                  NULL, NULL, D3DXSHADER_DEBUG, NULL, &g_Effect, NULL);
    assert(SUCCEEDED(hr));

    g_pd3d->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
    g_pd3d->SetRenderState(D3DRS_ZENABLE, TRUE);
    g_pd3d->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);

    CreateRTT();
    UpdateTitle(wnd);
}

static void Render()
{
    static float t = 0.f; t += 0.0001f;

    // 行列（LH）
    D3DXMATRIX W, V, P;
    D3DXMatrixRotationY(&W, t);
    D3DXVECTOR3 eye(6, 3, -6), at(0, 0, 0), up(0, 1, 0);
    D3DXMatrixLookAtLH(&V, &eye, &at, &up);
    const float nearZ = 0.5f, farZ = 100.0f;
    D3DXMatrixPerspectiveFovLH(&P, D3DX_PI / 4, (float)WIN_W / WIN_H, nearZ, farZ);

    // 共通パラメータ
    float nf[2] = { nearZ, farZ };
    float texel[2] = { 1.0f / WIN_W, 1.0f / WIN_H };
    g_Effect->SetFloatArray("g_NearFar", nf, 2);
    g_Effect->SetFloatArray("g_TexelSize", texel, 2);
    g_Effect->SetFloat("g_SampleRadius", g_Radius);
    g_Effect->SetFloat("g_Intensity", g_Intensity);

    // 量子化幅に応じて閾値を自動調整
    const float depthEps = g_LowPrecisionDN ? (1.0f / 255.0f) : (1.0f / 1024.0f); // 目安
    const float biasUse = max(g_BiasUser, depthEps * 1.5f);
    g_Effect->SetFloat("g_DepthEps", depthEps);
    g_Effect->SetFloat("g_Bias", biasUse);

    // ---------- 1) DepthNormal → RTT ----------
    g_pd3d->SetRenderTarget(0, g_RTDN);
    g_pd3d->SetDepthStencilSurface(g_RTZ);
    g_pd3d->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0F);
    g_pd3d->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    g_pd3d->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.f, 0);

    g_pd3d->BeginScene();
    DrawMeshWithTechnique("Tech_DepthNormal", W, V, P);
    g_pd3d->EndScene();

    // ---------- 2) Scene → RTT ----------
    g_pd3d->SetRenderTarget(0, g_RTScene);
    g_pd3d->SetDepthStencilSurface(g_RTZ);
    g_pd3d->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(90, 90, 90), 1.f, 0);

    g_pd3d->BeginScene();
    DrawMeshWithTechnique("Tech_Scene", W, V, P);
    g_pd3d->EndScene();

    // ---------- 3) BackBuffer へ ----------
    g_pd3d->SetRenderTarget(0, g_BackBuf);
    g_pd3d->SetDepthStencilSurface(g_DefaultZ);
    g_pd3d->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.f, 0);

    g_pd3d->BeginScene();
    switch (g_ViewMode)
    {
    case MODE_0_GREEN: DrawGreenQuadFFP(); break;

    case MODE_1_SHOW_SCENE_FFP: DrawTexQuadFFP(g_TexScene); break;

    case MODE_2_SHOW_SCENE_EFX:
        g_Effect->SetTexture("tScene", g_TexScene);
        g_Effect->SetTechnique("Tech_Show_Scene");
        g_pd3d->SetVertexShader(NULL);
        {
            UINT np = 0; g_Effect->Begin(&np, 0); g_Effect->BeginPass(0);
            DrawQuadForEffect();
            g_Effect->EndPass(); g_Effect->End();
        }
        break;

    case MODE_3_SHOW_DN_DIRECT:
        // RTTを使わず DepthNormal を直接描く（シェーダの出力確認）
        DrawMeshWithTechnique("Tech_DepthNormal", W, V, P);
        break;

    case MODE_4_SSAO:
        g_Effect->SetTexture("tScene", g_TexScene);
        g_Effect->SetTexture("tDepthNormal", g_TexDN);
        g_Effect->SetTechnique("Tech_SSAOCombine");
        g_pd3d->SetVertexShader(NULL);
        {
            UINT np = 0; g_Effect->Begin(&np, 0); g_Effect->BeginPass(0);
            DrawQuadForEffect();
            g_Effect->EndPass(); g_Effect->End();
        }
        break;

    case MODE_5_SHOW_DN_RTT:
        g_Effect->SetTexture("tDepthNormal", g_TexDN);
        g_Effect->SetTechnique("Tech_Show_DN");
        g_pd3d->SetVertexShader(NULL);
        {
            UINT np = 0; g_Effect->Begin(&np, 0); g_Effect->BeginPass(0);
            DrawQuadForEffect();
            g_Effect->EndPass(); g_Effect->End();
        }
        break;

    case MODE_6_SHOW_DEPTH_ONLY:
        g_Effect->SetTexture("tDepthNormal", g_TexDN);
        g_Effect->SetTechnique("Tech_Show_Depth");
        g_pd3d->SetVertexShader(NULL);
        {
            UINT np = 0; g_Effect->Begin(&np, 0); g_Effect->BeginPass(0);
            DrawQuadForEffect();
            g_Effect->EndPass(); g_Effect->End();
        }
        break;
    }
    g_pd3d->EndScene();

    g_pd3d->Present(NULL, NULL, NULL, NULL);
}

static void Cleanup()
{
    SAFE_RELEASE(g_Mesh);
    SAFE_RELEASE(g_Effect);

    SAFE_RELEASE(g_RTZ);
    SAFE_RELEASE(g_RTDN);
    SAFE_RELEASE(g_TexDN);
    SAFE_RELEASE(g_RTScene);
    SAFE_RELEASE(g_TexScene);

    SAFE_RELEASE(g_DefaultZ);
    SAFE_RELEASE(g_BackBuf);

    SAFE_RELEASE(g_pd3d);
    SAFE_RELEASE(g_pD3D);
}

int WINAPI WinMain(HINSTANCE h, HINSTANCE, LPSTR, int)
{
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.style = CS_CLASSDC; wc.lpfnWndProc = WndProc; wc.hInstance = h; wc.lpszClassName = _T("DX9_SSAO");
    RegisterClassEx(&wc);

    RECT r = { 0,0,WIN_W,WIN_H };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND wnd = CreateWindow(wc.lpszClassName, _T("DX9 SSAO Minimal"),
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                            r.right - r.left, r.bottom - r.top, NULL, NULL, h, NULL);

    InitD3D(wnd);
    ShowWindow(wnd, SW_SHOWDEFAULT); UpdateWindow(wnd);

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        else { Render(); }
    }
    Cleanup();
    UnregisterClass(wc.lpszClassName, h);
    return 0;
}
