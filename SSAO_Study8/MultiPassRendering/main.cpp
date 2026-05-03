// main.cpp - SSAO demo with interactive camera and settings window
// MRT3でSSAOを実装

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9d.lib")
#pragma comment(lib, "comctl32.lib")

#include <windows.h>
#include <commctrl.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <tchar.h>
#include <cmath>
#include <string>
#include <vector>

#define SAFE_RELEASE(p) do { if (p) { (p)->Release(); (p)=NULL; } } while(0)

static const int kBackW = (int)(1600 * 1.0);
static const int kBackH = (int)(900 * 1.0);

enum
{
    ID_TRACK_AO_STRENGTH = 1001,
    ID_TRACK_AO_RADIUS,
    ID_TRACK_AO_EDGE_Z,
    ID_TRACK_AO_DEPTH_REJECT,
    ID_TRACK_AO_BIAS,
    ID_TRACK_AO_POWER,
    ID_TRACK_AO_SAMPLES,
    ID_TRACK_BLUR_RADIUS,
    ID_TRACK_POS_RANGE,

    ID_VALUE_AO_STRENGTH = 1101,
    ID_VALUE_AO_RADIUS,
    ID_VALUE_AO_EDGE_Z,
    ID_VALUE_AO_DEPTH_REJECT,
    ID_VALUE_AO_BIAS,
    ID_VALUE_AO_POWER,
    ID_VALUE_AO_SAMPLES,
    ID_VALUE_BLUR_RADIUS,
    ID_VALUE_POS_RANGE,

    ID_CHECK_USE_TEXTURE = 1201,
    ID_CHECK_USE_BLUR,
    ID_CHECK_USE_LAMBERT
};

struct SsaoSettings
{
    float aoStrength;
    float aoRadius;
    float edgeZ;
    float depthReject;
    float aoBias;
    float aoPower;
    int sampleCount;
    int blurRadius;
};

LPDIRECT3D9                     g_pD3D = NULL;
LPDIRECT3DDEVICE9               g_pd3dDevice = NULL;
LPD3DXMESH                      g_pMeshMonkey = NULL;
LPD3DXMESH                      g_pMeshObstacle = NULL;
std::vector<LPDIRECT3DTEXTURE9> g_pTexMonkey;
std::vector<LPDIRECT3DTEXTURE9> g_pTexObstacle;
DWORD                           g_dwNumMonkeyMaterials = 0;
DWORD                           g_dwNumObstacleMaterials = 0;

LPD3DXEFFECT                    g_pEffect1 = NULL; // simple.fx
LPD3DXEFFECT                    g_pEffect2 = NULL; // simple2.fx

LPDIRECT3DTEXTURE9              g_pRenderTarget = NULL;
LPDIRECT3DTEXTURE9              g_pRenderTargetZ = NULL;
LPDIRECT3DTEXTURE9              g_pRenderTargetPos = NULL;
LPDIRECT3DTEXTURE9              g_pRenderTargetNormal = NULL;
LPDIRECT3DTEXTURE9              g_pAoTex = NULL;
LPDIRECT3DTEXTURE9              g_pAoTempBlur = NULL;

LPDIRECT3DVERTEXDECLARATION9    g_pQuadDecl = NULL;

HWND                            g_hMainWnd = NULL;
HWND                            g_hSettingsWnd = NULL;
bool                            g_bClose = false;
bool                            g_bMouseLookInitialized = false;
bool                            g_bCursorHidden = false;
bool                            g_bMouseCaptureEnabled = true;

float                           g_posRange = 24.0f;
bool                            g_bUseTexture = true;
bool                            g_bUseBlur = true;
bool                            g_bUseLambert = true;
SsaoSettings                    g_ssao = { 1.6f, 2.0f, 0.006f, 0.0001f, 0.0015f, 1.4f, 32, 10 };

D3DXMATRIX                      g_mView;
D3DXMATRIX                      g_mProj;

D3DXVECTOR3                     g_cameraPos(6.0f, 3.0f, -6.0f);
float                           g_cameraYaw = -D3DX_PI * 0.25f;
float                           g_cameraPitch = -0.23f;
float                           g_cameraMoveSpeed = 6.0f;
float                           g_cameraBoostMultiplier = 3.0f;
float                           g_mouseSensitivity = 0.0025f;
ULONGLONG                       g_prevFrameTick = 0;

struct QuadVertex
{
    float x, y, z, w;
    float u, v;
};

static void InitD3D(HWND hWnd);
static void Cleanup();
static void RenderPass1();
static void RenderPass2();
static void DrawFullscreenQuad();
static void UpdateFrame();
static void UpdateCamera(float deltaSeconds);
static void SetMouseLookEnabled(bool enabled);
static void CreateSettingsWindow(HINSTANCE hInstance);
static void ToggleSettingsWindow();
static void SyncSettingsControls();
static void ApplyTrackbarSetting(int controlId, int value);
static void UpdateProjectionMatrix();
static std::wstring FormatFloatValue(float value, int decimals);
static std::wstring FormatIntValue(int value);
static void DrawOverlayText(const wchar_t* text, int x, int y, D3DCOLOR color);
static bool IsMainWindowFocused();

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK SettingsWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern int WINAPI _tWinMain(_In_ HINSTANCE hInstance,
                            _In_opt_ HINSTANCE hPrevInstance,
                            _In_ LPWSTR lpCmdLine,
                            _In_ int nShowCmd);

