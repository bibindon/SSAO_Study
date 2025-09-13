// main.cpp
// - 1パス目: MRT (RT0=カラー, RT1=Z(深度)を書き込む想定)
// - 2パス目: RT0をfullscreen描画、RT1をD3DXSPRITEで左上1/2サイズ表示
//   ※ RT1 には simple.fx の PixelShaderMRT 側で「線形化したZ値(0..1)」を RGB に複製して書き込む実装を想定。
//      （本ファイルでは g_fNear/g_fFar/g_matView/g_matProj をエフェクトに渡しています）

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

LPDIRECT3D9                 g_pD3D = NULL;
LPDIRECT3DDEVICE9           g_pd3dDevice = NULL;
LPD3DXFONT                  g_pFont = NULL;
LPD3DXMESH                  g_pMesh = NULL;
LPD3DXMESH                  g_pMeshSphere = NULL;
std::vector<D3DMATERIAL9>   g_pMaterials;
std::vector<LPDIRECT3DTEXTURE9> g_pTextures;
DWORD                       g_dwNumMaterials = 0;
LPD3DXEFFECT                g_pEffect1 = NULL;  // simple.fx
LPD3DXEFFECT                g_pEffect2 = NULL;  // simple2.fx

// MRT: 2枚のレンダーターゲット
LPDIRECT3DTEXTURE9          g_pRenderTarget = NULL; // RT0 (カラー)
LPDIRECT3DTEXTURE9          g_pRenderTarget2 = NULL; // RT1 (Zを0..1で格納するテクスチャ)

// フルスクリーン描画
LPDIRECT3DVERTEXDECLARATION9 g_pQuadDecl = NULL;

// スプライト
LPD3DXSPRITE                g_pSprite = NULL;

bool g_bClose = false;

struct QuadVertex
{
    float x, y, z, w;   // クリップ空間
    float u, v;         // テクスチャ座標
};

static void InitD3D(HWND hWnd);
static void Cleanup();
static void RenderPass1();
static void RenderPass2();
static void DrawFullscreenQuad();
static void TextDraw(LPD3DXFONT pFont, const TCHAR* text, int X, int Y);

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = MsgProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = _T("Window1");

    ATOM atom = RegisterClassEx(&wc);
    assert(atom != 0);

    RECT rect;
    SetRect(&rect, 0, 0, 640, 480);
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    HWND hWnd = CreateWindow(_T("Window1"),
                             _T("Hello DirectX9 World !!"),
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             width,
                             height,
                             NULL,
                             NULL,
                             wc.hInstance,
                             NULL);
    assert(hWnd != NULL);

    InitD3D(hWnd);

    ShowWindow(hWnd, SW_SHOWDEFAULT);
    UpdateWindow(hWnd);

    MSG msg;
    while (true)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            DispatchMessage(&msg);
        }
        else
        {
            Sleep(16);
            RenderPass1();
            RenderPass2();
        }

        if (g_bClose)
        {
            break;
        }
    }

    Cleanup();
    UnregisterClass(_T("Window1"), wc.hInstance);
    return 0;
}

