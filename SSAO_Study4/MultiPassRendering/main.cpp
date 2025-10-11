// main.cpp - 簡素化版
// MRT3でSSAOを実装

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9d.lib")

#include <d3d9.h>
#include <d3dx9.h>
#include <tchar.h>
#include <cassert>
#include <vector>

#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p)=NULL; } } while(0)

static const int kBackW = 800;
static const int kBackH = 600;

LPDIRECT3D9                   g_pD3D = NULL;
LPDIRECT3DDEVICE9             g_pd3dDevice = NULL;
LPD3DXMESH                    g_pMeshCube = NULL;
LPD3DXMESH                    g_pMeshSphere = NULL;
LPD3DXMESH                    g_pMeshSky = NULL;
std::vector<LPDIRECT3DTEXTURE9> g_pTextures;
DWORD                         g_dwNumMaterials = 0;

LPD3DXEFFECT                  g_pEffect1 = NULL; // simple.fx
LPD3DXEFFECT                  g_pEffect2 = NULL; // simple2.fx

// MRT: 3枚
LPDIRECT3DTEXTURE9            g_pRenderTarget = NULL;  // RT0: color
LPDIRECT3DTEXTURE9            g_pRenderTarget2 = NULL; // RT1: Z画像
LPDIRECT3DTEXTURE9            g_pRenderTarget3 = NULL; // RT2: POS画像

LPDIRECT3DVERTEXDECLARATION9  g_pQuadDecl = NULL;
bool                          g_bClose = false;

D3DXMATRIX g_lastView, g_lastProj;

struct QuadVertex {
    float x, y, z, w;
    float u, v;
};

static void InitD3D(HWND hWnd);
static void Cleanup();
static void RenderPass1();
static void RenderPass2();
static void DrawFullscreenQuad();
LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int)
{
    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = MsgProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = _T("SSAODemo");
    RegisterClassEx(&wc);

    RECT rc = { 0,0,kBackW,kBackH };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hWnd = CreateWindow(_T("SSAODemo"), _T("SSAO Demo"),
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                             rc.right - rc.left, rc.bottom - rc.top,
                             NULL, NULL, hInstance, NULL);

    InitD3D(hWnd);
    ShowWindow(hWnd, SW_SHOWDEFAULT);

    MSG msg;
    while (true)
    {
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
    UnregisterClass(_T("SSAODemo"), hInstance);
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
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D16;
    pp.hDeviceWindow = hWnd;

    g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                         D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &g_pd3dDevice);

    // cube.xロード
    LPD3DXBUFFER pMtrlBuf = NULL;
    D3DXLoadMeshFromX(_T("cube.x"), D3DXMESH_SYSTEMMEM, g_pd3dDevice,
                      NULL, &pMtrlBuf, NULL, &g_dwNumMaterials, &g_pMeshCube);

    D3DXMATERIAL* mtrls = (D3DXMATERIAL*)pMtrlBuf->GetBufferPointer();
    g_pTextures.resize(g_dwNumMaterials, NULL);
    pMtrlBuf->Release();

    // sphere.xロード
    D3DXLoadMeshFromX(_T("sphere.x"), D3DXMESH_SYSTEMMEM, g_pd3dDevice,
                      NULL, NULL, NULL, NULL, &g_pMeshSphere);

    D3DXLoadMeshFromX(_T("sky.blend.x"), D3DXMESH_SYSTEMMEM, g_pd3dDevice,
                      NULL, NULL, NULL, NULL, &g_pMeshSky);

    // エフェクト
    D3DXCreateEffectFromFile(g_pd3dDevice, _T("simple.fx"),
                             NULL, NULL, 0, NULL, &g_pEffect1, NULL);
    D3DXCreateEffectFromFile(g_pd3dDevice, _T("simple2.fx"),
                             NULL, NULL, 0, NULL, &g_pEffect2, NULL);

    // MRT
    D3DXCreateTexture(g_pd3dDevice, kBackW, kBackH, 1,
                      D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                      D3DPOOL_DEFAULT, &g_pRenderTarget);
    D3DXCreateTexture(g_pd3dDevice, kBackW, kBackH, 1,
                      D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F,
                      D3DPOOL_DEFAULT, &g_pRenderTarget2);
    D3DXCreateTexture(g_pd3dDevice, kBackW, kBackH, 1,
                      D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F,
                      D3DPOOL_DEFAULT, &g_pRenderTarget3);

    // クアッド宣言
    D3DVERTEXELEMENT9 elems[] = {
        {0, 0,  D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()
    };
    g_pd3dDevice->CreateVertexDeclaration(elems, &g_pQuadDecl);
}

void Cleanup()
{
    for (size_t i = 0; i < g_pTextures.size(); ++i) SAFE_RELEASE(g_pTextures[i]);
    SAFE_RELEASE(g_pMeshCube);
    SAFE_RELEASE(g_pMeshSphere);
    SAFE_RELEASE(g_pMeshSky);
    SAFE_RELEASE(g_pEffect1);
    SAFE_RELEASE(g_pEffect2);
    SAFE_RELEASE(g_pRenderTarget);
    SAFE_RELEASE(g_pRenderTarget2);
    SAFE_RELEASE(g_pRenderTarget3);
    SAFE_RELEASE(g_pQuadDecl);
    SAFE_RELEASE(g_pd3dDevice);
    SAFE_RELEASE(g_pD3D);
}

void RenderPass1()
{
    // バックバッファ退避
    LPDIRECT3DSURFACE9 pOldRT0 = NULL;
    g_pd3dDevice->GetRenderTarget(0, &pOldRT0);

    // MRT設定
    LPDIRECT3DSURFACE9 pRT0, pRT1, pRT2;
    g_pRenderTarget->GetSurfaceLevel(0, &pRT0);
    g_pRenderTarget2->GetSurfaceLevel(0, &pRT1);
    g_pRenderTarget3->GetSurfaceLevel(0, &pRT2);
    g_pd3dDevice->SetRenderTarget(0, pRT0);
    g_pd3dDevice->SetRenderTarget(1, pRT1);
    g_pd3dDevice->SetRenderTarget(2, pRT2);

    static float t = 0.0f;
    t += 0.025f;

    // カメラ設定
    D3DXMATRIX W, V, P, WVP;
    D3DXMatrixIdentity(&W);
    D3DXVECTOR3 eye(10.0f * sinf(t), 5.0f, -10.0f * cosf(t));
    D3DXVECTOR3 at(0, 2, 0), up(0, 1, 0);
    D3DXMatrixLookAtLH(&V, &eye, &at, &up);
    D3DXMatrixPerspectiveFovLH(&P, D3DXToRadian(45.0f),
                               (float)kBackW / (float)kBackH, 1.0f, 1000.0f);
    WVP = W * V * P;

    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                        D3DCOLOR_XRGB(100, 100, 100), 1.0f, 0);

    g_pd3dDevice->BeginScene();

    // エフェクトパラメータ設定
    g_pEffect1->SetMatrix("g_matWorld", &W);
    g_pEffect1->SetMatrix("g_matView", &V);
    g_pEffect1->SetMatrix("g_matProj", &P);
    g_pEffect1->SetMatrix("g_matWorldViewProj", &WVP);
    g_pEffect1->SetFloat("g_fNear", 1.0f);
    g_pEffect1->SetFloat("g_fFar", 1000.0f);
    g_pEffect1->SetFloat("g_vizMax", 100.0f);
    g_pEffect1->SetFloat("g_vizGamma", 0.25f);
    g_pEffect1->SetFloat("g_posRange", 50.0f);

    // 描画
    g_pEffect1->SetTechnique("TechniqueMRT");
    UINT nPass;
    g_pEffect1->Begin(&nPass, 0);
    g_pEffect1->BeginPass(0);

    g_pEffect1->SetBool("g_bUseTexture", FALSE);

    // キューブ描画
    for (DWORD i = 0; i < g_dwNumMaterials; ++i) {
        g_pEffect1->CommitChanges();
        g_pMeshCube->DrawSubset(i);
    }

    // 球体描画（上に配置）
    static float t2 = 0.0f;
    t2 += 0.05f;
    D3DXMatrixTranslation(&W, 0.0f, 2.0f + sinf(t2) * 1, 0.0f);
    g_pEffect1->SetMatrix("g_matWorld", &W);
    g_pEffect1->CommitChanges();
    g_pMeshSphere->DrawSubset(0);
    g_pMeshSky->DrawSubset(0);

    g_pEffect1->EndPass();
    g_pEffect1->End();
    g_pd3dDevice->EndScene();

    // 復帰
    g_pd3dDevice->SetRenderTarget(2, NULL);
    g_pd3dDevice->SetRenderTarget(1, NULL);
    g_pd3dDevice->SetRenderTarget(0, pOldRT0);
    SAFE_RELEASE(pRT0); SAFE_RELEASE(pRT1); SAFE_RELEASE(pRT2); SAFE_RELEASE(pOldRT0);

    g_lastView = V;
    g_lastProj = P;
}

