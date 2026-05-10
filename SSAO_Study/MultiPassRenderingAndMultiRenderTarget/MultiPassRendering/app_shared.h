#pragma once

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

// COM オブジェクトは参照カウント方式なので、使い終わったら Release が必要です。
// 毎回 if 文を書く代わりに、このマクロで「NULL なら何もしない / そうでなければ解放」をまとめています。
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = NULL; } }

namespace
{
    // ここにある値は、アプリ全体で使う「実験用の基本設定」です。
    // シェーダー初学者の視点では、こうした値が「どの単位系で考えるか」の土台になります。
    constexpr int kRenderWidth = 1600;
    constexpr int kRenderHeight = 900;
    constexpr float kCameraMoveSpeed = 6.0f;
    constexpr float kCameraSlowMoveScale = 0.25f;
    constexpr float kMouseSensitivity = 0.0009f;
    constexpr float kRemoteDesktopMouseSensitivityScale = 4.0f;
    constexpr float kMaxPitch = D3DX_PI * 0.45f;
    constexpr float kExtendedMaxPitch = D3DX_PI * 0.499f;
    constexpr int kCubeGridWidth = 11;
    constexpr int kCubeGridDepth = 11;
    constexpr float kCubeSpacing = 1.2f;
    constexpr float kUserMeshPlacementDistance = 4.5f;
    constexpr int kToolDialogButtonId = 1001;
    constexpr int kToolDialogSsaoSampleEditId = 1002;
    constexpr int kToolDialogApplySsaoButtonId = 1003;
    constexpr int kToolDialogUseThicknessCheckboxId = 1004;
    constexpr int kToolDialogThicknessScaleEditId = 1005;
    constexpr int kToolDialogApplyThicknessScaleButtonId = 1006;
    constexpr int kToolDialogSsaoDepthRangeEditId = 1007;
    constexpr int kToolDialogApplySsaoDepthRangeButtonId = 1008;
    constexpr int kToolDialogRemoteDesktopCheckboxId = 1009;
    constexpr int kToolDialogNormalBiasScaleEditId = 1010;
    constexpr int kToolDialogApplyNormalBiasScaleButtonId = 1011;
    constexpr int kToolDialogDepthBiasScaleEditId = 1012;
    constexpr int kToolDialogApplyDepthBiasScaleButtonId = 1013;
    constexpr int kToolDialogDepthCompareDistanceEditId = 1014;
    constexpr int kToolDialogApplyDepthCompareDistanceButtonId = 1015;
    constexpr int kToolDialogAllowStraightUpDownCheckboxId = 1016;
    constexpr int kToolDialogDepthBiasDistanceEditId = 1017;
    constexpr int kToolDialogApplyDepthBiasDistanceButtonId = 1018;
    constexpr int kToolDialogSampleCountEditId = 1019;
    constexpr int kToolDialogApplySampleCountButtonId = 1020;
    constexpr int kToolDialogDepthScaledSampleDistanceCheckboxId = 1021;
    constexpr int kToolDialogEnableThicknessCapCheckboxId = 1022;
    constexpr int kToolDialogThicknessCapEditId = 1023;
    constexpr int kToolDialogApplyThicknessCapButtonId = 1024;
    constexpr int kToolDialogAutoScaleSsaoByCenterCheckboxId = 1025;
    constexpr int kToolDialogSmoothAutoSsaoCheckboxId = 1026;
    constexpr int kToolDialogEnableSsaoBlurCheckboxId = 1027;
    constexpr int kToolDialogSsaoBlur5x5RadioId = 1028;
    constexpr int kToolDialogSsaoBlur11x11RadioId = 1029;
    constexpr int kToolDialogSsaoBlur21x21RadioId = 1036;
    constexpr int kToolDialogFixedSsaoSampleDistanceCheckboxId = 1030;
    constexpr int kToolDialogShadowStrengthEditId = 1031;
    constexpr int kToolDialogApplyShadowStrengthButtonId = 1032;
    constexpr int kToolDialogSaturateShadowCheckboxId = 1033;
    constexpr int kToolDialogShadowSaturationStrengthEditId = 1034;
    constexpr int kToolDialogApplyShadowSaturationStrengthButtonId = 1035;
    constexpr int kDebugViewNone = 0;
    constexpr int kDebugViewDepth = 1;
    constexpr int kDebugViewNormal = 2;
    constexpr int kDebugViewThickness = 3;
    constexpr int kDebugViewBackDepth = 4;
    constexpr float kCameraNearPlane = 0.1f;
    constexpr float kCameraFarPlane = 50.0f;
    constexpr float kDefaultSsaoDepthRange = 15.0f;
    constexpr float kMinSsaoDepthRange = 0.5f;
    constexpr float kDepthCompareDistance = 0.1f;
    constexpr float kAutoSsaoSmoothingDurationSeconds = 10.0f / 60.0f;
    constexpr float kAutoSsaoMaxDepthRange = 30.0f;
    constexpr float kAutoSsaoMaxSampleDistanceMeters = 3.75f;
}

