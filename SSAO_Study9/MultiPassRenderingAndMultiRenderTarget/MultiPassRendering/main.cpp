#pragma comment( lib, "d3d9.lib" )
#if defined(DEBUG) || defined(_DEBUG)
#pragma comment( lib, "d3dx9d.lib" )
#else
#pragma comment( lib, "d3dx9.lib" )
#endif

#include <d3d9.h>
#include <d3dx9.h>
#include <commdlg.h>
#include <string>
#include <tchar.h>
#include <cassert>
#include <crtdbg.h>
#include <cmath>
#include <map>
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
    constexpr float kUserMeshPlacementDistance = 4.5f;
    constexpr int kToolDialogButtonId = 1001;
    constexpr int kToolDialogSsaoSampleEditId = 1002;
    constexpr int kToolDialogApplySsaoButtonId = 1003;
}

LPDIRECT3D9 g_pD3D = NULL;
LPDIRECT3DDEVICE9 g_pd3dDevice = NULL;
LPD3DXFONT g_pFont = NULL;
LPD3DXMESH g_pMesh = NULL;
LPD3DXMESH g_pLargeCubeMesh = NULL;
LPD3DXMESH g_pPlateMesh = NULL;

std::vector<D3DMATERIAL9> g_pMaterials;
std::vector<LPDIRECT3DTEXTURE9> g_pTextures;
DWORD g_dwNumMaterials = 0;
std::vector<D3DMATERIAL9> g_pLargeCubeMaterials;
std::vector<LPDIRECT3DTEXTURE9> g_pLargeCubeTextures;
DWORD g_dwLargeCubeNumMaterials = 0;
std::vector<D3DMATERIAL9> g_pPlateMaterials;
std::vector<LPDIRECT3DTEXTURE9> g_pPlateTextures;
DWORD g_dwPlateNumMaterials = 0;
std::map<std::wstring, LPDIRECT3DTEXTURE9> g_textureCache;
LPD3DXEFFECT g_pEffect1 = NULL;
LPD3DXEFFECT g_pEffect2 = NULL;

bool g_bClose = false;

// === 変更: RT を 2 枚用意 ===
LPDIRECT3DTEXTURE9 g_pRenderTarget = NULL;
LPDIRECT3DTEXTURE9 g_pDepthRenderTarget = NULL;
LPDIRECT3DTEXTURE9 g_pNormalRenderTarget = NULL;

// フルスクリーンクアッド用
LPDIRECT3DVERTEXDECLARATION9 g_pQuadDecl = NULL;

// 追加: スプライト
LPD3DXSPRITE g_pSprite = NULL;
HWND g_hWnd = NULL;
bool g_bShowDebugSprite = false;
bool g_bPrevDepthInfoKeyDown = false;
bool g_bPrevNormalInfoKeyDown = false;
bool g_bPrevCursorToggleKeyDown = false;
bool g_bPrevLambertToggleKeyDown = false;
bool g_bPrevDialogToggleKeyDown = false;
bool g_bPrevSimpleSsaoToggleKeyDown = false;
bool g_bPrevEscapeToggleKeyDown = false;
bool g_bMouseCursorVisible = false;
bool g_bUseLambertLighting = true;
bool g_bEnableSimpleSsao = true;
bool g_bShowNormalInfo = false;
float g_simpleSsaoSamplePixels = 20.0f;
float g_cameraYaw = -D3DX_PI * 0.25f;
float g_cameraPitch = -0.34f;
D3DXVECTOR3 g_cameraPosition(2.0f, 1.0f, -3.0f);
HWND g_hToolDialog = NULL;
HWND g_hOpenMeshButton = NULL;
HWND g_hSsaoSampleEdit = NULL;
HWND g_hApplySsaoButton = NULL;

struct UserMeshInstance
{
    LPD3DXMESH mesh = NULL;
    std::vector<D3DMATERIAL9> materials;
    std::vector<LPDIRECT3DTEXTURE9> textures;
    DWORD numMaterials = 0;
    D3DXVECTOR3 position = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    float yaw = 0.0f;
};