static void SetCheckState(HWND hWnd, int controlId, bool checked)
{
    SendDlgItemMessageW(hWnd, controlId, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

static bool IsSettingsWindowVisible()
{
    return g_hSettingsWnd != NULL && IsWindowVisible(g_hSettingsWnd) != FALSE;
}

static bool IsMainWindowFocused()
{
    return GetForegroundWindow() == g_hMainWnd;
}

static void UpdateValueText(int controlId, const wchar_t* label, const std::wstring& value)
{
    wchar_t buffer[128] = {};
    swprintf_s(buffer, L"%s: %s", label, value.c_str());
    SetDlgItemTextW(g_hSettingsWnd, controlId, buffer);
}

static HWND CreateLabel(HWND hParent, const wchar_t* text, int x, int y, int w, int h, int id)
{
    return CreateWindowW(L"STATIC",
                         text,
                         WS_CHILD | WS_VISIBLE,
                         x,
                         y,
                         w,
                         h,
                         hParent,
                         (HMENU)(INT_PTR)id,
                         GetModuleHandleW(NULL),
                         NULL);
}

static HWND CreateTrack(HWND hParent, int x, int y, int w, int h, int id, int minValue, int maxValue)
{
    HWND hTrack = CreateWindowExW(0,
                                  TRACKBAR_CLASSW,
                                  L"",
                                  WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS | TBS_HORZ,
                                  x,
                                  y,
                                  w,
                                  h,
                                  hParent,
                                  (HMENU)(INT_PTR)id,
                                  GetModuleHandleW(NULL),
                                  NULL);
    SendMessageW(hTrack, TBM_SETRANGE, TRUE, MAKELONG(minValue, maxValue));
    SendMessageW(hTrack, TBM_SETTICFREQ, max(1, (maxValue - minValue) / 8), 0);
    return hTrack;
}

static void CreateLabeledTrack(HWND hParent,
                               const wchar_t* label,
                               int y,
                               int trackId,
                               int valueId,
                               int minValue,
                               int maxValue)
{
    CreateLabel(hParent, label, 16, y, 140, 20, 0);
    CreateTrack(hParent, 16, y + 20, 220, 32, trackId, minValue, maxValue);
    CreateLabel(hParent, L"", 250, y + 20, 140, 20, valueId);
}

static D3DXVECTOR3 GetCameraForward()
{
    const float cosPitch = cosf(g_cameraPitch);
    return D3DXVECTOR3(sinf(g_cameraYaw) * cosPitch,
                       sinf(g_cameraPitch),
                       cosf(g_cameraYaw) * cosPitch);
}

int WINAPI _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR lpCmdLine,
                     _In_ int nShowCmd)
{
    INITCOMMONCONTROLSEX icex = {};
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSEX wc = { sizeof(WNDCLASSEX) };
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = MsgProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = _T("SSAODemo");
    RegisterClassEx(&wc);

    WNDCLASSEX settingsWc = { sizeof(WNDCLASSEX) };
    settingsWc.style = CS_CLASSDC;
    settingsWc.lpfnWndProc = SettingsWndProc;
    settingsWc.hInstance = hInstance;
    settingsWc.hCursor = LoadCursor(NULL, IDC_ARROW);
    settingsWc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    settingsWc.lpszClassName = _T("SSAOSettingsWindow");
    RegisterClassEx(&settingsWc);

    RECT rc = { 0, 0, kBackW, kBackH };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    g_hMainWnd = CreateWindow(_T("SSAODemo"),
                              _T("SSAO Demo"),
                              WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT,
                              CW_USEDEFAULT,
                              rc.right - rc.left,
                              rc.bottom - rc.top,
                              NULL,
                              NULL,
                              hInstance,
                              NULL);

    CreateSettingsWindow(hInstance);
    InitD3D(g_hMainWnd);
    ShowWindow(g_hMainWnd, SW_SHOWDEFAULT);
    UpdateWindow(g_hMainWnd);

    g_prevFrameTick = GetTickCount64();

    MSG msg = {};
    while (true)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (g_hSettingsWnd == NULL || !IsDialogMessage(g_hSettingsWnd, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            UpdateFrame();
            RenderPass1();
            RenderPass2();
            g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
        }

        if (g_bClose)
        {
            break;
        }
    }

    Cleanup();
    UnregisterClass(_T("SSAODemo"), hInstance);
    UnregisterClass(_T("SSAOSettingsWindow"), hInstance);
    return 0;
}

void InitD3D(HWND hWnd)
{
    g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);

    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D16;
    pp.hDeviceWindow = hWnd;
    pp.MultiSampleType = D3DMULTISAMPLE_NONE;

    g_pD3D->CreateDevice(D3DADAPTER_DEFAULT,
                         D3DDEVTYPE_HAL,
                         hWnd,
                         D3DCREATE_HARDWARE_VERTEXPROCESSING,
                         &pp,
                         &g_pd3dDevice);

    {
        LPD3DXBUFFER pMtrlBuf = NULL;
        D3DXLoadMeshFromX(L"monkey.blend.x",
                          D3DXMESH_SYSTEMMEM,
                          g_pd3dDevice,
                          NULL,
                          &pMtrlBuf,
                          NULL,
                          &g_dwNumMonkeyMaterials,
                          &g_pMeshMonkey);

        D3DXMATERIAL* mtrls = (D3DXMATERIAL*)pMtrlBuf->GetBufferPointer();
        g_pTexMonkey.resize(g_dwNumMonkeyMaterials, NULL);

        for (DWORD i = 0; i < g_dwNumMonkeyMaterials; ++i)
        {
            if (mtrls[i].pTextureFilename && mtrls[i].pTextureFilename[0] != '\0')
            {
                LPDIRECT3DTEXTURE9 tex = NULL;
                D3DXCreateTextureFromFileA(g_pd3dDevice, mtrls[i].pTextureFilename, &tex);
                g_pTexMonkey[i] = tex;
            }
        }
        SAFE_RELEASE(pMtrlBuf);
    }

    {
        LPD3DXBUFFER pMtrlBuf = NULL;
        D3DXLoadMeshFromX(L"cube.x",
                          D3DXMESH_SYSTEMMEM,
                          g_pd3dDevice,
                          NULL,
                          &pMtrlBuf,
                          NULL,
                          &g_dwNumObstacleMaterials,
                          &g_pMeshObstacle);

        D3DXMATERIAL* mtrls = (D3DXMATERIAL*)pMtrlBuf->GetBufferPointer();
        g_pTexObstacle.resize(g_dwNumObstacleMaterials, NULL);

        for (DWORD i = 0; i < g_dwNumObstacleMaterials; ++i)
        {
            if (mtrls[i].pTextureFilename && mtrls[i].pTextureFilename[0] != '\0')
            {
                LPDIRECT3DTEXTURE9 tex = NULL;
                D3DXCreateTextureFromFileA(g_pd3dDevice, mtrls[i].pTextureFilename, &tex);
                g_pTexObstacle[i] = tex;
            }
        }
        SAFE_RELEASE(pMtrlBuf);
    }

    D3DXCreateEffectFromFile(g_pd3dDevice,
                             _T("../x64/Debug/simple.cso"),
                             NULL,
                             NULL,
                             0,
                             NULL,
                             &g_pEffect1,
                             NULL);

    D3DXCreateEffectFromFile(g_pd3dDevice,
                             _T("../x64/Debug/simple2.cso"),
                             NULL,
                             NULL,
                             0,
                             NULL,
                             &g_pEffect2,
                             NULL);

    D3DXCreateTexture(g_pd3dDevice,
                      kBackW,
                      kBackH,
                      1,
                      D3DUSAGE_RENDERTARGET,
                      D3DFMT_A8R8G8B8,
                      D3DPOOL_DEFAULT,
                      &g_pRenderTarget);

    D3DXCreateTexture(g_pd3dDevice,
                      kBackW,
                      kBackH,
                      1,
                      D3DUSAGE_RENDERTARGET,
                      D3DFMT_A16B16G16R16F,
                      D3DPOOL_DEFAULT,
                      &g_pRenderTargetZ);

    D3DXCreateTexture(g_pd3dDevice,
                      kBackW,
                      kBackH,
                      1,
                      D3DUSAGE_RENDERTARGET,
                      D3DFMT_A16B16G16R16F,
                      D3DPOOL_DEFAULT,
                      &g_pRenderTargetPos);

    D3DXCreateTexture(g_pd3dDevice,
                      kBackW,
                      kBackH,
                      1,
                      D3DUSAGE_RENDERTARGET,
                      D3DFMT_A16B16G16R16F,
                      D3DPOOL_DEFAULT,
                      &g_pRenderTargetNormal);

    D3DXCreateTexture(g_pd3dDevice,
                      kBackW,
                      kBackH,
                      1,
                      D3DUSAGE_RENDERTARGET,
                      D3DFMT_A8R8G8B8,
                      D3DPOOL_DEFAULT,
                      &g_pAoTex);

    D3DXCreateTexture(g_pd3dDevice,
                      kBackW,
                      kBackH,
                      1,
                      D3DUSAGE_RENDERTARGET,
                      D3DFMT_A8R8G8B8,
                      D3DPOOL_DEFAULT,
                      &g_pAoTempBlur);

    D3DVERTEXELEMENT9 elems[] =
    {
        {0, 0,  D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()
    };

    g_pd3dDevice->CreateVertexDeclaration(elems, &g_pQuadDecl);
    UpdateProjectionMatrix();
    SyncSettingsControls();
}

void Cleanup()
{
    SetMouseLookEnabled(false);

    for (size_t i = 0; i < g_pTexMonkey.size(); ++i)
    {
        SAFE_RELEASE(g_pTexMonkey[i]);
    }

    for (size_t i = 0; i < g_pTexObstacle.size(); ++i)
    {
        SAFE_RELEASE(g_pTexObstacle[i]);
    }

    SAFE_RELEASE(g_pMeshMonkey);
    SAFE_RELEASE(g_pMeshObstacle);
    SAFE_RELEASE(g_pEffect1);
    SAFE_RELEASE(g_pEffect2);
    SAFE_RELEASE(g_pRenderTarget);
    SAFE_RELEASE(g_pRenderTargetZ);
    SAFE_RELEASE(g_pRenderTargetPos);
    SAFE_RELEASE(g_pRenderTargetNormal);
    SAFE_RELEASE(g_pAoTex);
    SAFE_RELEASE(g_pAoTempBlur);
    SAFE_RELEASE(g_pQuadDecl);
    SAFE_RELEASE(g_pd3dDevice);
    SAFE_RELEASE(g_pD3D);
}

static void DrawOverlayText(const wchar_t* text, int x, int y, D3DCOLOR color)
{
    RECT rc = { x, y, x + 700, y + 40 };
    g_pd3dDevice->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
    D3DXFONT_DESCW fontDesc = {};
    fontDesc.Height = 22;
    fontDesc.Weight = FW_BOLD;
    wcscpy_s(fontDesc.FaceName, L"Meiryo");

    ID3DXFont* pFont = NULL;
    if (SUCCEEDED(D3DXCreateFontIndirectW(g_pd3dDevice, &fontDesc, &pFont)) && pFont)
    {
        pFont->DrawTextW(NULL, text, -1, &rc, DT_LEFT | DT_TOP | DT_NOCLIP, color);
    }
    SAFE_RELEASE(pFont);
}

static void UpdateProjectionMatrix()
{
    D3DXMatrixPerspectiveFovLH(&g_mProj,
                               D3DXToRadian(45.0f),
                               (float)kBackW / (float)kBackH,
                               1.0f,
                               1000.0f);
}

static void UpdateFrame()
{
    const ULONGLONG now = GetTickCount64();
    float deltaSeconds = 0.016f;

    if (g_prevFrameTick != 0)
    {
        deltaSeconds = (float)(now - g_prevFrameTick) / 1000.0f;
        if (deltaSeconds < 0.0f)
        {
            deltaSeconds = 0.0f;
        }
        if (deltaSeconds > 0.05f)
        {
            deltaSeconds = 0.05f;
        }
    }

    g_prevFrameTick = now;
    UpdateCamera(deltaSeconds);
}

static void UpdateCamera(float deltaSeconds)
{
    if (!IsMainWindowFocused())
    {
        SetMouseLookEnabled(false);
        return;
    }

    const bool canUseMouseLook = g_bMouseCaptureEnabled;
    SetMouseLookEnabled(canUseMouseLook);

    if (canUseMouseLook)
    {
        RECT clientRect = {};
        GetClientRect(g_hMainWnd, &clientRect);

        POINT center =
        {
            (clientRect.left + clientRect.right) / 2,
            (clientRect.top + clientRect.bottom) / 2
        };
        ClientToScreen(g_hMainWnd, &center);

        POINT cursorPos = {};
        GetCursorPos(&cursorPos);

        if (!g_bMouseLookInitialized)
        {
            SetCursorPos(center.x, center.y);
            g_bMouseLookInitialized = true;
        }
        else
        {
            LONG deltaX = cursorPos.x - center.x;
            LONG deltaY = cursorPos.y - center.y;

            g_cameraYaw += (float)deltaX * g_mouseSensitivity;
            g_cameraPitch -= (float)deltaY * g_mouseSensitivity;

            const float pitchLimit = D3DXToRadian(89.0f);
            if (g_cameraPitch > pitchLimit)
            {
                g_cameraPitch = pitchLimit;
            }
            if (g_cameraPitch < -pitchLimit)
            {
                g_cameraPitch = -pitchLimit;
            }

            SetCursorPos(center.x, center.y);
        }
    }

    D3DXVECTOR3 forward = GetCameraForward();
    D3DXVec3Normalize(&forward, &forward);

    D3DXVECTOR3 up(0.0f, 1.0f, 0.0f);
    D3DXVECTOR3 right;
    D3DXVec3Cross(&right, &up, &forward);
    D3DXVec3Normalize(&right, &right);

    D3DXVECTOR3 move(0.0f, 0.0f, 0.0f);

    if (GetAsyncKeyState('W') & 0x8000) { move += forward; }
    if (GetAsyncKeyState('S') & 0x8000) { move -= forward; }
    if (GetAsyncKeyState('D') & 0x8000) { move += right; }
    if (GetAsyncKeyState('A') & 0x8000) { move -= right; }
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) { move.y += 1.0f; }
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) { move.y -= 1.0f; }

    if (D3DXVec3LengthSq(&move) > 0.0f)
    {
        D3DXVec3Normalize(&move, &move);
        float speed = g_cameraMoveSpeed;
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000)
        {
            speed *= g_cameraBoostMultiplier;
        }
        g_cameraPos += move * (speed * deltaSeconds);
    }
}