// ユーザーが読み込んだメッシュも、最初から置いてあるメッシュも、
// 描画時には「メッシュ本体 + マテリアル + テクスチャ + 配置情報」という共通の形で扱えます。
// そのため、描画しやすい単位としてこの構造体にまとめています。
struct UserMeshInstance
{
    LPD3DXMESH mesh = NULL;
    std::vector<D3DMATERIAL9> materials;
    std::vector<LPDIRECT3DTEXTURE9> textures;
    DWORD numMaterials = 0;
    D3DXVECTOR3 position = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    float yaw = 0.0f;
};

// フルスクリーンクアッドは、ポストプロセス用に画面全体を 2 枚の三角形で覆うための頂点です。
// POSITION はすでにクリップ空間(-1..1)で持ち、TEXCOORD はテクスチャ参照先を表します。
struct QuadVertex
{
    float x, y, z, w;
    float u, v;
};

// ここから下は複数の .cpp で共有するグローバル状態です。
// 本サンプルでは「学習しやすさ」と「DirectX9 のサンプルらしい見通し」を優先して
// 明示的な共有状態としてまとめています。

// Direct3D の中核オブジェクト群
extern LPDIRECT3D9 g_pD3D;
extern LPDIRECT3DDEVICE9 g_pd3dDevice;
extern LPD3DXFONT g_pFont;
extern LPD3DXMESH g_pMesh;
extern LPD3DXMESH g_pLargeCubeMesh;
extern LPD3DXMESH g_pPlateMesh;

// メッシュごとのマテリアルとテクスチャ
extern std::vector<D3DMATERIAL9> g_pMaterials;
extern std::vector<LPDIRECT3DTEXTURE9> g_pTextures;
extern DWORD g_dwNumMaterials;
extern std::vector<D3DMATERIAL9> g_pLargeCubeMaterials;
extern std::vector<LPDIRECT3DTEXTURE9> g_pLargeCubeTextures;
extern DWORD g_dwLargeCubeNumMaterials;
extern std::vector<D3DMATERIAL9> g_pPlateMaterials;
extern std::vector<LPDIRECT3DTEXTURE9> g_pPlateTextures;
extern DWORD g_dwPlateNumMaterials;

// 同じテクスチャを何度も読み込まないようにする簡易キャッシュ
extern std::map<std::wstring, LPDIRECT3DTEXTURE9> g_textureCache;

// ジオメトリ描画用エフェクトとポストプロセス用エフェクト
extern LPD3DXEFFECT g_pEffect1;
extern LPD3DXEFFECT g_pEffect2;
extern bool g_bClose;