std::vector<UserMeshInstance> g_userMeshes;
std::vector<UserMeshInstance> g_sceneMeshes;

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
static std::wstring GetDirectoryFromPath(const std::wstring& path);
static std::wstring ResolveTexturePath(const std::wstring& meshPath, const char* textureFilename);
static LPDIRECT3DTEXTURE9 LoadTextureCached(const std::wstring& texturePath);
static void ReleaseMeshOnly(LPD3DXMESH* ppMesh,
                            std::vector<D3DMATERIAL9>& materials,
                            std::vector<LPDIRECT3DTEXTURE9>& textures,
                            DWORD* pNumMaterials);
static void CreateToolDialog();
static void ToggleToolDialog();
static void OpenMeshFileDialog();
static void PlaceUserMeshAtCurrentLookTarget(UserMeshInstance& userMesh);
static void LoadSceneMeshInstance(const TCHAR* meshPath, const D3DXVECTOR3& position, float yaw);
static void SetMouseCursorVisible(bool visible);
static void UpdateInputAndCamera();
static void DrawOverlayText();
static void RenderPass1();
static void RenderPass2();
static void DrawFullscreenQuad();

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ToolDialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
            TranslateMessage(&msg);
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
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24X8;
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

    LoadMeshWithTextures(_T("resource\\small_cube.x"), &g_pMesh, g_pMaterials, g_pTextures, &g_dwNumMaterials);
    LoadMeshWithTextures(_T("resource\\large_cube_inside.x"), &g_pLargeCubeMesh, g_pLargeCubeMaterials, g_pLargeCubeTextures, &g_dwLargeCubeNumMaterials);
    LoadMeshWithTextures(_T("resource\\plate.x"), &g_pPlateMesh, g_pPlateMaterials, g_pPlateTextures, &g_dwPlateNumMaterials);
    LoadSceneMeshInstance(_T("resource\\cube_red.x"), D3DXVECTOR3(-8.0f, 0.5f, -6.0f), 0.2f);
    LoadSceneMeshInstance(_T("resource\\cube_green.x"), D3DXVECTOR3(-3.5f, 0.5f, -7.5f), 0.8f);
    LoadSceneMeshInstance(_T("resource\\cube_blue.x"), D3DXVECTOR3(2.5f, 0.5f, -6.5f), -0.4f);
    LoadSceneMeshInstance(_T("resource\\sphere_orange.x"), D3DXVECTOR3(-6.5f, 1.2f, 3.0f), 0.0f);
    LoadSceneMeshInstance(_T("resource\\sphere_pink.x"), D3DXVECTOR3(-1.5f, 1.2f, 5.0f), 0.0f);
    LoadSceneMeshInstance(_T("resource\\sphere_yellowgreen.x"), D3DXVECTOR3(4.5f, 1.2f, 4.0f), 0.0f);
    LoadSceneMeshInstance(_T("resource\\cube_white.x"), D3DXVECTOR3(-8.8f, 0.5f, -1.0f), 0.1f);
    LoadSceneMeshInstance(_T("resource\\cube_black.x"), D3DXVECTOR3(8.6f, 0.5f, 2.5f), -0.2f);
    LoadSceneMeshInstance(_T("resource\\sphere_orange.x"), D3DXVECTOR3(-8.6f, 8.4f, 5.5f), 0.0f);
    LoadSceneMeshInstance(_T("resource\\sphere_pink.x"), D3DXVECTOR3(0.0f, 8.5f, -7.2f), 0.0f);
    LoadSceneMeshInstance(_T("resource\\cube_red.x"), D3DXVECTOR3(6.8f, 8.3f, 8.6f), 0.6f);
    LoadSceneMeshInstance(_T("resource\\cube_green.x"), D3DXVECTOR3(-4.0f, 8.2f, -8.8f), -0.5f);

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
                                D3DFMT_R32F,
                                D3DPOOL_DEFAULT,
                                &g_pDepthRenderTarget);
    assert(hResult == S_OK);

    hResult = D3DXCreateTexture(g_pd3dDevice,
                                kRenderWidth, kRenderHeight,
                                1,
                                D3DUSAGE_RENDERTARGET,
                                D3DFMT_A8R8G8B8,
                                D3DPOOL_DEFAULT,
                                &g_pNormalRenderTarget);
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
    for (auto& entry : g_textureCache)
    {
        SAFE_RELEASE(entry.second);
    }
    g_textureCache.clear();

    SAFE_RELEASE(g_pMesh);
    SAFE_RELEASE(g_pLargeCubeMesh);
    SAFE_RELEASE(g_pPlateMesh);
    for (auto& userMesh : g_userMeshes)
    {
        SAFE_RELEASE(userMesh.mesh);
    }
    g_userMeshes.clear();
    for (auto& sceneMesh : g_sceneMeshes)
    {
        SAFE_RELEASE(sceneMesh.mesh);
    }
    g_sceneMeshes.clear();
    SAFE_RELEASE(g_pEffect1);
    SAFE_RELEASE(g_pEffect2);
    SAFE_RELEASE(g_pFont);

    // 追加: 解放漏れ防止
    SAFE_RELEASE(g_pRenderTarget);
    SAFE_RELEASE(g_pDepthRenderTarget);
    SAFE_RELEASE(g_pNormalRenderTarget);
    SAFE_RELEASE(g_pQuadDecl);
    SAFE_RELEASE(g_pSprite);
    if (g_hToolDialog)
    {
        DestroyWindow(g_hToolDialog);
        g_hToolDialog = NULL;
    }

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
    std::wstring meshPathW(meshPath);

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

        std::wstring texturePath = ResolveTexturePath(meshPathW, d3dxMaterials[i].pTextureFilename);
        if (!texturePath.empty())
        {
            textures[i] = LoadTextureCached(texturePath);
            assert(textures[i] != NULL);
        }
    }

    hResult = pD3DXMtrlBuffer->Release();
    assert(hResult == S_OK);
}