void RenderPass1()
{
    LPDIRECT3DSURFACE9 pOldRT0 = NULL;
    g_pd3dDevice->GetRenderTarget(0, &pOldRT0);

    LPDIRECT3DSURFACE9 pRT0 = NULL;
    LPDIRECT3DSURFACE9 pRT1 = NULL;
    LPDIRECT3DSURFACE9 pRT2 = NULL;
    LPDIRECT3DSURFACE9 pRT3 = NULL;

    g_pRenderTarget->GetSurfaceLevel(0, &pRT0);
    g_pRenderTargetZ->GetSurfaceLevel(0, &pRT1);
    g_pRenderTargetPos->GetSurfaceLevel(0, &pRT2);
    g_pRenderTargetNormal->GetSurfaceLevel(0, &pRT3);

    g_pd3dDevice->SetRenderTarget(0, pRT0);
    g_pd3dDevice->SetRenderTarget(1, pRT1);
    g_pd3dDevice->SetRenderTarget(2, pRT2);
    g_pd3dDevice->SetRenderTarget(3, pRT3);

    D3DXMATRIX matWorld;
    D3DXMATRIX matView;
    D3DXMATRIX matWorldViewProj;

    D3DXVECTOR3 eye = g_cameraPos;
    D3DXVECTOR3 at = g_cameraPos + GetCameraForward();
    D3DXVECTOR3 up(0.0f, 1.0f, 0.0f);
    D3DXMatrixLookAtLH(&matView, &eye, &at, &up);

    g_pd3dDevice->Clear(0,
                        NULL,
                        D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                        D3DCOLOR_XRGB(100, 100, 100),
                        1.0f,
                        0);

    g_pd3dDevice->BeginScene();

    D3DXMatrixIdentity(&matWorld);
    matWorldViewProj = matWorld * matView * g_mProj;

    g_pEffect1->SetMatrix("g_matWorld", &matWorld);
    g_pEffect1->SetMatrix("g_matView", &matView);
    g_pEffect1->SetMatrix("g_matWorldViewProj", &matWorldViewProj);
    g_pEffect1->SetFloat("g_fNear", 1.0f);
    g_pEffect1->SetFloat("g_fFar", 1000.0f);
    g_pEffect1->SetFloat("g_posRange", g_posRange);
    g_pEffect1->SetBool("g_bUseLambert", g_bUseLambert ? TRUE : FALSE);

    g_pEffect1->SetTechnique("TechniqueMRT");
    UINT numPass = 0;
    g_pEffect1->Begin(&numPass, 0);
    g_pEffect1->BeginPass(0);

    for (DWORD matIndex = 0; matIndex < g_dwNumMonkeyMaterials; ++matIndex)
    {
        if (g_pTexMonkey[matIndex] && g_bUseTexture)
        {
            g_pEffect1->SetBool("g_bUseTexture", TRUE);
            g_pEffect1->SetTexture("g_texBase", g_pTexMonkey[matIndex]);
        }
        else
        {
            g_pEffect1->SetBool("g_bUseTexture", FALSE);
            g_pEffect1->SetTexture("g_texBase", NULL);
        }

        g_pEffect1->CommitChanges();
        g_pMeshMonkey->DrawSubset(matIndex);
    }

    static float t2 = 0.0f;
    t2 += 0.01f;

    D3DXMatrixTranslation(&matWorld,
                          0.0f,
                          sinf(t2) * 1.0f + 1.0f,
                          0.0f);
    matWorldViewProj = matWorld * matView * g_mProj;

    g_pEffect1->SetMatrix("g_matWorld", &matWorld);
    g_pEffect1->SetMatrix("g_matWorldViewProj", &matWorldViewProj);
    g_pEffect1->CommitChanges();

    for (DWORD matIndex2 = 0; matIndex2 < g_dwNumObstacleMaterials; ++matIndex2)
    {
        if (g_pTexObstacle[matIndex2] && g_bUseTexture)
        {
            g_pEffect1->SetBool("g_bUseTexture", TRUE);
            g_pEffect1->SetTexture("g_texBase", g_pTexObstacle[matIndex2]);
        }
        else
        {
            g_pEffect1->SetBool("g_bUseTexture", FALSE);
            g_pEffect1->SetTexture("g_texBase", NULL);
        }

        g_pEffect1->CommitChanges();
        g_pMeshObstacle->DrawSubset(matIndex2);
    }

    g_pEffect1->EndPass();
    g_pEffect1->End();
    g_pd3dDevice->EndScene();

    g_pd3dDevice->SetRenderTarget(3, NULL);
    g_pd3dDevice->SetRenderTarget(2, NULL);
    g_pd3dDevice->SetRenderTarget(1, NULL);
    g_pd3dDevice->SetRenderTarget(0, pOldRT0);

    SAFE_RELEASE(pRT0);
    SAFE_RELEASE(pRT1);
    SAFE_RELEASE(pRT2);
    SAFE_RELEASE(pRT3);
    SAFE_RELEASE(pOldRT0);

    g_mView = matView;
}