// MRT と後段ポストプロセスで使う中間テクスチャ群
extern LPDIRECT3DTEXTURE9 g_pRenderTarget;
extern LPDIRECT3DTEXTURE9 g_pDepthRenderTarget;
extern LPDIRECT3DTEXTURE9 g_pRawDepthRenderTarget;
extern LPDIRECT3DTEXTURE9 g_pBackDepthRenderTarget;
extern LPDIRECT3DTEXTURE9 g_pNormalRenderTarget;
extern LPDIRECT3DTEXTURE9 g_pThicknessRenderTarget;
extern LPDIRECT3DTEXTURE9 g_pSsaoRenderTarget;
extern LPDIRECT3DTEXTURE9 g_pSsaoBlurRenderTarget;
extern LPDIRECT3DTEXTURE9 g_pCenterDepthResolveTexture;
extern LPDIRECT3DSURFACE9 g_pCenterDepthReadbackSurface;

// 画面全体描画用の宣言と、デバッグテキスト描画用スプライト
extern LPDIRECT3DVERTEXDECLARATION9 g_pQuadDecl;
extern LPD3DXSPRITE g_pSprite;
extern HWND g_hWnd;

// デバッグ表示やトグル入力の状態
extern bool g_bShowDebugSprite;
extern bool g_bPrevDepthInfoKeyDown;
extern bool g_bPrevNormalInfoKeyDown;
extern bool g_bPrevThicknessInfoKeyDown;
extern bool g_bPrevBackDepthInfoKeyDown;
extern bool g_bPrevCursorToggleKeyDown;
extern bool g_bPrevLambertToggleKeyDown;
extern bool g_bPrevDialogToggleKeyDown;
extern bool g_bPrevSimpleSsaoToggleKeyDown;
extern bool g_bPrevTextureToggleKeyDown;
extern bool g_bPrevEscapeToggleKeyDown;
extern bool g_bMouseCursorVisible;

// 描画・SSAO の各種設定値
extern bool g_bUseLambertLighting;
extern bool g_bUseTexture;
extern bool g_bEnableSimpleSsao;
extern bool g_bEnableSsaoBlur;
extern int g_ssaoBlurKernelSize;
extern bool g_bUseThicknessForSsao;
extern bool g_bRemoteDesktopCameraMode;
extern bool g_bAllowStraightUpDown;
extern bool g_bDepthScaledSampleDistance;
extern bool g_bAutoScaleSsaoByCenterDepth;
extern bool g_bSmoothAutoSsaoByCenterDepth;
extern bool g_bUseFixedSsaoSampleDistance;
extern bool g_bHasPreviousMousePosition;
extern int g_debugViewMode;
extern float g_simpleSsaoSampleDistanceMeters;
extern float g_autoSsaoSampleDistanceMeters;
extern float g_targetAutoSsaoSampleDistanceMeters;
extern int g_simpleSsaoSampleCount;
extern float g_thicknessScale;
extern float g_ssaoDepthRange;
extern float g_autoSsaoDepthRange;
extern float g_targetAutoSsaoDepthRange;
extern float g_targetNormalBiasScale;
extern float g_targetDepthBiasScale;
extern float g_depthCompareDistance;
extern float g_sampleDepthBiasDistance;
extern bool g_bEnableThicknessCap;
extern float g_thicknessCapMeters;
extern float g_shadowStrength;
extern float g_shadowSaturationStrength;
extern bool g_bUseShadowSaturation;

// カメラ状態
extern float g_cameraYaw;
extern float g_cameraPitch;
extern D3DXVECTOR3 g_cameraPosition;
extern POINT g_previousMousePosition;

