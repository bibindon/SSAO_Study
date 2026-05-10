#include "app_shared.h"
// 読み込んだメッシュを現在視線の少し先に置く。
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

// 初期シーンへ配置するメッシュを登録します。
void LoadSceneMeshInstance(const TCHAR* meshPath, const D3DXVECTOR3& position, float yaw)
{
    UserMeshInstance sceneMesh;
    LoadMeshWithTextures(meshPath, &sceneMesh.mesh, sceneMesh.materials, sceneMesh.textures, &sceneMesh.numMaterials);
    sceneMesh.position = position;
    sceneMesh.yaw = yaw;
    g_sceneMeshes.push_back(sceneMesh);
}

// マウス追跡の基準点をリセットする。
void ResetMouseLookTracking()
{
    g_bHasPreviousMousePosition = false;
    g_previousMousePosition.x = 0;
    g_previousMousePosition.y = 0;
}

// カーソル表示状態とマウスルック状態を同期する。
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
    ResetMouseLookTracking();
}

// 入力からカメラやデバッグ表示の状態を更新する。
void UpdateInputAndCamera()
{
    const float deltaTime = 1.0f / 60.0f;
    float maxPitch = kMaxPitch;
    if (g_bAllowStraightUpDown)
    {
        // 角度制限を少し緩める。
        maxPitch = kExtendedMaxPitch;
    }
    const HWND foregroundWindow = GetForegroundWindow();
    const bool isMainWindowActive = (foregroundWindow == g_hWnd);
    const bool isToolDialogActive = (g_hToolDialog != NULL && foregroundWindow == g_hToolDialog);
    const bool depthInfoKeyDown = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
    const bool normalInfoKeyDown = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
    const bool thicknessInfoKeyDown = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;
    const bool backDepthInfoKeyDown = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
    const bool cursorToggleKeyDown = (GetAsyncKeyState('2') & 0x8000) != 0;
    const bool lambertToggleKeyDown = (GetAsyncKeyState('3') & 0x8000) != 0;
    const bool dialogToggleKeyDown = (GetAsyncKeyState('4') & 0x8000) != 0;
    const bool simpleSsaoToggleKeyDown = (GetAsyncKeyState('5') & 0x8000) != 0;
    const bool textureToggleKeyDown = (GetAsyncKeyState('6') & 0x8000) != 0;
    const bool escapeToggleKeyDown = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;

    // ダイアログ編集中はカメラ更新を止める。
    if (isToolDialogActive)
    {
        g_bPrevDepthInfoKeyDown = depthInfoKeyDown;
        g_bPrevNormalInfoKeyDown = normalInfoKeyDown;
        g_bPrevThicknessInfoKeyDown = thicknessInfoKeyDown;
        g_bPrevBackDepthInfoKeyDown = backDepthInfoKeyDown;
        g_bPrevCursorToggleKeyDown = cursorToggleKeyDown;
        g_bPrevLambertToggleKeyDown = lambertToggleKeyDown;
        g_bPrevDialogToggleKeyDown = dialogToggleKeyDown;
        g_bPrevSimpleSsaoToggleKeyDown = simpleSsaoToggleKeyDown;
        g_bPrevTextureToggleKeyDown = textureToggleKeyDown;
        g_bPrevEscapeToggleKeyDown = escapeToggleKeyDown;
        ResetMouseLookTracking();
        return;
    }

    if (depthInfoKeyDown && !g_bPrevDepthInfoKeyDown)
    {
        if (g_bShowDebugSprite && g_debugViewMode == kDebugViewDepth)
        {
            g_bShowDebugSprite = false;
            g_debugViewMode = kDebugViewNone;
        }
        else
        {
            g_bShowDebugSprite = true;
            g_debugViewMode = kDebugViewDepth;
        }
    }
    g_bPrevDepthInfoKeyDown = depthInfoKeyDown;

    if (normalInfoKeyDown && !g_bPrevNormalInfoKeyDown)
    {
        if (g_bShowDebugSprite && g_debugViewMode == kDebugViewNormal)
        {
            g_bShowDebugSprite = false;
            g_debugViewMode = kDebugViewNone;
        }
        else
        {
            g_bShowDebugSprite = true;
            g_debugViewMode = kDebugViewNormal;
        }
    }
    g_bPrevNormalInfoKeyDown = normalInfoKeyDown;

    if (thicknessInfoKeyDown && !g_bPrevThicknessInfoKeyDown)
    {
        if (g_bShowDebugSprite && g_debugViewMode == kDebugViewThickness)
        {
            g_bShowDebugSprite = false;
            g_debugViewMode = kDebugViewNone;
        }
        else
        {
            g_bShowDebugSprite = true;
            g_debugViewMode = kDebugViewThickness;
        }
    }
    g_bPrevThicknessInfoKeyDown = thicknessInfoKeyDown;

    if (backDepthInfoKeyDown && !g_bPrevBackDepthInfoKeyDown)
    {
        if (g_bShowDebugSprite && g_debugViewMode == kDebugViewBackDepth)
        {
            g_bShowDebugSprite = false;
            g_debugViewMode = kDebugViewNone;
        }
        else
        {
            g_bShowDebugSprite = true;
            g_debugViewMode = kDebugViewBackDepth;
        }
    }
    g_bPrevBackDepthInfoKeyDown = backDepthInfoKeyDown;

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

    if (textureToggleKeyDown && !g_bPrevTextureToggleKeyDown)
    {
        g_bUseTexture = !g_bUseTexture;
    }
    g_bPrevTextureToggleKeyDown = textureToggleKeyDown;

    // メインウィンドウが非アクティブな間は視点更新を止める。
    if (!isMainWindowActive)
    {
        ResetMouseLookTracking();
        return;
    }

    // カーソル非表示時だけマウスルックを有効にする。
    if (!g_bMouseCursorVisible)
    {
        POINT mousePos;
        if (GetCursorPos(&mousePos))
        {
            if (g_bRemoteDesktopCameraMode)
            {
                // リモートデスクトップ向けの相対移動モード。
                if (g_bHasPreviousMousePosition)
                {
                    const LONG deltaX = mousePos.x - g_previousMousePosition.x;
                    const LONG deltaY = mousePos.y - g_previousMousePosition.y;

                    g_cameraYaw += static_cast<float>(deltaX) * kMouseSensitivity * kRemoteDesktopMouseSensitivityScale;
                    g_cameraPitch -= static_cast<float>(deltaY) * kMouseSensitivity * kRemoteDesktopMouseSensitivityScale;
                    if (g_cameraPitch < -maxPitch)
                    {
                        g_cameraPitch = -maxPitch;
                    }
                    if (g_cameraPitch > maxPitch)
                    {
                        g_cameraPitch = maxPitch;
                    }
                }

                g_previousMousePosition = mousePos;
                g_bHasPreviousMousePosition = true;
            }
            else
            {
                // ローカル実行ではカーソルを中央へ戻し、その差分を回転量に使う。
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

                    const LONG deltaX = mousePos.x - screenCenter.x;
                    const LONG deltaY = mousePos.y - screenCenter.y;

                    g_cameraYaw += static_cast<float>(deltaX) * kMouseSensitivity;
                    g_cameraPitch -= static_cast<float>(deltaY) * kMouseSensitivity;
                    if (g_cameraPitch < -maxPitch)
                    {
                        g_cameraPitch = -maxPitch;
                    }
                    if (g_cameraPitch > maxPitch)
                    {
                        g_cameraPitch = maxPitch;
                    }

                    SetCursorPos(screenCenter.x, screenCenter.y);
                    ResetMouseLookTracking();
                }
            }
        }
    }

    // yaw と pitch から前方向ベクトルを組み立てる。
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

    // WASD / EQ から移動ベクトルを作る。
    if (D3DXVec3LengthSq(&move) > 0.0f)
    {
        const bool isShiftHeld = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
        const bool isHorizontalMoveHeld =
            ((GetAsyncKeyState('W') & 0x8000) != 0) ||
            ((GetAsyncKeyState('A') & 0x8000) != 0) ||
            ((GetAsyncKeyState('S') & 0x8000) != 0) ||
            ((GetAsyncKeyState('D') & 0x8000) != 0);
        float moveSpeedScale = 1.0f;
        if (isShiftHeld && isHorizontalMoveHeld)
        {
            moveSpeedScale = kCameraSlowMoveScale;
        }

        // 斜め移動だけ速くならないように正規化する。
        D3DXVec3Normalize(&move, &move);
        g_cameraPosition += move * (kCameraMoveSpeed * moveSpeedScale * deltaTime);
    }
}
