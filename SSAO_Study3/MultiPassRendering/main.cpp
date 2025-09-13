// main.cpp  — MRT3: RT0=color, RT1=Z可視化(左上1/2), RT2=WorldPos可視化(左下1/2)

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
#include <string>
#include <vector>
#include <crtdbg.h>

#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p) = NULL; } } while(0)

LPDIRECT3D9                   g_pD3D = NULL;
LPDIRECT3DDEVICE9             g_pd3dDevice = NULL;
LPD3DXFONT                    g_pFont = NULL;
LPD3DXMESH                    g_pMesh = NULL;
LPD3DXMESH                    g_pMeshSphere = NULL;
std::vector<D3DMATERIAL9>     g_pMaterials;
std::vector<LPDIRECT3DTEXTURE9> g_pTextures;
DWORD                         g_dwNumMaterials = 0;
LPD3DXEFFECT                  g_pEffect1 = NULL;  // simple.fx
LPD3DXEFFECT                  g_pEffect2 = NULL;  // simple2.fx

// MRT: 3枚のレンダーターゲット
LPDIRECT3DTEXTURE9            g_pRenderTarget = NULL; // RT0: color
LPDIRECT3DTEXTURE9            g_pRenderTarget2 = NULL; // RT1: depth viz
LPDIRECT3DTEXTURE9            g_pRenderTarget3 = NULL; // RT2: world-pos viz

LPDIRECT3DVERTEXDECLARATION9  g_pQuadDecl = NULL;
LPD3DXSPRITE                  g_pSprite = NULL;

bool g_bClose = false;

struct QuadVertex {
    float x, y, z, w;
    float u, v;
};

static void InitD3D(HWND hWnd);
static void Cleanup();
static void RenderPass1();
static void RenderPass2();
static void DrawFullscreenQuad();
static void TextDraw(LPD3DXFONT pFont, const TCHAR* text, int X, int Y);

LRESULT WINAPI MsgProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = MsgProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = _T("Window1");
    RegisterClassEx(&wc);

    RECT rc = { 0,0,640,480 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;

    HWND hWnd = CreateWindow(_T("Window1"), _T("Hello DirectX9 World !!"),
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, w, h,
                             NULL, NULL, hInstance, NULL);

    InitD3D(hWnd);
    ShowWindow(hWnd, SW_SHOWDEFAULT);
    UpdateWindow(hWnd);

    MSG msg;
    while (true) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            DispatchMessage(&msg);
        }
        else {
            Sleep(16);
            RenderPass1();
            RenderPass2();
        }
        if (g_bClose) break;
    }

    Cleanup();
    UnregisterClass(_T("Window1"), hInstance);
    return 0;
}

void InitD3D(HWND hWnd)
{
    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    assert(g_pD3D);

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferCount = 1;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D16;
    pp.hDeviceWindow = hWnd;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    HRESULT hr = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &g_pd3dDevice);
    if (FAILED(hr)) {
        hr = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &g_pd3dDevice);
        assert(SUCCEEDED(hr));
    }

    // MRT 3枚に対応しているか軽く確認（無ければ2までは動く）
    D3DCAPS9 caps = {};
    g_pD3D->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps);
    // caps.NumSimultaneousRTs >= 3 が望ましい

    hr = D3DXCreateFont(g_pd3dDevice, 20, 0, FW_HEAVY, 1, FALSE, SHIFTJIS_CHARSET,
                        OUT_TT_ONLY_PRECIS, CLEARTYPE_NATURAL_QUALITY, FF_DONTCARE,
                        _T("ＭＳ ゴシック"), &g_pFont);
    assert(SUCCEEDED(hr));

    // メッシュ
    LPD3DXBUFFER pMtrlBuf = NULL;
    hr = D3DXLoadMeshFromX(_T("cube.x"), D3DXMESH_SYSTEMMEM, g_pd3dDevice,
                           NULL, &pMtrlBuf, NULL, &g_dwNumMaterials, &g_pMesh);
    assert(SUCCEEDED(hr));

    D3DXMATERIAL* mtrls = (D3DXMATERIAL*)pMtrlBuf->GetBufferPointer();
    g_pMaterials.resize(g_dwNumMaterials);
    g_pTextures.resize(g_dwNumMaterials, NULL);
    for (DWORD i = 0; i < g_dwNumMaterials; ++i) {
        g_pMaterials[i] = mtrls[i].MatD3D;
        g_pMaterials[i].Ambient = g_pMaterials[i].Diffuse;
        std::string tex = mtrls[i].pTextureFilename ? mtrls[i].pTextureFilename : "";
        if (!tex.empty()) {
#ifndef UNICODE
            hr = D3DXCreateTextureFromFileA(g_pd3dDevice, tex.c_str(), &g_pTextures[i]);
#else
            int len = MultiByteToWideChar(CP_ACP, 0, tex.c_str(), -1, NULL, 0);
            std::wstring w(len, 0);
            MultiByteToWideChar(CP_ACP, 0, tex.c_str(), -1, &w[0], len);
            hr = D3DXCreateTextureFromFileW(g_pd3dDevice, w.c_str(), &g_pTextures[i]);
#endif
            assert(SUCCEEDED(hr));
        }
    }
    pMtrlBuf->Release();

    // エフェクト
    hr = D3DXCreateEffectFromFile(g_pd3dDevice, _T("simple.fx"),
                                  NULL, NULL, D3DXSHADER_DEBUG, NULL, &g_pEffect1, NULL);
    assert(SUCCEEDED(hr));
    hr = D3DXCreateEffectFromFile(g_pd3dDevice, _T("simple2.fx"),
                                  NULL, NULL, D3DXSHADER_DEBUG, NULL, &g_pEffect2, NULL);
    assert(SUCCEEDED(hr));

    // 球
    hr = D3DXCreateSphere(g_pd3dDevice, 20.0f, 32, 32, &g_pMeshSphere, NULL);
    assert(SUCCEEDED(hr));

    // RT0/RT1/RT2 作成（A8R8G8B8）
    hr = D3DXCreateTexture(g_pd3dDevice, 640, 480, 1,
                           D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                           D3DPOOL_DEFAULT, &g_pRenderTarget);  assert(SUCCEEDED(hr));
    hr = D3DXCreateTexture(g_pd3dDevice, 640, 480, 1,
                           D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                           D3DPOOL_DEFAULT, &g_pRenderTarget2); assert(SUCCEEDED(hr));
    hr = D3DXCreateTexture(g_pd3dDevice, 640, 480, 1,
                           D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                           D3DPOOL_DEFAULT, &g_pRenderTarget3); assert(SUCCEEDED(hr));

    // フルスクリーン・クアッド宣言
    D3DVERTEXELEMENT9 elems[] = {
        {0, 0,  D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()
    };
    hr = g_pd3dDevice->CreateVertexDeclaration(elems, &g_pQuadDecl);
    assert(SUCCEEDED(hr));

    // スプライト
    hr = D3DXCreateSprite(g_pd3dDevice, &g_pSprite);
    assert(SUCCEEDED(hr));
}