void RenderPass2()
{
    g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    D3DXVECTOR2 invSize(1.0f / kBackW, 1.0f / kBackH);
    g_pEffect2->SetFloatArray("g_invSize", (FLOAT*)&invSize, 2);
    UINT n = 0;

    {
        g_pEffect2->SetTechnique("TechniqueAO_Create");
        g_pEffect2->SetMatrix("g_matView", &g_mView);
        g_pEffect2->SetMatrix("g_matProj", &g_mProj);
        g_pEffect2->SetFloat("g_fNear", 1.0f);
        g_pEffect2->SetFloat("g_fFar", 1000.0f);
        g_pEffect2->SetFloat("g_posRange", g_posRange);
        g_pEffect2->SetTexture("texZ", g_pRenderTargetZ);
        g_pEffect2->SetTexture("texPos", g_pRenderTargetPos);
        g_pEffect2->SetTexture("texNormal", g_pRenderTargetNormal);

        g_pEffect2->SetFloat("g_aoStrength", g_ssao.aoStrength);
        g_pEffect2->SetFloat("g_aoStepWorld", g_ssao.aoRadius);
        g_pEffect2->SetFloat("g_edgeZ", g_ssao.edgeZ);
        g_pEffect2->SetFloat("g_aoBias", g_ssao.aoBias);
        g_pEffect2->SetFloat("g_aoPower", g_ssao.aoPower);
        g_pEffect2->SetInt("g_sampleCount", g_ssao.sampleCount);

        LPDIRECT3DSURFACE9 pAo = NULL;
        g_pAoTex->GetSurfaceLevel(0, &pAo);
        g_pd3dDevice->SetRenderTarget(0, pAo);

        g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
        g_pd3dDevice->BeginScene();
        g_pEffect2->Begin(&n, 0);
        g_pEffect2->BeginPass(0);
        DrawFullscreenQuad();
        g_pEffect2->EndPass();
        g_pEffect2->End();
        g_pd3dDevice->EndScene();
        SAFE_RELEASE(pAo);
    }

    if (g_bUseBlur)
    {
        g_pEffect2->SetFloat("g_depthReject", g_ssao.depthReject);
        g_pEffect2->SetInt("g_blurRadius", g_ssao.blurRadius);

        {
            g_pEffect2->SetTechnique("TechniqueAO_BlurH");
            g_pEffect2->SetTexture("texAO", g_pAoTex);

            LPDIRECT3DSURFACE9 pTemp = NULL;
            g_pAoTempBlur->GetSurfaceLevel(0, &pTemp);
            g_pd3dDevice->SetRenderTarget(0, pTemp);

            g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
            g_pd3dDevice->BeginScene();
            g_pEffect2->Begin(&n, 0);
            g_pEffect2->BeginPass(0);
            DrawFullscreenQuad();
            g_pEffect2->EndPass();
            g_pEffect2->End();
            g_pd3dDevice->EndScene();

            SAFE_RELEASE(pTemp);
        }

        {
            g_pEffect2->SetTechnique("TechniqueAO_BlurV");
            g_pEffect2->SetTexture("texAO", g_pAoTempBlur);

            LPDIRECT3DSURFACE9 pAo2 = NULL;
            g_pAoTex->GetSurfaceLevel(0, &pAo2);
            g_pd3dDevice->SetRenderTarget(0, pAo2);

            g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
            g_pd3dDevice->BeginScene();
            g_pEffect2->Begin(&n, 0);
            g_pEffect2->BeginPass(0);
            DrawFullscreenQuad();
            g_pEffect2->EndPass();
            g_pEffect2->End();
            g_pd3dDevice->EndScene();

            SAFE_RELEASE(pAo2);
        }
    }

    {
        g_pEffect2->SetTechnique("TechniqueAO_Composite");
        g_pEffect2->SetTexture("texColor", g_pRenderTarget);
        g_pEffect2->SetTexture("texAO", g_pAoTex);
        g_pEffect2->SetFloatArray("g_invSize", (FLOAT*)&invSize, 2);

        LPDIRECT3DSURFACE9 pBack = NULL;
        g_pd3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBack);
        g_pd3dDevice->SetRenderTarget(0, pBack);

        g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, 0, 1.0f, 0);
        g_pd3dDevice->BeginScene();
        g_pEffect2->Begin(&n, 0);
        g_pEffect2->BeginPass(0);
        DrawFullscreenQuad();
        g_pEffect2->EndPass();
        g_pEffect2->End();
        g_pd3dDevice->EndScene();

        DrawOverlayText(L"Press 1 to open SSAO settings / Esc to toggle mouse look", 16, 16, D3DCOLOR_ARGB(255, 255, 255, 255));

        SAFE_RELEASE(pBack);
    }

    g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
}