std::wstring GetDirectoryFromPath(const std::wstring& path)
{
    const size_t pos = path.find_last_of(L"\\/");
    if (pos == std::wstring::npos)
    {
        return L"";
    }
    return path.substr(0, pos + 1);
}

std::wstring ResolveTexturePath(const std::wstring& meshPath, const char* textureFilename)
{
    if (!textureFilename || !textureFilename[0])
    {
        return L"";
    }

    const int len = MultiByteToWideChar(CP_ACP, 0, textureFilename, -1, nullptr, 0);
    std::wstring texturePath(static_cast<size_t>(len - 1), L'\0');
    MultiByteToWideChar(CP_ACP, 0, textureFilename, -1, &texturePath[0], len);

    const bool isAbsolute = (texturePath.size() >= 2 && texturePath[1] == L':') ||
                            (texturePath.size() >= 2 && texturePath[0] == L'\\' && texturePath[1] == L'\\');
    if (isAbsolute)
    {
        return texturePath;
    }

    return GetDirectoryFromPath(meshPath) + texturePath;
}

LPDIRECT3DTEXTURE9 LoadTextureCached(const std::wstring& texturePath)
{
    auto it = g_textureCache.find(texturePath);
    if (it != g_textureCache.end())
    {
        return it->second;
    }

    HRESULT hResult = E_FAIL;
    LPDIRECT3DTEXTURE9 pTexture = NULL;
    hResult = D3DXCreateTextureFromFileW(g_pd3dDevice, texturePath.c_str(), &pTexture);
    assert(hResult == S_OK);

    g_textureCache[texturePath] = pTexture;
    return pTexture;
}

void ReleaseMeshOnly(LPD3DXMESH* ppMesh,
                     std::vector<D3DMATERIAL9>& materials,
                     std::vector<LPDIRECT3DTEXTURE9>& textures,
                     DWORD* pNumMaterials)
{
    SAFE_RELEASE(*ppMesh);
    materials.clear();
    textures.clear();
    *pNumMaterials = 0;
}

