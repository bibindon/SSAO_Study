#pragma comment( lib, "d3d9.lib" )
#if defined(DEBUG) || defined(_DEBUG)
#pragma comment( lib, "d3dx9d.lib" )
#else
#pragma comment( lib, "d3dx9.lib" )
#endif

#include <d3d9.h>
#include <d3dx9.h>
#include <string>
#include <tchar.h>
#include <cassert>
#include <crtdbg.h>
#include <cmath>
#include <vector>

#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = NULL; } }

namespace
{
    constexpr int kRenderWidth = 1600;
    constexpr int kRenderHeight = 900;
    constexpr float kCameraMoveSpeed = 6.0f;
    constexpr float kMouseSensitivity = 0.0009f;
    constexpr float kMaxPitch = D3DX_PI * 0.45f;
    constexpr int kCubeGridWidth = 11;
    constexpr int kCubeGridDepth = 11;
    constexpr float kCubeSpacing = 1.2f;
}

LPDIRECT3D9 g_pD3D = NULL;
LPDIRECT3DDEVICE9 g_pd3dDevice = NULL;
LPD3DXFONT g_pFont = NULL;
LPD3DXMESH g_pMesh = NULL;
LPD3DXMESH g_pLargeCubeMesh = NULL;

std::vector<D3DMATERIAL9> g_pMaterials;
std::vector<LPDIRECT3DTEXTURE9> g_pTextures;
DWORD g_dwNumMaterials = 0;
std::vector<D3DMATERIAL9> g_pLargeCubeMaterials;
std::vector<LPDIRECT3DTEXTURE9> g_pLargeCubeTextures;
DWORD g_dwLargeCubeNumMaterials = 0;
LPD3DXEFFECT g_pEffect1 = NULL;
LPD3DXEFFECT g_pEffect2 = NULL;

bool g_bClose = false;

// === 変更: RT を 2 枚用意 ===
LPDIRECT3DTEXTURE9 g_pRenderTarget = NULL;
LPDIRECT3DTEXTURE9 g_pRenderTarget2 = NULL;

// フルスクリーンクアッド用
LPDIRECT3DVERTEXDECLARATION9 g_pQuadDecl = NULL;

// 追加: スプライト
LPD3DXSPRITE g_pSprite = NULL;
HWND g_hWnd = NULL;
bool g_bShowDebugSprite = false;
bool g_bPrevToggleKeyDown = false;
bool g_bPrevCursorToggleKeyDown = false;
bool g_bMouseCursorVisible = false;
float g_cameraYaw = -D3DX_PI * 0.25f;
float g_cameraPitch = -0.34f;
D3DXVECTOR3 g_cameraPosition(10.0f, 5.0f, -10.0f);

struct QuadVertex
{
    float x, y, z, w; // クリップ空間（-1..1, w=1）
    float u, v;       // テクスチャ座標
};

static void TextDraw(LPD3DXFONT pFont, TCHAR* text, int X, int Y);
static void InitD3D(HWND hWnd);
static void Cleanup();

static void LoadMeshWithTextures(const TCHAR* meshPath,
                                 LPD3DXMESH* ppMesh,
                                 std::vector<D3DMATERIAL9>& materials,
                                 std::vector<LPDIRECT3DTEXTURE9>& textures,
                                 DWORD* pNumMaterials);
static void SetMouseCursorVisible(bool visible);
static void UpdateInputAndCamera();
static void DrawOverlayText();
static void RenderPass1();
static void RenderPass2();
static void DrawFullscreenQuad();

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern int WINAPI _tWinMain(_In_ HINSTANCE hInstance,
                            _In_opt_ HINSTANCE hPrevInstance,
                            _In_ LPTSTR lpCmdLine,
                            _In_ int nCmdShow);