void DrawFullscreenQuad()
{
    QuadVertex v[4] = {};

    float du = 0.5f / (float)kBackW;
    float dv = 0.5f / (float)kBackH;

    v[0] = { -1, -1, 0, 1, 0 + du, 1 - dv };
    v[1] = { -1,  1, 0, 1, 0 + du, 0 + dv };
    v[2] = {  1, -1, 0, 1, 1 - du, 1 - dv };
    v[3] = {  1,  1, 0, 1, 1 - du, 0 + dv };

    g_pd3dDevice->SetVertexDeclaration(g_pQuadDecl);
    g_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(QuadVertex));
}

static void SetMouseLookEnabled(bool enabled)
{
    if (enabled)
    {
        if (!g_bCursorHidden)
        {
            while (ShowCursor(FALSE) >= 0) {}
            g_bCursorHidden = true;
        }
    }
    else
    {
        g_bMouseLookInitialized = false;
        if (g_bCursorHidden)
        {
            while (ShowCursor(TRUE) < 0) {}
            g_bCursorHidden = false;
        }
    }
}

static void CreateSettingsWindow(HINSTANCE hInstance)
{
    g_hSettingsWnd = CreateWindowExW(WS_EX_TOOLWINDOW,
                                     L"SSAOSettingsWindow",
                                     L"SSAO Settings",
                                     WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                                     CW_USEDEFAULT,
                                     CW_USEDEFAULT,
                                     520,
                                     700,
                                     g_hMainWnd,
                                     NULL,
                                     hInstance,
                                     NULL);

    CreateLabel(g_hSettingsWnd, L"1キーで開閉 / メイン画面にフォーカス中は移動可能", 16, 12, 460, 20, 0);
    CreateLabeledTrack(g_hSettingsWnd, L"AO Strength", 40, ID_TRACK_AO_STRENGTH, ID_VALUE_AO_STRENGTH, 0, 400);
    CreateLabeledTrack(g_hSettingsWnd, L"AO Radius", 90, ID_TRACK_AO_RADIUS, ID_VALUE_AO_RADIUS, 10, 800);
    CreateLabeledTrack(g_hSettingsWnd, L"Edge Reject", 140, ID_TRACK_AO_EDGE_Z, ID_VALUE_AO_EDGE_Z, 1, 500);
    CreateLabeledTrack(g_hSettingsWnd, L"Blur Reject", 190, ID_TRACK_AO_DEPTH_REJECT, ID_VALUE_AO_DEPTH_REJECT, 1, 500);
    CreateLabeledTrack(g_hSettingsWnd, L"AO Bias", 240, ID_TRACK_AO_BIAS, ID_VALUE_AO_BIAS, 1, 1000);
    CreateLabeledTrack(g_hSettingsWnd, L"AO Power", 290, ID_TRACK_AO_POWER, ID_VALUE_AO_POWER, 50, 400);
    CreateLabeledTrack(g_hSettingsWnd, L"Sample Count", 340, ID_TRACK_AO_SAMPLES, ID_VALUE_AO_SAMPLES, 4, 64);
    CreateLabeledTrack(g_hSettingsWnd, L"Blur Radius", 390, ID_TRACK_BLUR_RADIUS, ID_VALUE_BLUR_RADIUS, 0, 16);
    CreateLabeledTrack(g_hSettingsWnd, L"Pos Range", 440, ID_TRACK_POS_RANGE, ID_VALUE_POS_RANGE, 4, 64);

    CreateLabel(g_hSettingsWnd, L"Render Options", 16, 520, 180, 24, 0);

    CreateWindowW(L"BUTTON",
                  L"Base Texture",
                  WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                  16,
                  552,
                  130,
                  20,
                  g_hSettingsWnd,
                  (HMENU)(INT_PTR)ID_CHECK_USE_TEXTURE,
                  hInstance,
                  NULL);

    CreateWindowW(L"BUTTON",
                  L"AO Blur",
                  WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                  170,
                  552,
                  110,
                  20,
                  g_hSettingsWnd,
                  (HMENU)(INT_PTR)ID_CHECK_USE_BLUR,
                  hInstance,
                  NULL);

    CreateWindowW(L"BUTTON",
                  L"Lambert",
                  WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                  310,
                  552,
                  110,
                  20,
                  g_hSettingsWnd,
                  (HMENU)(INT_PTR)ID_CHECK_USE_LAMBERT,
                  hInstance,
                  NULL);

    ShowWindow(g_hSettingsWnd, SW_HIDE);
}