void Cleanup()
{
    for (size_t i = 0; i < g_pTextures.size(); ++i) SAFE_RELEASE(g_pTextures[i]);
    SAFE_RELEASE(g_pMesh);
    SAFE_RELEASE(g_pMeshSphere);
    SAFE_RELEASE(g_pEffect1);
    SAFE_RELEASE(g_pEffect2);
    SAFE_RELEASE(g_pFont);
    SAFE_RELEASE(g_pRenderTarget);
    SAFE_RELEASE(g_pRenderTarget2);
    SAFE_RELEASE(g_pRenderTarget3);
    SAFE_RELEASE(g_pQuadDecl);
    SAFE_RELEASE(g_pSprite);
    SAFE_RELEASE(g_pd3dDevice);
    SAFE_RELEASE(g_pD3D);
}

void RenderPass1()
{
    // バックバッファを退避
    LPDIRECT3DSURFACE9 pOldRT0 = NULL;
    g_pd3dDevice->GetRenderTarget(0, &pOldRT0);

    // 3枚のRT
    LPDIRECT3DSURFACE9 pRT0 = NULL, pRT1 = NULL, pRT2 = NULL;
    g_pRenderTarget->GetSurfaceLevel(0, &pRT0);
    g_pRenderTarget2->GetSurfaceLevel(0, &pRT1);
    g_pRenderTarget3->GetSurfaceLevel(0, &pRT2);

    g_pd3dDevice->SetRenderTarget(0, pRT0);
    g_pd3dDevice->SetRenderTarget(1, pRT1);
    g_pd3dDevice->SetRenderTarget(2, pRT2);

    static float t = 0.0f; t += 0.025f;

    const float zNear = 1.0f, zFar = 10000.0f;
    D3DXMATRIX W, V, P, WVP;
    D3DXMatrixIdentity(&W);

    D3DXVECTOR3 eye(10.0f * sinf(t), 5.0f, -10.0f * cosf(t));
    D3DXVECTOR3 at(0, 0, 0), up(0, 1, 0);
    D3DXMatrixLookAtLH(&V, &eye, &at, &up);
    D3DXMatrixPerspectiveFovLH(&P, D3DXToRadian(45.0f), 640.0f / 480.0f, zNear, zFar);
    WVP = W * V * P;

    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                        D3DCOLOR_XRGB(100, 100, 100), 1.0f, 0);

    g_pd3dDevice->BeginScene();
    TextDraw(g_pFont, _T("MRT3: RT0=color, RT1=Z(top-left), RT2=WorldPos(bottom-left)"), 8, 8);

    // エフェクトへ行列／パラメータ
    g_pEffect1->SetMatrix("g_matWorld", &W);
    g_pEffect1->SetMatrix("g_matView", &V);
    g_pEffect1->SetMatrix("g_matProj", &P);
    g_pEffect1->SetMatrix("g_matWorldViewProj", &WVP);
    g_pEffect1->SetFloat("g_fNear", zNear);
    g_pEffect1->SetFloat("g_fFar", zFar);

    // （必要に応じて可視化パラメータを調整）
    g_pEffect1->SetFloat("g_vizMax", 100.0f);
    g_pEffect1->SetFloat("g_vizGamma", 0.25f);
    D3DXVECTOR4 posCenter(0, 0, 0, 0);
    g_pEffect1->SetVector("g_posCenter", &posCenter);
    g_pEffect1->SetFloat("g_posRange", 1.0f);

    g_pEffect1->SetTechnique("TechniqueMRT");

    UINT nPass = 0;
    g_pEffect1->Begin(&nPass, 0);
    g_pEffect1->BeginPass(0);

    // テクスチャありメッシュ
    g_pEffect1->SetBool("g_bUseTexture", TRUE);
    for (DWORD i = 0; i < g_dwNumMaterials; ++i) {
        g_pEffect1->SetTexture("texture1", g_pTextures[i]);
        g_pEffect1->CommitChanges();
        g_pMesh->DrawSubset(i);
    }

    // 球（テクスチャなし）
    g_pEffect1->SetBool("g_bUseTexture", FALSE);
    g_pEffect1->SetTexture("texture1", NULL);
    g_pEffect1->CommitChanges();
    g_pMeshSphere->DrawSubset(0);

    g_pEffect1->EndPass();
    g_pEffect1->End();

    g_pd3dDevice->EndScene();

    // 復帰
    g_pd3dDevice->SetRenderTarget(2, NULL);
    g_pd3dDevice->SetRenderTarget(1, NULL);
    g_pd3dDevice->SetRenderTarget(0, pOldRT0);

    SAFE_RELEASE(pRT0);
    SAFE_RELEASE(pRT1);
    SAFE_RELEASE(pRT2);
    SAFE_RELEASE(pOldRT0);
}