void InitD3D(HWND hWnd)
{
    HRESULT hr = E_FAIL;

    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    assert(g_pD3D != NULL);

    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
    d3dpp.BackBufferCount = 1;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    d3dpp.hDeviceWindow = hWnd;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    hr = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT,
                              D3DDEVTYPE_HAL,
                              hWnd,
                              D3DCREATE_HARDWARE_VERTEXPROCESSING,
                              &d3dpp,
                              &g_pd3dDevice);
    if (FAILED(hr))
    {
        hr = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT,
                                  D3DDEVTYPE_HAL,
                                  hWnd,
                                  D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                  &d3dpp,
                                  &g_pd3dDevice);
        assert(SUCCEEDED(hr));
    }

    hr = D3DXCreateFont(g_pd3dDevice,
                        20,
                        0,
                        FW_HEAVY,
                        1,
                        FALSE,
                        SHIFTJIS_CHARSET,
                        OUT_TT_ONLY_PRECIS,
                        CLEARTYPE_NATURAL_QUALITY,
                        FF_DONTCARE,
                        _T("ＭＳ ゴシック"),
                        &g_pFont);
    assert(SUCCEEDED(hr));

    // メッシュ読み込み（cube.x）
    LPD3DXBUFFER pMtrlBuffer = NULL;
    hr = D3DXLoadMeshFromX(_T("cube.x"),
                           D3DXMESH_SYSTEMMEM,
                           g_pd3dDevice,
                           NULL,
                           &pMtrlBuffer,
                           NULL,
                           &g_dwNumMaterials,
                           &g_pMesh);
    assert(SUCCEEDED(hr));

    D3DXMATERIAL* pMtrls = (D3DXMATERIAL*)pMtrlBuffer->GetBufferPointer();
    g_pMaterials.resize(g_dwNumMaterials);
    g_pTextures.resize(g_dwNumMaterials);

    for (DWORD i = 0; i < g_dwNumMaterials; ++i)
    {
        g_pMaterials[i] = pMtrls[i].MatD3D;
        g_pMaterials[i].Ambient = g_pMaterials[i].Diffuse;
        g_pTextures[i] = NULL;

        std::string tex(pMtrls[i].pTextureFilename ? pMtrls[i].pTextureFilename : "");
        if (!tex.empty())
        {
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
    pMtrlBuffer->Release();

    // エフェクト
    hr = D3DXCreateEffectFromFile(g_pd3dDevice,
                                  _T("simple.fx"),
                                  NULL, NULL,
                                  D3DXSHADER_DEBUG,
                                  NULL,
                                  &g_pEffect1,
                                  NULL);
    assert(SUCCEEDED(hr));

    hr = D3DXCreateEffectFromFile(g_pd3dDevice,
                                  _T("simple2.fx"),
                                  NULL, NULL,
                                  D3DXSHADER_DEBUG,
                                  NULL,
                                  &g_pEffect2,
                                  NULL);
    assert(SUCCEEDED(hr));

    // 球メッシュ（テクスチャなし）
    hr = D3DXCreateSphere(g_pd3dDevice, 20.0f, 32, 32, &g_pMeshSphere, NULL);
    assert(SUCCEEDED(hr));

    // RT0, RT1（A8R8G8B8）を作成
    hr = D3DXCreateTexture(g_pd3dDevice,
                           640, 480,
                           1,
                           D3DUSAGE_RENDERTARGET,
                           D3DFMT_A8R8G8B8,
                           D3DPOOL_DEFAULT,
                           &g_pRenderTarget);
    assert(SUCCEEDED(hr));

    hr = D3DXCreateTexture(g_pd3dDevice,
                           640, 480,
                           1,
                           D3DUSAGE_RENDERTARGET,
                           D3DFMT_A8R8G8B8,
                           D3DPOOL_DEFAULT,
                           &g_pRenderTarget2);
    assert(SUCCEEDED(hr));

    // フルスクリーンクアッドの頂宣言
    D3DVERTEXELEMENT9 elems[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
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
    for (size_t i = 0; i < g_pTextures.size(); ++i)
    {
        SAFE_RELEASE(g_pTextures[i]);
    }

    SAFE_RELEASE(g_pMesh);
    SAFE_RELEASE(g_pMeshSphere);
    SAFE_RELEASE(g_pEffect1);
    SAFE_RELEASE(g_pEffect2);
    SAFE_RELEASE(g_pFont);

    SAFE_RELEASE(g_pRenderTarget);
    SAFE_RELEASE(g_pRenderTarget2);
    SAFE_RELEASE(g_pQuadDecl);
    SAFE_RELEASE(g_pSprite);

    SAFE_RELEASE(g_pd3dDevice);
    SAFE_RELEASE(g_pD3D);
}

void RenderPass1()
{
    HRESULT hr = E_FAIL;

    // 既存のバックバッファRTを保存
    LPDIRECT3DSURFACE9 pOldRT0 = NULL;
    hr = g_pd3dDevice->GetRenderTarget(0, &pOldRT0);
    assert(SUCCEEDED(hr));

    // RT0/RT1 のサーフェスを取得
    LPDIRECT3DSURFACE9 pRT0 = NULL;
    LPDIRECT3DSURFACE9 pRT1 = NULL;
    hr = g_pRenderTarget->GetSurfaceLevel(0, &pRT0);  assert(SUCCEEDED(hr));
    hr = g_pRenderTarget2->GetSurfaceLevel(0, &pRT1); assert(SUCCEEDED(hr));

    // MRT 設定
    hr = g_pd3dDevice->SetRenderTarget(0, pRT0); assert(SUCCEEDED(hr));
    hr = g_pd3dDevice->SetRenderTarget(1, pRT1); assert(SUCCEEDED(hr));

    // ビュー・プロジェクション
    static float t = 0.0f;
    t += 0.025f;

    const float zNear = 1.0f;
    const float zFar = 10000.0f;

    D3DXMATRIX matView, matProj, matWVP, matId;
    D3DXVECTOR3 eye(10.0f * sinf(t), 5.0f, -10.0f * cosf(t));
    D3DXVECTOR3 at(0.0f, 0.0f, 0.0f);
    D3DXVECTOR3 up(0.0f, 1.0f, 0.0f);

    D3DXMatrixLookAtLH(&matView, &eye, &at, &up);
    D3DXMatrixPerspectiveFovLH(&matProj, D3DXToRadian(45.0f), 640.0f / 480.0f, zNear, zFar);
    D3DXMatrixIdentity(&matId);
    matWVP = matId * matView * matProj;

    // クリア（RT0/RT1/Depth）
    hr = g_pd3dDevice->Clear(0, NULL,
                             D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                             D3DCOLOR_XRGB(100, 100, 100),
                             1.0f, 0);
    assert(SUCCEEDED(hr));

    hr = g_pd3dDevice->BeginScene(); assert(SUCCEEDED(hr));

    // 画面テキスト
    TextDraw(g_pFont, _T("MRT: RT0=color, RT1=linear Z (0..1)"), 8, 8);

    // エフェクトへ行列とNear/Farを渡す（simple.fx 側でZ線形化に使用）
    hr = g_pEffect1->SetMatrix("g_matWorldViewProj", &matWVP); assert(SUCCEEDED(hr));
    hr = g_pEffect1->SetMatrix("g_matView", &matView);         assert(SUCCEEDED(hr));
    hr = g_pEffect1->SetMatrix("g_matProj", &matProj);         assert(SUCCEEDED(hr));
    hr = g_pEffect1->SetFloat("g_fNear", zNear);               assert(SUCCEEDED(hr));
    hr = g_pEffect1->SetFloat("g_fFar", zFar);                assert(SUCCEEDED(hr));

    // MRT 用テクニック
    hr = g_pEffect1->SetTechnique("TechniqueMRT"); assert(SUCCEEDED(hr));

    UINT nPass = 0;
    hr = g_pEffect1->Begin(&nPass, 0);      assert(SUCCEEDED(hr));
    hr = g_pEffect1->BeginPass(0);          assert(SUCCEEDED(hr));

    // テクスチャ有りメッシュ
    hr = g_pEffect1->SetBool("g_bUseTexture", TRUE); assert(SUCCEEDED(hr));
    for (DWORD i = 0; i < g_dwNumMaterials; ++i)
    {
        hr = g_pEffect1->SetTexture("texture1", g_pTextures[i]); assert(SUCCEEDED(hr));
        hr = g_pEffect1->CommitChanges();                        assert(SUCCEEDED(hr));
        hr = g_pMesh->DrawSubset(i);                             assert(SUCCEEDED(hr));
    }

    // 球（テクスチャなし）
    hr = g_pEffect1->SetBool("g_bUseTexture", FALSE); assert(SUCCEEDED(hr));
    hr = g_pEffect1->SetTexture("texture1", NULL);    assert(SUCCEEDED(hr));
    hr = g_pEffect1->CommitChanges();                 assert(SUCCEEDED(hr));
    hr = g_pMeshSphere->DrawSubset(0);                assert(SUCCEEDED(hr));

    hr = g_pEffect1->EndPass(); assert(SUCCEEDED(hr));
    hr = g_pEffect1->End();     assert(SUCCEEDED(hr));

    hr = g_pd3dDevice->EndScene(); assert(SUCCEEDED(hr));

    // MRT解除＆バックバッファへ復帰
    g_pd3dDevice->SetRenderTarget(1, NULL);
    g_pd3dDevice->SetRenderTarget(0, pOldRT0);

    SAFE_RELEASE(pRT0);
    SAFE_RELEASE(pRT1);
    SAFE_RELEASE(pOldRT0);
}

void RenderPass2()
{
    HRESULT hr = E_FAIL;

    hr = g_pd3dDevice->Clear(0, NULL,
                             D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                             D3DCOLOR_XRGB(0, 0, 0),
                             1.0f, 0);
    assert(SUCCEEDED(hr));

    // 2D用にZ無効
    g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);

    hr = g_pd3dDevice->BeginScene(); assert(SUCCEEDED(hr));

    // フルスクリーン（RT0 を simple2.fx で表示）
    hr = g_pEffect2->SetTechnique("Technique1");           assert(SUCCEEDED(hr));
    UINT nPass = 0;
    hr = g_pEffect2->Begin(&nPass, 0);                     assert(SUCCEEDED(hr));
    hr = g_pEffect2->BeginPass(0);                         assert(SUCCEEDED(hr));
    hr = g_pEffect2->SetTexture("texture1", g_pRenderTarget); assert(SUCCEEDED(hr));
    hr = g_pEffect2->CommitChanges();                      assert(SUCCEEDED(hr));

    DrawFullscreenQuad();

    hr = g_pEffect2->EndPass(); assert(SUCCEEDED(hr));
    hr = g_pEffect2->End();     assert(SUCCEEDED(hr));

    // 左上に RT1 (Z) を 1/2 スケールで描画（D3DXSPRITE）
    if (g_pSprite != NULL)
    {
        hr = g_pSprite->Begin(D3DXSPRITE_ALPHABLEND); assert(SUCCEEDED(hr));

        D3DXMATRIX mat;
        D3DXVECTOR2 scale(0.5f, 0.5f);
        D3DXVECTOR2 trans(0.0f, 0.0f);
        D3DXMatrixTransformation2D(&mat, NULL, 0.0f, &scale, NULL, 0.0f, &trans);
        g_pSprite->SetTransform(&mat);

        // simple.fx 側で RT1 に (linearZ,linearZ,linearZ,1) を出力していれば、そのまま可視化されます
        hr = g_pSprite->Draw(g_pRenderTarget2, NULL, NULL, NULL, 0xFFFFFFFF);
        assert(SUCCEEDED(hr));

        hr = g_pSprite->End(); assert(SUCCEEDED(hr));
    }

    hr = g_pd3dDevice->EndScene();  assert(SUCCEEDED(hr));
    hr = g_pd3dDevice->Present(NULL, NULL, NULL, NULL); assert(SUCCEEDED(hr));

    g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
}

void DrawFullscreenQuad()
{
    QuadVertex v[4];

    // 0.5ピクセルオフセット補正
    const float du = 0.5f / 640.0f;
    const float dv = 0.5f / 480.0f;

    v[0].x = -1.0f; v[0].y = -1.0f; v[0].z = 0.0f; v[0].w = 1.0f; v[0].u = 0.0f + du; v[0].v = 1.0f - dv;
    v[1].x = -1.0f; v[1].y = 1.0f; v[1].z = 0.0f; v[1].w = 1.0f; v[1].u = 0.0f + du; v[1].v = 0.0f + dv;
    v[2].x = 1.0f; v[2].y = -1.0f; v[2].z = 0.0f; v[2].w = 1.0f; v[2].u = 1.0f - du; v[2].v = 1.0f - dv;
    v[3].x = 1.0f; v[3].y = 1.0f; v[3].z = 0.0f; v[3].w = 1.0f; v[3].u = 1.0f - du; v[3].v = 0.0f + dv;

    g_pd3dDevice->SetVertexDeclaration(g_pQuadDecl);
    g_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(QuadVertex));
}

void TextDraw(LPD3DXFONT pFont, const TCHAR* text, int X, int Y)
{
    RECT rc = { X, Y, 0, 0 };
    HRESULT hr = pFont->DrawText(NULL, text, -1, &rc, DT_LEFT | DT_NOCLIP, D3DCOLOR_ARGB(255, 0, 0, 0));
    assert(SUCCEEDED(hr));
}

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        g_bClose = true;
        return 0;
    default:
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