static void ToggleSettingsWindow()
{
    if (g_hSettingsWnd == NULL)
    {
        return;
    }

    if (IsSettingsWindowVisible())
    {
        ShowWindow(g_hSettingsWnd, SW_HIDE);
        SetForegroundWindow(g_hMainWnd);
    }
    else
    {
        SyncSettingsControls();
        ShowWindow(g_hSettingsWnd, SW_SHOW);
        SetForegroundWindow(g_hSettingsWnd);
    }
}

static std::wstring FormatFloatValue(float value, int decimals)
{
    wchar_t buffer[64] = {};
    swprintf_s(buffer, L"%.*f", decimals, value);
    return buffer;
}

static std::wstring FormatIntValue(int value)
{
    wchar_t buffer[64] = {};
    swprintf_s(buffer, L"%d", value);
    return buffer;
}

static void SyncSettingsControls()
{
    if (g_hSettingsWnd == NULL)
    {
        return;
    }

    SendDlgItemMessageW(g_hSettingsWnd, ID_TRACK_AO_STRENGTH, TBM_SETPOS, TRUE, (LPARAM)(int)(g_ssao.aoStrength * 100.0f));
    SendDlgItemMessageW(g_hSettingsWnd, ID_TRACK_AO_RADIUS, TBM_SETPOS, TRUE, (LPARAM)(int)(g_ssao.aoRadius * 100.0f));
    SendDlgItemMessageW(g_hSettingsWnd, ID_TRACK_AO_EDGE_Z, TBM_SETPOS, TRUE, (LPARAM)(int)(g_ssao.edgeZ * 10000.0f));
    SendDlgItemMessageW(g_hSettingsWnd, ID_TRACK_AO_DEPTH_REJECT, TBM_SETPOS, TRUE, (LPARAM)(int)(g_ssao.depthReject * 100000.0f));
    SendDlgItemMessageW(g_hSettingsWnd, ID_TRACK_AO_BIAS, TBM_SETPOS, TRUE, (LPARAM)(int)(g_ssao.aoBias * 100000.0f));
    SendDlgItemMessageW(g_hSettingsWnd, ID_TRACK_AO_POWER, TBM_SETPOS, TRUE, (LPARAM)(int)(g_ssao.aoPower * 100.0f));
    SendDlgItemMessageW(g_hSettingsWnd, ID_TRACK_AO_SAMPLES, TBM_SETPOS, TRUE, (LPARAM)g_ssao.sampleCount);
    SendDlgItemMessageW(g_hSettingsWnd, ID_TRACK_BLUR_RADIUS, TBM_SETPOS, TRUE, (LPARAM)g_ssao.blurRadius);
    SendDlgItemMessageW(g_hSettingsWnd, ID_TRACK_POS_RANGE, TBM_SETPOS, TRUE, (LPARAM)(int)g_posRange);

    UpdateValueText(ID_VALUE_AO_STRENGTH, L"Value", FormatFloatValue(g_ssao.aoStrength, 2));
    UpdateValueText(ID_VALUE_AO_RADIUS, L"Value", FormatFloatValue(g_ssao.aoRadius, 2));
    UpdateValueText(ID_VALUE_AO_EDGE_Z, L"Value", FormatFloatValue(g_ssao.edgeZ, 4));
    UpdateValueText(ID_VALUE_AO_DEPTH_REJECT, L"Value", FormatFloatValue(g_ssao.depthReject, 5));
    UpdateValueText(ID_VALUE_AO_BIAS, L"Value", FormatFloatValue(g_ssao.aoBias, 5));
    UpdateValueText(ID_VALUE_AO_POWER, L"Value", FormatFloatValue(g_ssao.aoPower, 2));
    UpdateValueText(ID_VALUE_AO_SAMPLES, L"Value", FormatIntValue(g_ssao.sampleCount));
    UpdateValueText(ID_VALUE_BLUR_RADIUS, L"Value", FormatIntValue(g_ssao.blurRadius));
    UpdateValueText(ID_VALUE_POS_RANGE, L"Value", FormatFloatValue(g_posRange, 0));

    SetCheckState(g_hSettingsWnd, ID_CHECK_USE_TEXTURE, g_bUseTexture);
    SetCheckState(g_hSettingsWnd, ID_CHECK_USE_BLUR, g_bUseBlur);
    SetCheckState(g_hSettingsWnd, ID_CHECK_USE_LAMBERT, g_bUseLambert);
}

