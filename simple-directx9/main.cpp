// DX9 SSAO Minimal — study-only
#pragma comment(lib,"d3d9.lib")
#if defined(DEBUG) || defined(_DEBUG)
#pragma comment(lib,"d3dx9d.lib")
#else
#pragma comment(lib,"d3dx9.lib")
#endif

#include <d3d9.h>
#include <d3dx9.h>
#include <tchar.h>
#include <cassert>

#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p) = NULL; } } while (0)

const int WIN_W = 1600, WIN_H = 900;

// D3D
LPDIRECT3D9        g_pD3D = NULL;
LPDIRECT3DDEVICE9  g_pd3d = NULL;
LPDIRECT3DSURFACE9 g_BackBuf = NULL, g_ZDefault = NULL;

// Mesh & Effect
LPD3DXMESH   g_Mesh = NULL;
DWORD        g_SubsetCount = 0;
LPD3DXEFFECT g_Effect = NULL;

// RTTs
LPDIRECT3DTEXTURE9 g_TexScene = NULL;  LPDIRECT3DSURFACE9 g_RTScene = NULL;
LPDIRECT3DTEXTURE9 g_TexDN = NULL;  LPDIRECT3DSURFACE9 g_RTDN = NULL;
LPDIRECT3DSURFACE9 g_RTZ = NULL;

static void DrawMeshWithTech(const char* tech,
                             const D3DXMATRIX& W, const D3DXMATRIX& V, const D3DXMATRIX& P)
{
    D3DXMATRIX WVP = W * V * P;
    g_Effect->SetMatrix("g_matWorld", &W);
    g_Effect->SetMatrix("g_matView", &V);
    g_Effect->SetMatrix("g_matProj", &P);
    g_Effect->SetMatrix("g_matWorldViewProj", &WVP);

    g_Effect->SetTechnique(tech);
    UINT np = 0; g_Effect->Begin(&np, 0);
    for (UINT p = 0; p < np; ++p)
    {
        g_Effect->BeginPass(p);
        if (g_SubsetCount == 0) g_Mesh->DrawSubset(0);
        else for (DWORD i = 0; i < g_SubsetCount; ++i) g_Mesh->DrawSubset(i);
        g_Effect->EndPass();
    }
    g_Effect->End();
}

static void CreateRTT()
{
    HRESULT hr;
    // Scene color
    hr = g_pd3d->CreateTexture(WIN_W, WIN_H, 1, D3DUSAGE_RENDERTARGET,
                               D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &g_TexScene, NULL);
    assert(SUCCEEDED(hr));
    g_TexScene->GetSurfaceLevel(0, &g_RTScene);

    // Depth+Normal（FP16）
    hr = g_pd3d->CreateTexture(WIN_W, WIN_H, 1, D3DUSAGE_RENDERTARGET,
                               D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &g_TexDN, NULL);
    assert(SUCCEEDED(hr));
    g_TexDN->GetSurfaceLevel(0, &g_RTDN);

    // Z for RTTs
    hr = g_pd3d->CreateDepthStencilSurface(WIN_W, WIN_H, D3DFMT_D24S8,
                                           D3DMULTISAMPLE_NONE, 0, TRUE, &g_RTZ, NULL);
    assert(SUCCEEDED(hr));
}

static void DrawFullscreenQuad()
{
    struct V { float x, y, z, w; float u, v; };
    V v[4] =
    {
        { 0,          0,           0,1, 0,0 },
        { (float)WIN_W,0,          0,1, 1,0 },
        { 0,          (float)WIN_H,0,1, 0,1 },
        { (float)WIN_W,(float)WIN_H,0,1, 1,1 },
    };
    g_pd3d->SetVertexShader(NULL);
    g_pd3d->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
    g_pd3d->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_pd3d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(v[0]));
    g_pd3d->SetRenderState(D3DRS_ZENABLE, TRUE);
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
    if (FAILED(hr))
    {
        hr = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &g_pd3d);
        assert(SUCCEEDED(hr));
    }

    g_pd3d->GetRenderTarget(0, &g_BackBuf);
    g_pd3d->GetDepthStencilSurface(&g_ZDefault);

    // Mesh
    LPD3DXBUFFER mtrl = NULL;
    hr = D3DXLoadMeshFromX(_T("cube.x"), D3DXMESH_SYSTEMMEM, g_pd3d, NULL, &mtrl, NULL, &g_SubsetCount, &g_Mesh);
    assert(SUCCEEDED(hr));
    SAFE_RELEASE(mtrl);

    if (!(g_Mesh->GetFVF() & D3DFVF_NORMAL))
    {
        LPD3DXMESH tmp = NULL;
        g_Mesh->CloneMeshFVF(g_Mesh->GetOptions(), g_Mesh->GetFVF() | D3DFVF_NORMAL, g_pd3d, &tmp);
        SAFE_RELEASE(g_Mesh);
        g_Mesh = tmp;
        D3DXComputeNormals(g_Mesh, NULL);
    }

    // Effect
    hr = D3DXCreateEffectFromFile(g_pd3d, _T("simple.fx"),
                                  NULL, NULL, D3DXSHADER_DEBUG, NULL, &g_Effect, NULL);
    assert(SUCCEEDED(hr));

    g_pd3d->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
    g_pd3d->SetRenderState(D3DRS_ZENABLE, TRUE);

    CreateRTT();
}