int WINAPI _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR lpCmdLine,
                     _In_ int nCmdShow)
{
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    WNDCLASSEX wc { };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = MsgProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hIcon = NULL;
    wc.hCursor = NULL;
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = _T("Window1");
    wc.hIconSm = NULL;

    ATOM atom = RegisterClassEx(&wc);
    assert(atom != 0);

    RECT rect;
    SetRect(&rect, 0, 0, kRenderWidth, kRenderHeight);
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    rect.right = rect.right - rect.left;
    rect.bottom = rect.bottom - rect.top;
    rect.top = 0;
    rect.left = 0;

    HWND hWnd = CreateWindow(_T("Window1"),
                             _T("Hello DirectX9 World !!"),
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             rect.right,
                             rect.bottom,
                             NULL,
                             NULL,
                             wc.hInstance,
                             NULL);

    g_hWnd = hWnd;
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

            UpdateInputAndCamera();
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

void TextDraw(LPD3DXFONT pFont, TCHAR* text, int X, int Y)
{
    RECT rect = { X, Y, 0, 0 };

    HRESULT hResult = pFont->DrawText(NULL,
                                      text,
                                      -1,
                                      &rect,
                                      DT_LEFT | DT_NOCLIP,
                                      D3DCOLOR_ARGB(255, 0, 0, 0));

    assert((int)hResult >= 0);
}

void InitD3D(HWND hWnd)
{
    HRESULT hResult = E_FAIL;

    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    assert(g_pD3D != NULL);

    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.BackBufferWidth = kRenderWidth;
    d3dpp.BackBufferHeight = kRenderHeight;
    d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
    d3dpp.BackBufferCount = 1;
    d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;
    d3dpp.MultiSampleQuality = 0;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
    d3dpp.hDeviceWindow = hWnd;
    d3dpp.Flags = 0;
    d3dpp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    hResult = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT,
                                   D3DDEVTYPE_HAL,
                                   hWnd,
                                   D3DCREATE_HARDWARE_VERTEXPROCESSING,
                                   &d3dpp,
                                   &g_pd3dDevice);

    if (FAILED(hResult))
    {
        hResult = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT,
                                       D3DDEVTYPE_HAL,
                                       hWnd,
                                       D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                       &d3dpp,
                                       &g_pd3dDevice);
        assert(hResult == S_OK);
    }

    hResult = D3DXCreateFont(g_pd3dDevice,
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
    assert(hResult == S_OK);

    LoadMeshWithTextures(_T("small_cube.x"), &g_pMesh, g_pMaterials, g_pTextures, &g_dwNumMaterials);
    LoadMeshWithTextures(_T("large_cube_inside.x"), &g_pLargeCubeMesh, g_pLargeCubeMaterials, g_pLargeCubeTextures, &g_dwLargeCubeNumMaterials);

    hResult = D3DXCreateEffectFromFile(g_pd3dDevice,
                                       _T("simple.fx"),
                                       NULL,
                                       NULL,
                                       D3DXSHADER_DEBUG,
                                       NULL,
                                       &g_pEffect1,
                                       NULL);
    assert(hResult == S_OK);

    hResult = D3DXCreateEffectFromFile(g_pd3dDevice,
                                       _T("simple2.fx"),
                                       NULL,
                                       NULL,
                                       D3DXSHADER_DEBUG,
                                       NULL,
                                       &g_pEffect2,
                                       NULL);
    assert(hResult == S_OK);
    // === 変更: RT を 2 枚作成（両方 A8R8G8B8） ===
    hResult = D3DXCreateTexture(g_pd3dDevice,
                                kRenderWidth, kRenderHeight,
                                1,
                                D3DUSAGE_RENDERTARGET,
                                D3DFMT_A8R8G8B8,
                                D3DPOOL_DEFAULT,
                                &g_pRenderTarget);
    assert(hResult == S_OK);

    hResult = D3DXCreateTexture(g_pd3dDevice,
                                kRenderWidth, kRenderHeight,
                                1,
                                D3DUSAGE_RENDERTARGET,
                                D3DFMT_A8R8G8B8,
                                D3DPOOL_DEFAULT,
                                &g_pRenderTarget2);
    assert(hResult == S_OK);

    // フルスクリーンクアッドの頂宣言
    D3DVERTEXELEMENT9 elems[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    hResult = g_pd3dDevice->CreateVertexDeclaration(elems, &g_pQuadDecl);
    assert(hResult == S_OK);

    // スプライト
    hResult = D3DXCreateSprite(g_pd3dDevice, &g_pSprite);
    assert(hResult == S_OK);

    while (ShowCursor(FALSE) >= 0)
    {
    }
}

void Cleanup()
{
    for (auto& texture : g_pTextures)
    {
        SAFE_RELEASE(texture);
    }
    for (auto& texture : g_pLargeCubeTextures)
    {
        SAFE_RELEASE(texture);
    }

    SAFE_RELEASE(g_pMesh);
    SAFE_RELEASE(g_pLargeCubeMesh);
    SAFE_RELEASE(g_pEffect1);
    SAFE_RELEASE(g_pEffect2);
    SAFE_RELEASE(g_pFont);

    // 追加: 解放漏れ防止
    SAFE_RELEASE(g_pRenderTarget);
    SAFE_RELEASE(g_pRenderTarget2);
    SAFE_RELEASE(g_pQuadDecl);
    SAFE_RELEASE(g_pSprite);

    SAFE_RELEASE(g_pd3dDevice);
    SAFE_RELEASE(g_pD3D);
}

void LoadMeshWithTextures(const TCHAR* meshPath,
                          LPD3DXMESH* ppMesh,
                          std::vector<D3DMATERIAL9>& materials,
                          std::vector<LPDIRECT3DTEXTURE9>& textures,
                          DWORD* pNumMaterials)
{
    HRESULT hResult = E_FAIL;
    LPD3DXBUFFER pD3DXMtrlBuffer = NULL;

    hResult = D3DXLoadMeshFromX(meshPath,
                                D3DXMESH_SYSTEMMEM,
                                g_pd3dDevice,
                                NULL,
                                &pD3DXMtrlBuffer,
                                NULL,
                                pNumMaterials,
                                ppMesh);
    assert(hResult == S_OK);

    D3DXMATERIAL* d3dxMaterials = (D3DXMATERIAL*)pD3DXMtrlBuffer->GetBufferPointer();
    materials.resize(*pNumMaterials);
    textures.resize(*pNumMaterials);

    for (DWORD i = 0; i < *pNumMaterials; i++)
    {
        materials[i] = d3dxMaterials[i].MatD3D;
        materials[i].Ambient = materials[i].Diffuse;
        textures[i] = NULL;

        std::string pTexPath(d3dxMaterials[i].pTextureFilename ? d3dxMaterials[i].pTextureFilename : "");
        if (!pTexPath.empty())
        {
            bool bUnicode = false;
#ifdef UNICODE
            bUnicode = true;
#endif
            if (!bUnicode)
            {
                hResult = D3DXCreateTextureFromFileA(g_pd3dDevice, pTexPath.c_str(), &textures[i]);
                assert(hResult == S_OK);
            }
            else
            {
                int len = MultiByteToWideChar(CP_ACP, 0, pTexPath.c_str(), -1, nullptr, 0);
                std::wstring pTexPathW(len, 0);
                MultiByteToWideChar(CP_ACP, 0, pTexPath.c_str(), -1, &pTexPathW[0], len);

                hResult = D3DXCreateTextureFromFileW(g_pd3dDevice, pTexPathW.c_str(), &textures[i]);
                assert(hResult == S_OK);
            }
        }
    }

    hResult = pD3DXMtrlBuffer->Release();
    assert(hResult == S_OK);
}

void SetMouseCursorVisible(bool visible)
{
    if (g_bMouseCursorVisible == visible)
    {
        return;
    }

    if (visible)
    {
        while (ShowCursor(TRUE) < 0)
        {
        }
    }
    else
    {
        while (ShowCursor(FALSE) >= 0)
        {
        }
    }

    g_bMouseCursorVisible = visible;
}

void UpdateInputAndCamera()
{
    const float deltaTime = 1.0f / 60.0f;
    const bool isWindowActive = (GetForegroundWindow() == g_hWnd);
    const bool toggleKeyDown = (GetAsyncKeyState('1') & 0x8000) != 0;
    const bool cursorToggleKeyDown = (GetAsyncKeyState('2') & 0x8000) != 0;

    if (toggleKeyDown && !g_bPrevToggleKeyDown)
    {
        g_bShowDebugSprite = !g_bShowDebugSprite;
    }
    g_bPrevToggleKeyDown = toggleKeyDown;

    if (cursorToggleKeyDown && !g_bPrevCursorToggleKeyDown)
    {
        SetMouseCursorVisible(!g_bMouseCursorVisible);
    }
    g_bPrevCursorToggleKeyDown = cursorToggleKeyDown;

    if (!isWindowActive)
    {
        return;
    }

    if (!g_bMouseCursorVisible)
    {
        RECT clientRect = { };
        if (GetClientRect(g_hWnd, &clientRect))
        {
            POINT clientCenter =
            {
                (clientRect.right - clientRect.left) / 2,
                (clientRect.bottom - clientRect.top) / 2
            };
            POINT screenCenter = clientCenter;
            ClientToScreen(g_hWnd, &screenCenter);

            POINT mousePos;
            if (GetCursorPos(&mousePos))
            {
                const LONG deltaX = mousePos.x - screenCenter.x;
                const LONG deltaY = mousePos.y - screenCenter.y;

                g_cameraYaw += static_cast<float>(deltaX) * kMouseSensitivity;
                g_cameraPitch -= static_cast<float>(deltaY) * kMouseSensitivity;
                g_cameraPitch = (g_cameraPitch < -kMaxPitch) ? -kMaxPitch : g_cameraPitch;
                g_cameraPitch = (g_cameraPitch > kMaxPitch) ? kMaxPitch : g_cameraPitch;
            }

            SetCursorPos(screenCenter.x, screenCenter.y);
        }
    }

    D3DXVECTOR3 forward(sinf(g_cameraYaw) * cosf(g_cameraPitch),
                        sinf(g_cameraPitch),
                        cosf(g_cameraYaw) * cosf(g_cameraPitch));
    D3DXVec3Normalize(&forward, &forward);

    D3DXVECTOR3 worldUp(0.0f, 1.0f, 0.0f);
    D3DXVECTOR3 right;
    D3DXVec3Cross(&right, &worldUp, &forward);
    D3DXVec3Normalize(&right, &right);

    D3DXVECTOR3 move(0.0f, 0.0f, 0.0f);
    if (GetAsyncKeyState('W') & 0x8000) { move += forward; }
    if (GetAsyncKeyState('S') & 0x8000) { move -= forward; }
    if (GetAsyncKeyState('D') & 0x8000) { move += right; }
    if (GetAsyncKeyState('A') & 0x8000) { move -= right; }
    if (GetAsyncKeyState('E') & 0x8000) { move.y += 1.0f; }
    if (GetAsyncKeyState('Q') & 0x8000) { move.y -= 1.0f; }

    if (D3DXVec3LengthSq(&move) > 0.0f)
    {
        D3DXVec3Normalize(&move, &move);
        g_cameraPosition += move * (kCameraMoveSpeed * deltaTime);
    }
}

void DrawOverlayText()
{
    TCHAR lines[][128] =
    {
        _T("SSAO sample controls"),
        _T("W/A/S/D: move"),
        _T("E / Q: move up / down"),
        _T("Mouse: look around"),
        _T("1: toggle debug sprite"),
        _T("2: toggle mouse cursor"),
    };

    for (int i = 0; i < _countof(lines); ++i)
    {
        TextDraw(g_pFont, lines[i], 8, 8 + i * 22);
    }
}

void RenderPass1()
{
    HRESULT hResult = E_FAIL;

    // 既存の RT0 を保存
    LPDIRECT3DSURFACE9 pOldRT0 = NULL;
    hResult = g_pd3dDevice->GetRenderTarget(0, &pOldRT0);
    assert(hResult == S_OK);

    // 2 枚の RT サーフェスを取得
    LPDIRECT3DSURFACE9 pRT0 = NULL;
    LPDIRECT3DSURFACE9 pRT1 = NULL;
    hResult = g_pRenderTarget->GetSurfaceLevel(0, &pRT0);  assert(hResult == S_OK);
    hResult = g_pRenderTarget2->GetSurfaceLevel(0, &pRT1); assert(hResult == S_OK);

    // MRT セット（スロット 0 と 1）
    hResult = g_pd3dDevice->SetRenderTarget(0, pRT0); assert(hResult == S_OK);
    hResult = g_pd3dDevice->SetRenderTarget(1, pRT1); assert(hResult == S_OK);

    D3DXMATRIX View, Proj;

    D3DXMatrixPerspectiveFovLH(&Proj,
                               D3DXToRadian(45),
                               static_cast<float>(kRenderWidth) / static_cast<float>(kRenderHeight),
                               1.0f,
                               10000.0f);

    D3DXVECTOR3 forward(sinf(g_cameraYaw) * cosf(g_cameraPitch),
                        sinf(g_cameraPitch),
                        cosf(g_cameraYaw) * cosf(g_cameraPitch));
    D3DXVECTOR3 eye = g_cameraPosition;
    D3DXVECTOR3 at = eye + forward;
    D3DXVECTOR3 up(0, 1, 0);
    D3DXMatrixLookAtLH(&View, &eye, &at, &up);
    hResult = g_pd3dDevice->Clear(0, NULL,
                                  D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                                  D3DCOLOR_XRGB(100, 100, 100),
                                  1.0f, 0);
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->BeginScene(); assert(hResult == S_OK);

    // === 変更: MRT 用テクニックを使用 ===
    hResult = g_pEffect1->SetTechnique("TechniqueMRT");
    assert(hResult == S_OK);

    UINT numPass = 0;
    hResult = g_pEffect1->Begin(&numPass, 0); assert(hResult == S_OK);
    hResult = g_pEffect1->BeginPass(0);       assert(hResult == S_OK);

    // 4x4 テクスチャの小さなキューブをたくさん配置
    hResult = g_pEffect1->SetBool("g_bUseTexture", TRUE); assert(hResult == S_OK);

    D3DXMATRIX largeCubeWorld;
    D3DXMATRIX largeCubeWorldViewProj;
    D3DXMatrixTranslation(&largeCubeWorld, 0.0f, 0.0f, 0.0f);
    largeCubeWorldViewProj = largeCubeWorld * View * Proj;
    hResult = g_pEffect1->SetMatrix("g_matWorldViewProj", &largeCubeWorldViewProj); assert(hResult == S_OK);
    for (DWORD i = 0; i < g_dwLargeCubeNumMaterials; i++)
    {
        hResult = g_pEffect1->SetTexture("texture1", g_pLargeCubeTextures[i]); assert(hResult == S_OK);
        hResult = g_pEffect1->CommitChanges();                                  assert(hResult == S_OK);
        hResult = g_pLargeCubeMesh->DrawSubset(i);                              assert(hResult == S_OK);
    }

    for (int z = 0; z < kCubeGridDepth; ++z)
    {
        for (int x = 0; x < kCubeGridWidth; ++x)
        {
            D3DXMATRIX world;
            D3DXMATRIX worldViewProj;

            const float offsetX = (x - (kCubeGridWidth - 1) * 0.5f) * kCubeSpacing;
            const float offsetZ = (z - (kCubeGridDepth - 1) * 0.5f) * kCubeSpacing;
            const float offsetY = 0.15f * sinf(static_cast<float>(x) * 0.9f) + 0.15f * cosf(static_cast<float>(z) * 0.8f);

            D3DXMatrixTranslation(&world, offsetX, offsetY, offsetZ);
            worldViewProj = world * View * Proj;

            hResult = g_pEffect1->SetMatrix("g_matWorldViewProj", &worldViewProj); assert(hResult == S_OK);

            for (DWORD i = 0; i < g_dwNumMaterials; i++)
            {
                hResult = g_pEffect1->SetTexture("texture1", g_pTextures[i]); assert(hResult == S_OK);
                hResult = g_pEffect1->CommitChanges();                         assert(hResult == S_OK);
                hResult = g_pMesh->DrawSubset(i);                              assert(hResult == S_OK);
            }
        }
    }

    hResult = g_pEffect1->EndPass(); assert(hResult == S_OK);
    hResult = g_pEffect1->End();     assert(hResult == S_OK);

    hResult = g_pd3dDevice->EndScene(); assert(hResult == S_OK);

    // MRT を解除してバックバッファへ戻す
    hResult = g_pd3dDevice->SetRenderTarget(1, NULL);   assert(hResult == S_OK);
    hResult = g_pd3dDevice->SetRenderTarget(0, pOldRT0); assert(hResult == S_OK);

    SAFE_RELEASE(pRT0);
    SAFE_RELEASE(pRT1);
    SAFE_RELEASE(pOldRT0);
}

void RenderPass2()
{
    HRESULT hResult = E_FAIL;

    hResult = g_pd3dDevice->Clear(0, NULL,
                                  D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                                  D3DCOLOR_XRGB(0, 0, 0),
                                  1.0f, 0);
    assert(hResult == S_OK);

    // 2D 全面描画なので Z 無効
    hResult = g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->BeginScene(); assert(hResult == S_OK);

    // フルスクリーン: RT0 を simple2.fx で表示
    hResult = g_pEffect2->SetTechnique("Technique1");       assert(hResult == S_OK);

    UINT numPass = 0;
    hResult = g_pEffect2->Begin(&numPass, 0);               assert(hResult == S_OK);
    hResult = g_pEffect2->BeginPass(0);                     assert(hResult == S_OK);

    hResult = g_pEffect2->SetTexture("texture1", g_pRenderTarget); assert(hResult == S_OK);
    hResult = g_pEffect2->CommitChanges();                          assert(hResult == S_OK);

    DrawFullscreenQuad();

    hResult = g_pEffect2->EndPass(); assert(hResult == S_OK);
    hResult = g_pEffect2->End();     assert(hResult == S_OK);

    // === 追加: 左上に RT1 を 1/2 スケールで表示（D3DXSPRITE） ===
    if (g_bShowDebugSprite && g_pSprite)
    {
        hResult = g_pSprite->Begin(D3DXSPRITE_ALPHABLEND);  assert(hResult == S_OK);

        D3DXMATRIX mat;
        D3DXVECTOR2 scaling(0.5f, 0.5f);     // 半分
        D3DXVECTOR2 trans(0.0f, 0.0f);       // 左上
        D3DXMatrixTransformation2D(&mat, NULL, 0.0f, &scaling, NULL, 0.0f, &trans);
        g_pSprite->SetTransform(&mat);

        // そのまま (0,0) へ描画
        hResult = g_pSprite->Draw(g_pRenderTarget2, NULL, NULL, NULL, 0xFFFFFFFF);
        assert(hResult == S_OK);

        hResult = g_pSprite->End(); assert(hResult == S_OK);
    }

    DrawOverlayText();

    hResult = g_pd3dDevice->EndScene();  assert(hResult == S_OK);
    hResult = g_pd3dDevice->Present(NULL, NULL, NULL, NULL); assert(hResult == S_OK);

    hResult = g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
    assert(hResult == S_OK);
}

void DrawFullscreenQuad()
{
    QuadVertex v[4] { };

    float du = 0.5f / static_cast<float>(kRenderWidth);
    float dv = 0.5f / static_cast<float>(kRenderHeight);

    v[0].x = -1.0f; v[0].y = -1.0f; v[0].z = 0.0f; v[0].w = 1.0f; v[0].u = 0.0f + du; v[0].v = 1.0f - dv;
    v[1].x = -1.0f; v[1].y = 1.0f; v[1].z = 0.0f; v[1].w = 1.0f; v[1].u = 0.0f + du; v[1].v = 0.0f + dv;
    v[2].x = 1.0f; v[2].y = -1.0f; v[2].z = 0.0f; v[2].w = 1.0f; v[2].u = 1.0f - du; v[2].v = 1.0f - dv;
    v[3].x = 1.0f; v[3].y = 1.0f; v[3].z = 0.0f; v[3].w = 1.0f; v[3].u = 1.0f - du; v[3].v = 0.0f + dv;

    g_pd3dDevice->SetVertexDeclaration(g_pQuadDecl);
    g_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(QuadVertex));
}

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        g_bClose = true;
        return 0;
    }
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}