// 設定ダイアログの各ウィンドウハンドル
extern HWND g_hToolDialog;
extern HWND g_hOpenMeshButton;
extern HWND g_hSsaoSampleEdit;
extern HWND g_hApplySsaoButton;
extern HWND g_hSampleCountEdit;
extern HWND g_hApplySampleCountButton;
extern HWND g_hUseThicknessCheckbox;
extern HWND g_hThicknessScaleEdit;
extern HWND g_hApplyThicknessScaleButton;
extern HWND g_hSsaoDepthRangeEdit;
extern HWND g_hApplySsaoDepthRangeButton;
extern HWND g_hRemoteDesktopCheckbox;
extern HWND g_hNormalBiasScaleEdit;
extern HWND g_hApplyNormalBiasScaleButton;
extern HWND g_hDepthBiasScaleEdit;
extern HWND g_hApplyDepthBiasScaleButton;
extern HWND g_hDepthCompareDistanceEdit;
extern HWND g_hApplyDepthCompareDistanceButton;
extern HWND g_hAllowStraightUpDownCheckbox;
extern HWND g_hDepthBiasDistanceEdit;
extern HWND g_hApplyDepthBiasDistanceButton;
extern HWND g_hDepthScaledSampleDistanceCheckbox;
extern HWND g_hAutoScaleSsaoByCenterCheckbox;
extern HWND g_hSmoothAutoSsaoCheckbox;
extern HWND g_hEnableSsaoBlurCheckbox;
extern HWND g_hSsaoBlur5x5Radio;
extern HWND g_hSsaoBlur11x11Radio;
extern HWND g_hSsaoBlur21x21Radio;
extern HWND g_hFixedSsaoSampleDistanceCheckbox;
extern HWND g_hShadowStrengthEdit;
extern HWND g_hApplyShadowStrengthButton;
extern HWND g_hSaturateShadowCheckbox;
extern HWND g_hShadowSaturationStrengthEdit;
extern HWND g_hApplyShadowSaturationStrengthButton;
extern HWND g_hEnableThicknessCapCheckbox;
extern HWND g_hThicknessCapEdit;
extern HWND g_hApplyThicknessCapButton;
extern HFONT g_hToolDialogFont;

// シーンに最初からあるメッシュと、ユーザーが後から読み込んだメッシュ
extern std::vector<UserMeshInstance> g_userMeshes;
extern std::vector<UserMeshInstance> g_sceneMeshes;

// ここから下は実装ファイルごとに分かれた処理の宣言です。
// 「何を初期化するか」「何を描画するか」「どこで入力を処理するか」を追うと、
// アプリ全体の流れが見えやすくなります。
void TextDraw(LPD3DXFONT pFont, TCHAR* text, int X, int Y);
void InitD3D(HWND hWnd);
void Cleanup();
void LoadMeshWithTextures(const TCHAR* meshPath,
                          LPD3DXMESH* ppMesh,
                          std::vector<D3DMATERIAL9>& materials,
                          std::vector<LPDIRECT3DTEXTURE9>& textures,
                          DWORD* pNumMaterials);
std::wstring GetDirectoryFromPath(const std::wstring& path);
std::wstring ResolveTexturePath(const std::wstring& meshPath, const char* textureFilename);
LPDIRECT3DTEXTURE9 LoadTextureCached(const std::wstring& texturePath);
void ReleaseMeshOnly(LPD3DXMESH* ppMesh,
                     std::vector<D3DMATERIAL9>& materials,
                     std::vector<LPDIRECT3DTEXTURE9>& textures,
                     DWORD* pNumMaterials);
void CreateToolDialog();
void ToggleToolDialog();
void OpenMeshFileDialog();
void PlaceUserMeshAtCurrentLookTarget(UserMeshInstance& userMesh);
void LoadSceneMeshInstance(const TCHAR* meshPath, const D3DXVECTOR3& position, float yaw);
void ResetMouseLookTracking();
void SetMouseCursorVisible(bool visible);
void UpdateInputAndCamera();
void DrawOverlayText();
void DrawSceneGeometry(const D3DXMATRIX& View, const D3DXMATRIX& Proj);
void RenderPass1();
void RenderPass2();
void DrawFullscreenQuad();
void ApplyToolDialogFont(HWND controlHandle);
float GetActiveSsaoSampleDistanceMeters();
float GetActiveSsaoDepthRange();
void UpdateAutoSsaoParametersFromCenterDepth();
LRESULT WINAPI MsgProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK ToolDialogProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