void CreateToolDialog()
{
    if (g_hToolDialog)
    {
        return;
    }

    WNDCLASS wc = { };
    wc.lpfnWndProc = ToolDialogProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = _T("MeshToolDialogClass");
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClass(&wc);

    g_hToolDialog = CreateWindowEx(WS_EX_TOOLWINDOW,
                                   wc.lpszClassName,
                                   _T("Mesh Loader"),
                                   WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                   CW_USEDEFAULT,
                                   CW_USEDEFAULT,
                                   320,
                                   180,
                                   g_hWnd,
                                   NULL,
                                   wc.hInstance,
                                   NULL);
    assert(g_hToolDialog != NULL);

    g_hOpenMeshButton = CreateWindow(_T("BUTTON"),
                                     _T("Open X File"),
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     20,
                                     20,
                                     200,
                                     32,
                                     g_hToolDialog,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogButtonId)),
                                     wc.hInstance,
                                     NULL);
    assert(g_hOpenMeshButton != NULL);

    CreateWindow(_T("STATIC"),
                 _T("SSAO sample pixels:"),
                 WS_CHILD | WS_VISIBLE,
                 20,
                 72,
                 130,
                 20,
                 g_hToolDialog,
                 NULL,
                 wc.hInstance,
                 NULL);

    g_hSsaoSampleEdit = CreateWindow(_T("EDIT"),
                                     _T("20.0"),
                                     WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                     160,
                                     68,
                                     60,
                                     24,
                                     g_hToolDialog,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogSsaoSampleEditId)),
                                     wc.hInstance,
                                     NULL);
    assert(g_hSsaoSampleEdit != NULL);

    g_hApplySsaoButton = CreateWindow(_T("BUTTON"),
                                      _T("Apply"),
                                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                      230,
                                      66,
                                      60,
                                      28,
                                      g_hToolDialog,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogApplySsaoButtonId)),
                                      wc.hInstance,
                                      NULL);
    assert(g_hApplySsaoButton != NULL);
}

void ToggleToolDialog()
{
    CreateToolDialog();

    const bool showDialog = !IsWindowVisible(g_hToolDialog);
    ShowWindow(g_hToolDialog, showDialog ? SW_SHOW : SW_HIDE);
    if (showDialog)
    {
        SetMouseCursorVisible(true);
        SetForegroundWindow(g_hToolDialog);
    }
}

void OpenMeshFileDialog()
{
    OPENFILENAME ofn = { };
    TCHAR filePath[MAX_PATH] = { };

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hToolDialog ? g_hToolDialog : g_hWnd;
    ofn.lpstrFilter = _T("X Files (*.x)\0*.x\0All Files (*.*)\0*.*\0");
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = _countof(filePath);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = _T("Open X File");

    if (!GetOpenFileName(&ofn))
    {
        return;
    }

    UserMeshInstance userMesh;
    LoadMeshWithTextures(filePath, &userMesh.mesh, userMesh.materials, userMesh.textures, &userMesh.numMaterials);
    PlaceUserMeshAtCurrentLookTarget(userMesh);
    g_userMeshes.push_back(userMesh);
}

void PlaceUserMeshAtCurrentLookTarget(UserMeshInstance& userMesh)
{
    D3DXVECTOR3 forward(sinf(g_cameraYaw) * cosf(g_cameraPitch),
                        sinf(g_cameraPitch),
                        cosf(g_cameraYaw) * cosf(g_cameraPitch));
    D3DXVec3Normalize(&forward, &forward);

    userMesh.position = g_cameraPosition + forward * kUserMeshPlacementDistance;

    D3DXVECTOR3 toCamera = g_cameraPosition - userMesh.position;
    userMesh.yaw = atan2f(toCamera.x, toCamera.z) + D3DX_PI;
}