static void Render()
{
    static float t = 0.f; t += 0.0002f;

    // Matrices (LH)
    D3DXMATRIX W, V, P;
    D3DXMatrixRotationY(&W, t);
    D3DXVECTOR3 eye(6, 3, -6), at(0, 0, 0), up(0, 1, 0);
    D3DXMatrixLookAtLH(&V, &eye, &at, &up);
    const float nearZ = 0.5f, farZ = 100.0f;
    D3DXMatrixPerspectiveFovLH(&P, D3DX_PI / 4, (float)WIN_W / WIN_H, nearZ, farZ);

    // Common params
    float nf[2] = { nearZ, farZ };
    float texel[2] = { 1.f / WIN_W, 1.f / WIN_H };
    g_Effect->SetFloatArray("g_NearFar", nf, 2);
    g_Effect->SetFloatArray("g_TexelSize", texel, 2);
    g_Effect->SetFloat("g_SampleRadius", 12.0f);
    g_Effect->SetFloat("g_Intensity", 1.2f);
    g_Effect->SetFloat("g_DepthEps", 1.0f / 1024.0f);
    g_Effect->SetFloat("g_Bias", 0.002f);

    // 1) DepthNormal → RTT
    g_pd3d->SetRenderTarget(0, g_RTDN);
    g_pd3d->SetDepthStencilSurface(g_RTZ);
    g_pd3d->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.f, 0);
    g_pd3d->BeginScene();
    DrawMeshWithTech("Tech_DepthNormal", W, V, P);
    g_pd3d->EndScene();

    // 2) Scene → RTT
    g_pd3d->SetRenderTarget(0, g_RTScene);
    g_pd3d->SetDepthStencilSurface(g_RTZ);
    g_pd3d->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_XRGB(90, 90, 90), 1.f, 0);
    g_pd3d->BeginScene();
    DrawMeshWithTech("Tech_Scene", W, V, P);
    g_pd3d->EndScene();

    // 3) SSAO 合成 → BackBuffer
    g_pd3d->SetRenderTarget(0, g_BackBuf);
    g_pd3d->SetDepthStencilSurface(g_ZDefault);
    g_pd3d->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.f, 0);

    g_Effect->SetTexture("tScene", g_TexScene);
    g_Effect->SetTexture("tDepthNormal", g_TexDN);
    g_Effect->SetTechnique("Tech_SSAOCombine");

    g_pd3d->BeginScene();
    {
        UINT np = 0; g_Effect->Begin(&np, 0); g_Effect->BeginPass(0);
        DrawFullscreenQuad();
        g_Effect->EndPass(); g_Effect->End();
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
    SAFE_RELEASE(g_ZDefault);
    SAFE_RELEASE(g_BackBuf);
    SAFE_RELEASE(g_pd3d);
    SAFE_RELEASE(g_pD3D);
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.style = CS_CLASSDC; wc.lpfnWndProc = WndProc; wc.hInstance = hInst; wc.lpszClassName = _T("DX9_SSAO_MIN");
    RegisterClassEx(&wc);

    RECT r = { 0,0,WIN_W,WIN_H };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND wnd = CreateWindow(wc.lpszClassName, _T("DX9 SSAO Minimal"),
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                            r.right - r.left, r.bottom - r.top, NULL, NULL, hInst, NULL);

    InitD3D(wnd);
    ShowWindow(wnd, SW_SHOWDEFAULT); UpdateWindow(wnd);

    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Render();
        }
    }
    Cleanup();
    UnregisterClass(wc.lpszClassName, hInst);
    return 0;
}