void RenderPass2()
{
    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                        D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_pd3dDevice->BeginScene();

    // SSAO適用
    g_pEffect2->SetTechnique("TechniqueAO");
    g_pEffect2->SetMatrix("g_matView", &g_lastView);
    g_pEffect2->SetMatrix("g_matProj", &g_lastProj);
    g_pEffect2->SetFloat("g_fNear", 1.0f);
    g_pEffect2->SetFloat("g_fFar", 1000.0f);
    g_pEffect2->SetFloat("g_posRange", 50.0f);
    g_pEffect2->SetTexture("texColor", g_pRenderTarget);
    g_pEffect2->SetTexture("texZ", g_pRenderTarget2);
    g_pEffect2->SetTexture("texPos", g_pRenderTarget3);
    g_pEffect2->SetFloat("g_aoStepWorld", 1.0f);
    g_pEffect2->SetFloat("g_aoStrength", 1.5f);
    g_pEffect2->SetFloat("g_aoBias", 0.001f);

    UINT nPass;
    g_pEffect2->Begin(&nPass, 0);
    g_pEffect2->BeginPass(0);
    g_pEffect2->CommitChanges();

    DrawFullscreenQuad();

    g_pEffect2->EndPass();
    g_pEffect2->End();
    g_pd3dDevice->EndScene();
    g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
    g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
}

void DrawFullscreenQuad()
{
    QuadVertex v[4];
    const float du = 0.5f / (float)kBackW;
    const float dv = 0.5f / (float)kBackH;

    v[0] = { -1, -1, 0, 1, 0 + du, 1 - dv };
    v[1] = { -1,  1, 0, 1, 0 + du, 0 + dv };
    v[2] = { 1, -1, 0, 1, 1 - du, 1 - dv };
    v[3] = { 1,  1, 0, 1, 1 - du, 0 + dv };

    g_pd3dDevice->SetVertexDeclaration(g_pQuadDecl);
    g_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(QuadVertex));
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