void LoadSceneMeshInstance(const TCHAR* meshPath, const D3DXVECTOR3& position, float yaw)
{
    UserMeshInstance sceneMesh;
    LoadMeshWithTextures(meshPath, &sceneMesh.mesh, sceneMesh.materials, sceneMesh.textures, &sceneMesh.numMaterials);
    sceneMesh.position = position;
    sceneMesh.yaw = yaw;
    g_sceneMeshes.push_back(sceneMesh);
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
    const HWND foregroundWindow = GetForegroundWindow();
    const bool isMainWindowActive = (foregroundWindow == g_hWnd);
    const bool isToolDialogActive = (g_hToolDialog != NULL && foregroundWindow == g_hToolDialog);
    const bool depthInfoKeyDown = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
    const bool normalInfoKeyDown = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
    const bool cursorToggleKeyDown = (GetAsyncKeyState('2') & 0x8000) != 0;
    const bool lambertToggleKeyDown = (GetAsyncKeyState('3') & 0x8000) != 0;
    const bool dialogToggleKeyDown = (GetAsyncKeyState('4') & 0x8000) != 0;
    const bool simpleSsaoToggleKeyDown = (GetAsyncKeyState('5') & 0x8000) != 0;
    const bool escapeToggleKeyDown = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;

    if (isToolDialogActive)
    {
        g_bPrevDepthInfoKeyDown = depthInfoKeyDown;
        g_bPrevNormalInfoKeyDown = normalInfoKeyDown;
        g_bPrevCursorToggleKeyDown = cursorToggleKeyDown;
        g_bPrevLambertToggleKeyDown = lambertToggleKeyDown;
        g_bPrevDialogToggleKeyDown = dialogToggleKeyDown;
        g_bPrevSimpleSsaoToggleKeyDown = simpleSsaoToggleKeyDown;
        g_bPrevEscapeToggleKeyDown = escapeToggleKeyDown;
        return;
    }

    if (depthInfoKeyDown && !g_bPrevDepthInfoKeyDown)
    {
        if (g_bShowDebugSprite && !g_bShowNormalInfo)
        {
            g_bShowDebugSprite = false;
        }
        else
        {
            g_bShowDebugSprite = true;
            g_bShowNormalInfo = false;
        }
    }
    g_bPrevDepthInfoKeyDown = depthInfoKeyDown;

    if (normalInfoKeyDown && !g_bPrevNormalInfoKeyDown)
    {
        if (g_bShowDebugSprite && g_bShowNormalInfo)
        {
            g_bShowDebugSprite = false;
            g_bShowNormalInfo = false;
        }
        else
        {
            g_bShowDebugSprite = true;
            g_bShowNormalInfo = true;
        }
    }
    g_bPrevNormalInfoKeyDown = normalInfoKeyDown;

    if (cursorToggleKeyDown && !g_bPrevCursorToggleKeyDown)
    {
        SetMouseCursorVisible(!g_bMouseCursorVisible);
    }
    g_bPrevCursorToggleKeyDown = cursorToggleKeyDown;

    if (escapeToggleKeyDown && !g_bPrevEscapeToggleKeyDown)
    {
        SetMouseCursorVisible(!g_bMouseCursorVisible);
    }
    g_bPrevEscapeToggleKeyDown = escapeToggleKeyDown;

    if (lambertToggleKeyDown && !g_bPrevLambertToggleKeyDown)
    {
        g_bUseLambertLighting = !g_bUseLambertLighting;
    }
    g_bPrevLambertToggleKeyDown = lambertToggleKeyDown;

    if (dialogToggleKeyDown && !g_bPrevDialogToggleKeyDown)
    {
        ToggleToolDialog();
    }
    g_bPrevDialogToggleKeyDown = dialogToggleKeyDown;

    if (simpleSsaoToggleKeyDown && !g_bPrevSimpleSsaoToggleKeyDown)
    {
        g_bEnableSimpleSsao = !g_bEnableSimpleSsao;
    }
    g_bPrevSimpleSsaoToggleKeyDown = simpleSsaoToggleKeyDown;

    if (!isMainWindowActive)
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
        _T("F1: show depth info"),
        _T("F2: show normal info"),
        _T("2 / Esc: toggle mouse cursor"),
        _T("3: toggle lambert lighting"),
        _T("4: toggle mesh dialog"),
        _T("5: toggle simple SSAO"),
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
    LPDIRECT3DSURFACE9 pRT2 = NULL;
    hResult = g_pRenderTarget->GetSurfaceLevel(0, &pRT0);  assert(hResult == S_OK);
    hResult = g_pDepthRenderTarget->GetSurfaceLevel(0, &pRT1); assert(hResult == S_OK);
    hResult = g_pNormalRenderTarget->GetSurfaceLevel(0, &pRT2); assert(hResult == S_OK);

    // MRT セット
    hResult = g_pd3dDevice->SetRenderTarget(0, pRT0); assert(hResult == S_OK);
    hResult = g_pd3dDevice->SetRenderTarget(1, pRT1); assert(hResult == S_OK);
    hResult = g_pd3dDevice->SetRenderTarget(2, pRT2); assert(hResult == S_OK);

    D3DXMATRIX View, Proj;

    D3DXMatrixPerspectiveFovLH(&Proj,
                               D3DXToRadian(45),
                               static_cast<float>(kRenderWidth) / static_cast<float>(kRenderHeight),
                               0.1f,
                               50.0f);

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
    hResult = g_pEffect1->SetBool("g_bUseLambert", g_bUseLambertLighting ? TRUE : FALSE); assert(hResult == S_OK);
    hResult = g_pEffect1->SetBool("g_bShowNormalInfo", g_bShowNormalInfo ? TRUE : FALSE); assert(hResult == S_OK);

    D3DXMATRIX largeCubeWorld;
    D3DXMATRIX largeCubeWorldView;
    D3DXMATRIX largeCubeWorldViewProj;
    D3DXMatrixTranslation(&largeCubeWorld, 0.0f, 0.0f, 0.0f);
    largeCubeWorldView = largeCubeWorld * View;
    largeCubeWorldViewProj = largeCubeWorld * View * Proj;
    hResult = g_pEffect1->SetMatrix("g_matWorldView", &largeCubeWorldView); assert(hResult == S_OK);
    hResult = g_pEffect1->SetMatrix("g_matWorldViewProj", &largeCubeWorldViewProj); assert(hResult == S_OK);
    for (DWORD i = 0; i < g_dwLargeCubeNumMaterials; i++)
    {
        hResult = g_pEffect1->SetTexture("texture1", g_pLargeCubeTextures[i]); assert(hResult == S_OK);
        hResult = g_pEffect1->CommitChanges();                                  assert(hResult == S_OK);
        hResult = g_pLargeCubeMesh->DrawSubset(i);                              assert(hResult == S_OK);
    }

    D3DXMATRIX plateWorld;
    D3DXMATRIX plateWorldView;
    D3DXMATRIX plateWorldViewProj;
    D3DXMatrixTranslation(&plateWorld, 0.0f, 0.0f, 0.0f);
    plateWorldView = plateWorld * View;
    plateWorldViewProj = plateWorld * View * Proj;
    hResult = g_pEffect1->SetMatrix("g_matWorldView", &plateWorldView); assert(hResult == S_OK);
    hResult = g_pEffect1->SetMatrix("g_matWorldViewProj", &plateWorldViewProj); assert(hResult == S_OK);
    for (DWORD i = 0; i < g_dwPlateNumMaterials; i++)
    {
        hResult = g_pEffect1->SetTexture("texture1", g_pPlateTextures[i]); assert(hResult == S_OK);
        hResult = g_pEffect1->CommitChanges();                              assert(hResult == S_OK);
        hResult = g_pPlateMesh->DrawSubset(i);                              assert(hResult == S_OK);
    }

    for (const auto& sceneMesh : g_sceneMeshes)
    {
        D3DXMATRIX sceneMeshWorld;
        D3DXMATRIX sceneMeshRotation;
        D3DXMATRIX sceneMeshTranslation;
        D3DXMATRIX sceneMeshWorldView;
        D3DXMATRIX sceneMeshWorldViewProj;
        D3DXMatrixRotationY(&sceneMeshRotation, sceneMesh.yaw);
        D3DXMatrixTranslation(&sceneMeshTranslation, sceneMesh.position.x, sceneMesh.position.y, sceneMesh.position.z);
        sceneMeshWorld = sceneMeshRotation * sceneMeshTranslation;
        sceneMeshWorldView = sceneMeshWorld * View;
        sceneMeshWorldViewProj = sceneMeshWorld * View * Proj;
        hResult = g_pEffect1->SetMatrix("g_matWorldView", &sceneMeshWorldView); assert(hResult == S_OK);
        hResult = g_pEffect1->SetMatrix("g_matWorldViewProj", &sceneMeshWorldViewProj); assert(hResult == S_OK);
        for (DWORD i = 0; i < sceneMesh.numMaterials; i++)
        {
            hResult = g_pEffect1->SetTexture("texture1", sceneMesh.textures[i]); assert(hResult == S_OK);
            hResult = g_pEffect1->CommitChanges();                                assert(hResult == S_OK);
            hResult = sceneMesh.mesh->DrawSubset(i);                              assert(hResult == S_OK);
        }
    }

    for (const auto& userMesh : g_userMeshes)
    {
        D3DXMATRIX userMeshWorld;
        D3DXMATRIX userMeshRotation;
        D3DXMATRIX userMeshTranslation;
        D3DXMATRIX userMeshWorldView;
        D3DXMATRIX userMeshWorldViewProj;
        D3DXMatrixRotationY(&userMeshRotation, userMesh.yaw);
        D3DXMatrixTranslation(&userMeshTranslation, userMesh.position.x, userMesh.position.y, userMesh.position.z);
        userMeshWorld = userMeshRotation * userMeshTranslation;
        userMeshWorldView = userMeshWorld * View;
        userMeshWorldViewProj = userMeshWorld * View * Proj;
        hResult = g_pEffect1->SetMatrix("g_matWorldView", &userMeshWorldView); assert(hResult == S_OK);
        hResult = g_pEffect1->SetMatrix("g_matWorldViewProj", &userMeshWorldViewProj); assert(hResult == S_OK);
        for (DWORD i = 0; i < userMesh.numMaterials; i++)
        {
            hResult = g_pEffect1->SetTexture("texture1", userMesh.textures[i]); assert(hResult == S_OK);
            hResult = g_pEffect1->CommitChanges();                                assert(hResult == S_OK);
            hResult = userMesh.mesh->DrawSubset(i);                               assert(hResult == S_OK);
        }
    }

    for (int z = 0; z < kCubeGridDepth; ++z)
    {
        for (int x = 0; x < kCubeGridWidth; ++x)
        {
            D3DXMATRIX world;
            D3DXMATRIX worldView;
            D3DXMATRIX worldViewProj;

            const float offsetX = (x - (kCubeGridWidth - 1) * 0.5f) * kCubeSpacing;
            const float offsetZ = (z - (kCubeGridDepth - 1) * 0.5f) * kCubeSpacing;
            const float offsetY = 0.15f * sinf(static_cast<float>(x) * 0.9f) + 0.15f * cosf(static_cast<float>(z) * 0.8f);

            D3DXMatrixTranslation(&world, offsetX, offsetY, offsetZ);
            worldView = world * View;
            worldViewProj = world * View * Proj;

            hResult = g_pEffect1->SetMatrix("g_matWorldView", &worldView); assert(hResult == S_OK);
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
    hResult = g_pd3dDevice->SetRenderTarget(2, NULL);   assert(hResult == S_OK);
    hResult = g_pd3dDevice->SetRenderTarget(1, NULL);   assert(hResult == S_OK);
    hResult = g_pd3dDevice->SetRenderTarget(0, pOldRT0); assert(hResult == S_OK);

    SAFE_RELEASE(pRT0);
    SAFE_RELEASE(pRT1);
    SAFE_RELEASE(pRT2);
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

    hResult = g_pEffect2->SetBool("g_bEnableSimpleSsao", g_bEnableSimpleSsao ? TRUE : FALSE); assert(hResult == S_OK);
    hResult = g_pEffect2->SetFloat("g_simpleSsaoSamplePixels", g_simpleSsaoSamplePixels); assert(hResult == S_OK);
    hResult = g_pEffect2->SetTexture("texture1", g_pRenderTarget); assert(hResult == S_OK);
    hResult = g_pEffect2->SetTexture("depthTexture", g_pDepthRenderTarget); assert(hResult == S_OK);
    hResult = g_pEffect2->SetTexture("normalTexture", g_pNormalRenderTarget); assert(hResult == S_OK);
    hResult = g_pEffect2->CommitChanges();                          assert(hResult == S_OK);

    DrawFullscreenQuad();

    hResult = g_pEffect2->EndPass(); assert(hResult == S_OK);
    hResult = g_pEffect2->End();     assert(hResult == S_OK);

    if (g_bShowDebugSprite)
    {
        D3DVIEWPORT9 oldViewport;
        D3DVIEWPORT9 debugViewport;
        hResult = g_pd3dDevice->GetViewport(&oldViewport); assert(hResult == S_OK);
        debugViewport = oldViewport;
        debugViewport.X = 0;
        debugViewport.Y = 0;
        debugViewport.Width = kRenderWidth / 2;
        debugViewport.Height = kRenderHeight / 2;
        hResult = g_pd3dDevice->SetViewport(&debugViewport); assert(hResult == S_OK);

        hResult = g_pEffect2->SetTechnique("TechniqueDebug"); assert(hResult == S_OK);

        UINT debugNumPass = 0;
        hResult = g_pEffect2->Begin(&debugNumPass, 0); assert(hResult == S_OK);
        hResult = g_pEffect2->BeginPass(0);            assert(hResult == S_OK);

        hResult = g_pEffect2->SetBool("g_bSingleChannelInput", g_bShowNormalInfo ? FALSE : TRUE); assert(hResult == S_OK);
        hResult = g_pEffect2->SetTexture("texture1", g_bShowNormalInfo ? g_pNormalRenderTarget : g_pDepthRenderTarget); assert(hResult == S_OK);
        hResult = g_pEffect2->CommitChanges(); assert(hResult == S_OK);
        DrawFullscreenQuad();

        hResult = g_pEffect2->EndPass(); assert(hResult == S_OK);
        hResult = g_pEffect2->End();     assert(hResult == S_OK);
        hResult = g_pd3dDevice->SetViewport(&oldViewport); assert(hResult == S_OK);
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

LRESULT CALLBACK ToolDialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_COMMAND:
    {
        if (LOWORD(wParam) == kToolDialogButtonId)
        {
            OpenMeshFileDialog();
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogApplySsaoButtonId)
        {
            TCHAR buffer[64] = { };
            GetWindowText(g_hSsaoSampleEdit, buffer, _countof(buffer));
            float samplePixels = g_simpleSsaoSamplePixels;
            if (_stscanf_s(buffer, _T("%f"), &samplePixels) == 1)
            {
                if (samplePixels < 1.0f)
                {
                    samplePixels = 1.0f;
                }
                g_simpleSsaoSamplePixels = samplePixels;

                TCHAR normalizedText[64] = { };
                _stprintf_s(normalizedText, _T("%.1f"), g_simpleSsaoSamplePixels);
                SetWindowText(g_hSsaoSampleEdit, normalizedText);
            }
            return 0;
        }
        break;
    }
    case WM_CLOSE:
    {
        ShowWindow(hWnd, SW_HIDE);
        return 0;
    }
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}
