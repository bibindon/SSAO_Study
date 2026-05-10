#include "app_shared.h"

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

LPDIRECT3DTEXTURE9 g_pRenderTarget = NULL;
LPDIRECT3DTEXTURE9 g_pDepthRenderTarget = NULL;
LPDIRECT3DTEXTURE9 g_pRawDepthRenderTarget = NULL;
LPDIRECT3DTEXTURE9 g_pBackDepthRenderTarget = NULL;
LPDIRECT3DTEXTURE9 g_pNormalRenderTarget = NULL;
LPDIRECT3DTEXTURE9 g_pThicknessRenderTarget = NULL;
LPDIRECT3DTEXTURE9 g_pSsaoRenderTarget = NULL;
LPDIRECT3DTEXTURE9 g_pSsaoBlurRenderTarget = NULL;
LPDIRECT3DTEXTURE9 g_pCenterDepthResolveTexture = NULL;
LPDIRECT3DSURFACE9 g_pCenterDepthReadbackSurface = NULL;

LPDIRECT3DVERTEXDECLARATION9 g_pQuadDecl = NULL;
LPD3DXSPRITE g_pSprite = NULL;
HWND g_hWnd = NULL;
bool g_bShowDebugSprite = false;
bool g_bPrevDepthInfoKeyDown = false;
bool g_bPrevNormalInfoKeyDown = false;
bool g_bPrevThicknessInfoKeyDown = false;
bool g_bPrevBackDepthInfoKeyDown = false;
bool g_bPrevCursorToggleKeyDown = false;
bool g_bPrevLambertToggleKeyDown = false;
bool g_bPrevDialogToggleKeyDown = false;
bool g_bPrevSimpleSsaoToggleKeyDown = false;
bool g_bPrevTextureToggleKeyDown = false;
bool g_bPrevEscapeToggleKeyDown = false;
bool g_bMouseCursorVisible = false;
bool g_bUseLambertLighting = true;
bool g_bUseTexture = true;
bool g_bEnableSimpleSsao = true;
bool g_bEnableSsaoBlur = true;
int g_ssaoBlurKernelSize = 11;
bool g_bUseThicknessForSsao = true;
bool g_bRemoteDesktopCameraMode = false;
bool g_bAllowStraightUpDown = false;
bool g_bDepthScaledSampleDistance = true;
bool g_bAutoScaleSsaoByCenterDepth = true;
bool g_bSmoothAutoSsaoByCenterDepth = false;
bool g_bUseFixedSsaoSampleDistance = true;
bool g_bHasPreviousMousePosition = false;
int g_debugViewMode = kDebugViewNone;
float g_simpleSsaoSampleDistanceMeters = 1.0f;
float g_autoSsaoSampleDistanceMeters = 1.0f;
float g_targetAutoSsaoSampleDistanceMeters = 1.0f;
int g_simpleSsaoSampleCount = 16;
float g_thicknessScale = 1.0f;
float g_ssaoDepthRange = kDefaultSsaoDepthRange;
float g_autoSsaoDepthRange = kDefaultSsaoDepthRange;
float g_targetAutoSsaoDepthRange = kDefaultSsaoDepthRange;
float g_targetNormalBiasScale = 1.0f;
float g_targetDepthBiasScale = 1.0f;
float g_depthCompareDistance = 0.0f;
float g_sampleDepthBiasDistance = 0.1f;
bool g_bEnableThicknessCap = false;
float g_thicknessCapMeters = 1.0f;
float g_shadowStrength = 1.0f;
float g_shadowSaturationStrength = 1.0f;
bool g_bUseShadowSaturation = false;
float g_cameraYaw = -D3DX_PI * 0.25f;
float g_cameraPitch = -0.34f;
D3DXVECTOR3 g_cameraPosition(2.0f, 1.0f, -3.0f);
POINT g_previousMousePosition = { };
HWND g_hToolDialog = NULL;
HWND g_hOpenMeshButton = NULL;
HWND g_hSsaoSampleEdit = NULL;
HWND g_hApplySsaoButton = NULL;
HWND g_hSampleCountEdit = NULL;
HWND g_hApplySampleCountButton = NULL;
HWND g_hUseThicknessCheckbox = NULL;
HWND g_hThicknessScaleEdit = NULL;
HWND g_hApplyThicknessScaleButton = NULL;
HWND g_hSsaoDepthRangeEdit = NULL;
HWND g_hApplySsaoDepthRangeButton = NULL;
HWND g_hRemoteDesktopCheckbox = NULL;
HWND g_hNormalBiasScaleEdit = NULL;
HWND g_hApplyNormalBiasScaleButton = NULL;
HWND g_hDepthBiasScaleEdit = NULL;
HWND g_hApplyDepthBiasScaleButton = NULL;
HWND g_hDepthCompareDistanceEdit = NULL;
HWND g_hApplyDepthCompareDistanceButton = NULL;
HWND g_hAllowStraightUpDownCheckbox = NULL;
HWND g_hDepthBiasDistanceEdit = NULL;
HWND g_hApplyDepthBiasDistanceButton = NULL;
HWND g_hDepthScaledSampleDistanceCheckbox = NULL;
HWND g_hAutoScaleSsaoByCenterCheckbox = NULL;
HWND g_hSmoothAutoSsaoCheckbox = NULL;
HWND g_hEnableSsaoBlurCheckbox = NULL;
HWND g_hSsaoBlur5x5Radio = NULL;
HWND g_hSsaoBlur11x11Radio = NULL;
HWND g_hSsaoBlur21x21Radio = NULL;
HWND g_hFixedSsaoSampleDistanceCheckbox = NULL;
HWND g_hShadowStrengthEdit = NULL;
HWND g_hApplyShadowStrengthButton = NULL;
HWND g_hSaturateShadowCheckbox = NULL;
HWND g_hShadowSaturationStrengthEdit = NULL;
HWND g_hApplyShadowSaturationStrengthButton = NULL;
HWND g_hEnableThicknessCapCheckbox = NULL;
HWND g_hThicknessCapEdit = NULL;
HWND g_hApplyThicknessCapButton = NULL;
HFONT g_hToolDialogFont = NULL;

std::vector<UserMeshInstance> g_userMeshes;
std::vector<UserMeshInstance> g_sceneMeshes;

// Win32 アプリケーションの入口です。
// 初期化後は、入力更新 -> ジオメトリ描画 -> ポストプロセス描画、という順で毎フレーム回します。
int WINAPI _tWinMain(_In_ HINSTANCE hInstance,
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
            // Sleep(16);

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

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// 最小限のウィンドウメッセージ処理です。
// 本サンプルでは入力の大半をポーリングで処理しているため、ここでは終了処理が主な役目です。
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