static void ApplyTrackbarSetting(int controlId, int value)
{
    switch (controlId)
    {
    case ID_TRACK_AO_STRENGTH:
        g_ssao.aoStrength = (float)value / 100.0f;
        break;
    case ID_TRACK_AO_RADIUS:
        g_ssao.aoRadius = (float)value / 100.0f;
        break;
    case ID_TRACK_AO_EDGE_Z:
        g_ssao.edgeZ = (float)value / 10000.0f;
        break;
    case ID_TRACK_AO_DEPTH_REJECT:
        g_ssao.depthReject = (float)value / 100000.0f;
        break;
    case ID_TRACK_AO_BIAS:
        g_ssao.aoBias = (float)value / 100000.0f;
        break;
    case ID_TRACK_AO_POWER:
        g_ssao.aoPower = (float)value / 100.0f;
        break;
    case ID_TRACK_AO_SAMPLES:
        g_ssao.sampleCount = value;
        break;
    case ID_TRACK_BLUR_RADIUS:
        g_ssao.blurRadius = value;
        break;
    case ID_TRACK_POS_RANGE:
        g_posRange = (float)value;
        break;
    default:
        break;
    }

    SyncSettingsControls();
}

LRESULT CALLBACK SettingsWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_HSCROLL:
        if ((HWND)lParam != NULL)
        {
            int controlId = GetDlgCtrlID((HWND)lParam);
            int pos = (int)SendMessageW((HWND)lParam, TBM_GETPOS, 0, 0);
            ApplyTrackbarSetting(controlId, pos);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case ID_CHECK_USE_TEXTURE:
            g_bUseTexture = (SendDlgItemMessageW(hWnd, ID_CHECK_USE_TEXTURE, BM_GETCHECK, 0, 0) == BST_CHECKED);
            return 0;
        case ID_CHECK_USE_BLUR:
            g_bUseBlur = (SendDlgItemMessageW(hWnd, ID_CHECK_USE_BLUR, BM_GETCHECK, 0, 0) == BST_CHECKED);
            return 0;
        case ID_CHECK_USE_LAMBERT:
            g_bUseLambert = (SendDlgItemMessageW(hWnd, ID_CHECK_USE_LAMBERT, BM_GETCHECK, 0, 0) == BST_CHECKED);
            return 0;
        default:
            break;
        }
        break;

    case WM_KEYDOWN:
        if (wParam == '1')
        {
            ToggleSettingsWindow();
            return 0;
        }
        break;

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        SetForegroundWindow(g_hMainWnd);
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_KEYDOWN:
        if (wParam == '1')
        {
            ToggleSettingsWindow();
            return 0;
        }
        if (wParam == VK_ESCAPE)
        {
            g_bMouseCaptureEnabled = !g_bMouseCaptureEnabled;
            SetMouseLookEnabled(g_bMouseCaptureEnabled);
            return 0;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        g_bClose = true;
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}