void RenderPass2()
{
    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                        D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);

    g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_pd3dDevice->BeginScene();

    // まず RT0 を全画面表示
    g_pEffect2->SetTechnique("Technique1");
    UINT nPass = 0;
    g_pEffect2->Begin(&nPass, 0);
    g_pEffect2->BeginPass(0);
    g_pEffect2->SetTexture("texture1", g_pRenderTarget);
    g_pEffect2->CommitChanges();
    DrawFullscreenQuad();
    g_pEffect2->EndPass();
    g_pEffect2->End();

    // 左上に RT1（Z）を 1/2 表示
    if (g_pSprite) {
        g_pSprite->Begin(D3DXSPRITE_ALPHABLEND);
        {
            D3DXMATRIX mat;
            D3DXVECTOR2 scale(0.5f, 0.5f);
            D3DXVECTOR2 trans(0.0f, 0.0f); // 左上
            D3DXMatrixTransformation2D(&mat, NULL, 0.0f, &scale, NULL, 0.0f, &trans);
            g_pSprite->SetTransform(&mat);
            g_pSprite->Draw(g_pRenderTarget2, NULL, NULL, NULL, 0xFFFFFFFF);
        }
        // 左下に RT2（WorldPos）を 1/2 表示
        {
            D3DXMATRIX mat;
            D3DXVECTOR2 scale(0.5f, 0.5f);
            D3DXVECTOR2 trans(0.0f, 480.0f * 0.5f); // 左下（高さの半分だけ下へ）
            D3DXMatrixTransformation2D(&mat, NULL, 0.0f, &scale, NULL, 0.0f, &trans);
            g_pSprite->SetTransform(&mat);
            g_pSprite->Draw(g_pRenderTarget3, NULL, NULL, NULL, 0xFFFFFFFF);
        }
        g_pSprite->End();
    }

    g_pd3dDevice->EndScene();
    g_pd3dDevice->Present(NULL, NULL, NULL, NULL);

    g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
}

void DrawFullscreenQuad()
{
    QuadVertex v[4];

    const float du = 0.5f / 640.0f;
    const float dv = 0.5f / 480.0f;

    v[0] = { -1, -1, 0, 1, 0 + du, 1 - dv };
    v[1] = { -1,  1, 0, 1, 0 + du, 0 + dv };
    v[2] = { 1, -1, 0, 1, 1 - du, 1 - dv };
    v[3] = { 1,  1, 0, 1, 1 - du, 0 + dv };

    g_pd3dDevice->SetVertexDeclaration(g_pQuadDecl);
    g_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(QuadVertex));
}

void TextDraw(LPD3DXFONT pFont, const TCHAR* text, int X, int Y)
{
    RECT r = { X, Y, 0, 0 };
    pFont->DrawText(NULL, text, -1, &r, DT_LEFT | DT_NOCLIP, D3DCOLOR_ARGB(255, 0, 0, 0));
}

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        g_bClose = true;
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
