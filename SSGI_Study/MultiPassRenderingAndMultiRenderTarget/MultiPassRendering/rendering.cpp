#include "app_shared.h"
// 操作方法や調整値のオーバーレイ表示。
void DrawOverlayText()
{
    const float activeDepthRange = GetActiveSsaoDepthRange();
    const float activeSampleDistanceMeters = GetActiveSsaoSampleDistanceMeters();
    const float fps = 60.0f;
    TCHAR lines[17][128] =
    {
        _T("Indirect light controls"),
        _T(""),
        _T("W/A/S/D: move"),
        _T("E / Q: move up / down"),
        _T("Mouse: look around"),
        _T("F1: show depth info"),
        _T("F2: show normal info"),
        _T("F3: show thickness info"),
        _T("F4: show back depth info"),
        _T("2 / Esc: toggle mouse cursor"),
        _T("3: toggle lambert lighting"),
        _T("4: toggle mesh dialog"),
        _T("6: toggle texture"),
        _T(""),
        _T(""),
        _T(""),
        _T(""),
    };
    _stprintf_s(lines[1], _T("FPS: %.1f"), fps);
    const TCHAR* remoteDesktopCameraText = _T("OFF");
    if (g_bRemoteDesktopCameraMode)
    {
        remoteDesktopCameraText = _T("ON");
    }
    _stprintf_s(lines[2], _T("Remote Desktop camera: %s"), remoteDesktopCameraText);
    _stprintf_s(lines[12], _T("Texture: %s"), g_bUseTexture ? _T("on") : _T("off"));
    _stprintf_s(lines[13], _T("GI depth range: %.1f m"), activeDepthRange);
    _stprintf_s(lines[14], _T("GI sample dist: %.2f m"), activeSampleDistanceMeters);
    _stprintf_s(lines[15], _T("Indirect light: %.2f"), g_indirectLightStrength);
    _stprintf_s(lines[16], _T("Indirect max: %.2f"), g_indirectLightMaxContribution);

    for (int i = 0; i < _countof(lines); ++i)
    {
        TextDraw(g_pFont, lines[i], 8, 8 + i * 22);
    }
}

// シーン内の全ジオメトリを描画する。
void DrawSceneGeometry(const D3DXMATRIX& View, const D3DXMATRIX& Proj)
{
    HRESULT hResult = E_FAIL;

    D3DXMATRIX largeCubeWorld;
    D3DXMATRIX largeCubeWorldView;
    D3DXMATRIX largeCubeWorldViewProj;
    D3DXMatrixTranslation(&largeCubeWorld, 0.0f, 0.0f, 0.0f);
    largeCubeWorldView = largeCubeWorld * View;
    largeCubeWorldViewProj = largeCubeWorld * View * Proj;
    hResult = g_pEffect1->SetMatrix("g_matWorld", &largeCubeWorld); assert(hResult == S_OK);
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
    hResult = g_pEffect1->SetMatrix("g_matWorld", &plateWorld); assert(hResult == S_OK);
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
        hResult = g_pEffect1->SetMatrix("g_matWorld", &sceneMeshWorld); assert(hResult == S_OK);
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
        hResult = g_pEffect1->SetMatrix("g_matWorld", &userMeshWorld); assert(hResult == S_OK);
        hResult = g_pEffect1->SetMatrix("g_matWorldView", &userMeshWorldView); assert(hResult == S_OK);
        hResult = g_pEffect1->SetMatrix("g_matWorldViewProj", &userMeshWorldViewProj); assert(hResult == S_OK);
        for (DWORD i = 0; i < userMesh.numMaterials; i++)
        {
            hResult = g_pEffect1->SetTexture("texture1", userMesh.textures[i]); assert(hResult == S_OK);
            hResult = g_pEffect1->CommitChanges();                                assert(hResult == S_OK);
            hResult = userMesh.mesh->DrawSubset(i);                               assert(hResult == S_OK);
        }
    }

}

// 現在有効な GI サンプル距離を返す。
float GetActiveSsaoSampleDistanceMeters()
{
    if (g_bAutoScaleSsaoByCenterDepth)
    {
        return g_autoSsaoSampleDistanceMeters;
    }

    return g_simpleSsaoSampleDistanceMeters;
}

// 現在有効な GI 深度範囲を返す。
float GetActiveSsaoDepthRange()
{
    if (g_bAutoScaleSsaoByCenterDepth)
    {
        return g_autoSsaoDepthRange;
    }

    return g_ssaoDepthRange;
}

// 画面中央付近の深度から GI の距離パラメータを自動調整する。
void UpdateAutoSsaoParametersFromCenterDepth()
{
    if (!g_bAutoScaleSsaoByCenterDepth || g_pCenterDepthResolveTexture == NULL || g_pCenterDepthReadbackSurface == NULL)
    {
        return;
    }

    HRESULT hResult = E_FAIL;
    LPDIRECT3DSURFACE9 pDepthSurface = NULL;
    LPDIRECT3DSURFACE9 pResolveSurface = NULL;

    hResult = g_pRawDepthRenderTarget->GetSurfaceLevel(0, &pDepthSurface);
    assert(hResult == S_OK);
    hResult = g_pCenterDepthResolveTexture->GetSurfaceLevel(0, &pResolveSurface);
    assert(hResult == S_OK);

    // 中央寄りの複数点を見る。
    const float sampleFractions[4] = { 0.2f, 0.4f, 0.6f, 0.8f };

    float nearestDepthMeters = kCameraFarPlane;
    bool hasValidDepth = false;
    for (int sampleYIndex = 0; sampleYIndex < _countof(sampleFractions); ++sampleYIndex)
    {
        for (int sampleXIndex = 0; sampleXIndex < _countof(sampleFractions); ++sampleXIndex)
        {
            const LONG sampleX = static_cast<LONG>(sampleFractions[sampleXIndex] * static_cast<float>(kRenderWidth - 1));
            const LONG sampleY = static_cast<LONG>(sampleFractions[sampleYIndex] * static_cast<float>(kRenderHeight - 1));
            RECT sampleRect =
            {
                sampleX,
                sampleY,
                sampleX + 1,
                sampleY + 1
            };
            // 1 ピクセルだけ 1x1 テクスチャへ解決して読み戻す。
            hResult = g_pd3dDevice->StretchRect(pDepthSurface, &sampleRect, pResolveSurface, NULL, D3DTEXF_POINT);
            assert(hResult == S_OK);

            hResult = g_pd3dDevice->GetRenderTargetData(pResolveSurface, g_pCenterDepthReadbackSurface);
            assert(hResult == S_OK);

            D3DLOCKED_RECT lockedRect = { };
            hResult = g_pCenterDepthReadbackSurface->LockRect(&lockedRect, NULL, D3DLOCK_READONLY);
            assert(hResult == S_OK);

            // 静的解析向けにも、読み戻しポインタが有効であることを明示しておく。
            if (lockedRect.pBits == NULL)
            {
                hResult = g_pCenterDepthReadbackSurface->UnlockRect();
                assert(hResult == S_OK);
                continue;
            }

            const float sampleDepthMeters = *reinterpret_cast<const float*>(lockedRect.pBits);
            hResult = g_pCenterDepthReadbackSurface->UnlockRect();
            assert(hResult == S_OK);

            if (sampleDepthMeters > 0.0f && sampleDepthMeters < nearestDepthMeters)
            {
                nearestDepthMeters = sampleDepthMeters;
                hasValidDepth = true;
            }
        }
    }

    SAFE_RELEASE(pResolveSurface);
    SAFE_RELEASE(pDepthSurface);

    if (!hasValidDepth || nearestDepthMeters >= kCameraFarPlane)
    {
        return;
    }

    const float centerDepthMeters = nearestDepthMeters;
    float clampedCenterDepthMeters = centerDepthMeters;
    if (centerDepthMeters > 15.0f)
    {
        clampedCenterDepthMeters = 15.0f;
    }
    float newTargetAutoSsaoDepthRange = clampedCenterDepthMeters * 2.0f;
    float newTargetAutoSsaoSampleDistanceMeters = clampedCenterDepthMeters * 0.25f;

    if (newTargetAutoSsaoDepthRange < kMinSsaoDepthRange)
    {
        newTargetAutoSsaoDepthRange = kMinSsaoDepthRange;
    }
    if (newTargetAutoSsaoDepthRange > kCameraFarPlane)
    {
        newTargetAutoSsaoDepthRange = kCameraFarPlane;
    }

    if (newTargetAutoSsaoSampleDistanceMeters < 0.0f)
    {
        newTargetAutoSsaoSampleDistanceMeters = 0.0f;
    }
    if (newTargetAutoSsaoSampleDistanceMeters > kCameraFarPlane)
    {
        newTargetAutoSsaoSampleDistanceMeters = kCameraFarPlane;
    }

    g_targetAutoSsaoDepthRange = newTargetAutoSsaoDepthRange;
    g_targetAutoSsaoSampleDistanceMeters = newTargetAutoSsaoSampleDistanceMeters;

    if (g_bSmoothAutoSsaoByCenterDepth)
    {
        const float deltaTime = 1.0f / 60.0f;
        const float depthRangeStep = (kAutoSsaoMaxDepthRange / kAutoSsaoSmoothingDurationSeconds) * deltaTime;
        const float sampleDistanceStep = (kAutoSsaoMaxSampleDistanceMeters / kAutoSsaoSmoothingDurationSeconds) * deltaTime;

        if (g_autoSsaoDepthRange < g_targetAutoSsaoDepthRange)
        {
            g_autoSsaoDepthRange += depthRangeStep;
            if (g_autoSsaoDepthRange > g_targetAutoSsaoDepthRange)
            {
                g_autoSsaoDepthRange = g_targetAutoSsaoDepthRange;
            }
        }
        else
        {
            g_autoSsaoDepthRange -= depthRangeStep;
            if (g_autoSsaoDepthRange < g_targetAutoSsaoDepthRange)
            {
                g_autoSsaoDepthRange = g_targetAutoSsaoDepthRange;
            }
        }

        if (g_autoSsaoSampleDistanceMeters < g_targetAutoSsaoSampleDistanceMeters)
        {
            g_autoSsaoSampleDistanceMeters += sampleDistanceStep;
            if (g_autoSsaoSampleDistanceMeters > g_targetAutoSsaoSampleDistanceMeters)
            {
                g_autoSsaoSampleDistanceMeters = g_targetAutoSsaoSampleDistanceMeters;
            }
        }
        else
        {
            g_autoSsaoSampleDistanceMeters -= sampleDistanceStep;
            if (g_autoSsaoSampleDistanceMeters < g_targetAutoSsaoSampleDistanceMeters)
            {
                g_autoSsaoSampleDistanceMeters = g_targetAutoSsaoSampleDistanceMeters;
            }
        }
    }
    else
    {
        g_autoSsaoDepthRange = g_targetAutoSsaoDepthRange;
        g_autoSsaoSampleDistanceMeters = g_targetAutoSsaoSampleDistanceMeters;
    }
}

// 第 1 パス。カラー、深度、法線、実深度、背面深度を作る。
void RenderPass1()
{
    HRESULT hResult = E_FAIL;
    const float activeSsaoDepthRange = GetActiveSsaoDepthRange();

    // 元のバックバッファを保存。
    LPDIRECT3DSURFACE9 pOldRT0 = NULL;
    hResult = g_pd3dDevice->GetRenderTarget(0, &pOldRT0);
    assert(hResult == S_OK);

    // MRT と背面深度用の RT サーフェスを取得
    LPDIRECT3DSURFACE9 pRT0 = NULL;
    LPDIRECT3DSURFACE9 pRT1 = NULL;
    LPDIRECT3DSURFACE9 pRT2 = NULL;
    LPDIRECT3DSURFACE9 pRT3 = NULL;
    LPDIRECT3DSURFACE9 pRT4 = NULL;
    hResult = g_pRenderTarget->GetSurfaceLevel(0, &pRT0);  assert(hResult == S_OK);
    hResult = g_pDepthRenderTarget->GetSurfaceLevel(0, &pRT1); assert(hResult == S_OK);
    hResult = g_pNormalRenderTarget->GetSurfaceLevel(0, &pRT2); assert(hResult == S_OK);
    hResult = g_pRawDepthRenderTarget->GetSurfaceLevel(0, &pRT3); assert(hResult == S_OK);
    hResult = g_pBackDepthRenderTarget->GetSurfaceLevel(0, &pRT4); assert(hResult == S_OK);

    // 複数レンダーターゲットを有効化。
    hResult = g_pd3dDevice->SetRenderTarget(0, pRT0); assert(hResult == S_OK);
    hResult = g_pd3dDevice->SetRenderTarget(1, pRT1); assert(hResult == S_OK);
    hResult = g_pd3dDevice->SetRenderTarget(2, pRT2); assert(hResult == S_OK);
    hResult = g_pd3dDevice->SetRenderTarget(3, pRT3); assert(hResult == S_OK);

    // View / Projection 行列を組み立てる。
    D3DXMATRIX View, Proj;

    D3DXMatrixPerspectiveFovLH(&Proj,
                               D3DXToRadian(45),
                               static_cast<float>(kRenderWidth) / static_cast<float>(kRenderHeight),
                               kCameraNearPlane,
                               kCameraFarPlane);

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

    // MRT で中間情報を書き出す。
    hResult = g_pEffect1->SetTechnique("TechniqueMRT");
    assert(hResult == S_OK);

    UINT numPass = 0;
    hResult = g_pEffect1->Begin(&numPass, 0); assert(hResult == S_OK);
    hResult = g_pEffect1->BeginPass(0);       assert(hResult == S_OK);

    hResult = g_pEffect1->SetBool("g_bUseTexture", g_bUseTexture ? TRUE : FALSE); assert(hResult == S_OK);
    if (g_bUseLambertLighting)
    {
        hResult = g_pEffect1->SetBool("g_bUseLambert", TRUE); assert(hResult == S_OK);
    }
    else
    {
        hResult = g_pEffect1->SetBool("g_bUseLambert", FALSE); assert(hResult == S_OK);
    }
    hResult = g_pEffect1->SetFloat("g_ssaoDepthRange", activeSsaoDepthRange); assert(hResult == S_OK);
    DrawSceneGeometry(View, Proj);

    hResult = g_pEffect1->EndPass(); assert(hResult == S_OK);
    hResult = g_pEffect1->End();     assert(hResult == S_OK);
    hResult = g_pd3dDevice->EndScene(); assert(hResult == S_OK);

    // MRT を解除し、背面深度用の描画へ切り替える。
    hResult = g_pd3dDevice->SetRenderTarget(2, NULL); assert(hResult == S_OK);
    hResult = g_pd3dDevice->SetRenderTarget(1, NULL); assert(hResult == S_OK);
    hResult = g_pd3dDevice->SetRenderTarget(3, NULL); assert(hResult == S_OK);
    hResult = g_pd3dDevice->SetRenderTarget(0, pRT4); assert(hResult == S_OK);

    hResult = g_pd3dDevice->Clear(0, NULL,
                                  D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                                  D3DCOLOR_XRGB(255, 255, 255),
                                  1.0f, 0);
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->BeginScene(); assert(hResult == S_OK);

    hResult = g_pEffect1->SetTechnique("TechniqueBackDepth");
    assert(hResult == S_OK);

    hResult = g_pEffect1->Begin(&numPass, 0); assert(hResult == S_OK);
    hResult = g_pEffect1->BeginPass(0);       assert(hResult == S_OK);

    DrawSceneGeometry(View, Proj);

    hResult = g_pEffect1->EndPass(); assert(hResult == S_OK);
    hResult = g_pEffect1->End();     assert(hResult == S_OK);

    hResult = g_pd3dDevice->EndScene(); assert(hResult == S_OK);

    // バックバッファへ戻す。
    hResult = g_pd3dDevice->SetRenderTarget(2, NULL);   assert(hResult == S_OK);
    hResult = g_pd3dDevice->SetRenderTarget(1, NULL);   assert(hResult == S_OK);
    hResult = g_pd3dDevice->SetRenderTarget(0, pOldRT0); assert(hResult == S_OK);

    SAFE_RELEASE(pRT0);
    SAFE_RELEASE(pRT1);
    SAFE_RELEASE(pRT2);
    SAFE_RELEASE(pRT3);
    SAFE_RELEASE(pRT4);
    SAFE_RELEASE(pOldRT0);
}

// 第 2 パス。thickness、間接光、ぼかし、最終合成を行う。
void RenderPass2()
{
    HRESULT hResult = E_FAIL;
    const float activeSsaoDepthRange = GetActiveSsaoDepthRange();
    const float activeSsaoSampleDistanceMeters = GetActiveSsaoSampleDistanceMeters();
    LPDIRECT3DSURFACE9 pOldRT0 = NULL;
    LPDIRECT3DSURFACE9 pThicknessRT = NULL;
    LPDIRECT3DSURFACE9 pSsaoRT = NULL;
    LPDIRECT3DSURFACE9 pSsaoBlurRT = NULL;

    // まず thickness を作る。
    hResult = g_pd3dDevice->GetRenderTarget(0, &pOldRT0); assert(hResult == S_OK);
    hResult = g_pThicknessRenderTarget->GetSurfaceLevel(0, &pThicknessRT); assert(hResult == S_OK);
    hResult = g_pd3dDevice->SetRenderTarget(0, pThicknessRT); assert(hResult == S_OK);

    hResult = g_pd3dDevice->Clear(0, NULL,
                                  D3DCLEAR_TARGET,
                                  D3DCOLOR_XRGB(0, 0, 0),
                                  1.0f, 0);
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->BeginScene(); assert(hResult == S_OK);

    hResult = g_pEffect2->SetTechnique("TechniqueThickness"); assert(hResult == S_OK);

    UINT thicknessNumPass = 0;
    hResult = g_pEffect2->Begin(&thicknessNumPass, 0); assert(hResult == S_OK);
    hResult = g_pEffect2->BeginPass(0);                assert(hResult == S_OK);

    if (g_bEnableThicknessCap)
    {
        hResult = g_pEffect2->SetBool("g_bEnableThicknessCap", TRUE); assert(hResult == S_OK);
    }
    else
    {
        hResult = g_pEffect2->SetBool("g_bEnableThicknessCap", FALSE); assert(hResult == S_OK);
    }
    hResult = g_pEffect2->SetFloat("g_thicknessCap", g_thicknessCapMeters / activeSsaoDepthRange); assert(hResult == S_OK);
    hResult = g_pEffect2->SetTexture("depthTexture", g_pDepthRenderTarget); assert(hResult == S_OK);
    hResult = g_pEffect2->SetTexture("backDepthTexture", g_pBackDepthRenderTarget); assert(hResult == S_OK);
    hResult = g_pEffect2->CommitChanges(); assert(hResult == S_OK);
    DrawFullscreenQuad();

    hResult = g_pEffect2->EndPass(); assert(hResult == S_OK);
    hResult = g_pEffect2->End();     assert(hResult == S_OK);
    hResult = g_pd3dDevice->EndScene(); assert(hResult == S_OK);

    hResult = g_pd3dDevice->SetRenderTarget(0, pOldRT0); assert(hResult == S_OK);
    SAFE_RELEASE(pThicknessRT);

    hResult = g_pSsaoRenderTarget->GetSurfaceLevel(0, &pSsaoRT); assert(hResult == S_OK);
    hResult = g_pd3dDevice->SetRenderTarget(0, pSsaoRT); assert(hResult == S_OK);

    hResult = g_pd3dDevice->Clear(0, NULL,
                                  D3DCLEAR_TARGET,
                                  D3DCOLOR_XRGB(255, 255, 255),
                                  1.0f, 0);
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->BeginScene(); assert(hResult == S_OK);

    if (g_simpleSsaoSampleCount <= 4)
    {
        hResult = g_pEffect2->SetTechnique("TechniqueSsao4"); assert(hResult == S_OK);
    }
    else if (g_simpleSsaoSampleCount <= 8)
    {
        hResult = g_pEffect2->SetTechnique("TechniqueSsao8"); assert(hResult == S_OK);
    }
    else if (g_simpleSsaoSampleCount <= 16)
    {
        hResult = g_pEffect2->SetTechnique("TechniqueSsao16"); assert(hResult == S_OK);
    }
    else if (g_simpleSsaoSampleCount <= 32)
    {
        hResult = g_pEffect2->SetTechnique("TechniqueSsao32"); assert(hResult == S_OK);
    }
    else
    {
        hResult = g_pEffect2->SetTechnique("TechniqueSsao64"); assert(hResult == S_OK);
    }

    UINT ssaoNumPass = 0;
    hResult = g_pEffect2->Begin(&ssaoNumPass, 0); assert(hResult == S_OK);
    hResult = g_pEffect2->BeginPass(0);           assert(hResult == S_OK);

    if (g_bUseThicknessForSsao)
    {
        hResult = g_pEffect2->SetBool("g_bUseThicknessForSsao", TRUE); assert(hResult == S_OK);
    }
    else
    {
        hResult = g_pEffect2->SetBool("g_bUseThicknessForSsao", FALSE); assert(hResult == S_OK);
    }
    if (g_bDepthScaledSampleDistance)
    {
        hResult = g_pEffect2->SetBool("g_bDepthScaledSampleDistance", TRUE); assert(hResult == S_OK);
    }
    else
    {
        hResult = g_pEffect2->SetBool("g_bDepthScaledSampleDistance", FALSE); assert(hResult == S_OK);
    }
    if (g_bUseFixedSsaoSampleDistance)
    {
        hResult = g_pEffect2->SetBool("g_bUseFixedSsaoSampleDistance", TRUE); assert(hResult == S_OK);
    }
    else
    {
        hResult = g_pEffect2->SetBool("g_bUseFixedSsaoSampleDistance", FALSE); assert(hResult == S_OK);
    }
    // 深度復元用に投影スケールを渡す。
    const float verticalFovRadians = D3DXToRadian(45.0f);
    const float projectionScaleY = 1.0f / tanf(verticalFovRadians * 0.5f);
    const float projectionScaleX = projectionScaleY / (static_cast<float>(kRenderWidth) / static_cast<float>(kRenderHeight));
    float projectionScale[2] = { projectionScaleX, projectionScaleY };
    hResult = g_pEffect2->SetFloat("g_simpleSsaoSampleDistanceMeters", activeSsaoSampleDistanceMeters); assert(hResult == S_OK);
    hResult = g_pEffect2->SetFloatArray("g_projectionScale", projectionScale, 2); assert(hResult == S_OK);
    hResult = g_pEffect2->SetFloat("g_thicknessScale", g_thicknessScale); assert(hResult == S_OK);
    hResult = g_pEffect2->SetFloat("g_ssaoDepthRange", activeSsaoDepthRange); assert(hResult == S_OK);
    hResult = g_pEffect2->SetFloat("g_depthCompareThreshold", g_depthCompareDistance / activeSsaoDepthRange); assert(hResult == S_OK);
    hResult = g_pEffect2->SetFloat("g_sampleDepthBiasThreshold", g_sampleDepthBiasDistance / activeSsaoDepthRange); assert(hResult == S_OK);
    hResult = g_pEffect2->SetFloat("g_targetNormalBiasScale", g_targetNormalBiasScale); assert(hResult == S_OK);
    hResult = g_pEffect2->SetFloat("g_targetDepthBiasScale", g_targetDepthBiasScale); assert(hResult == S_OK);
    hResult = g_pEffect2->SetFloat("g_indirectLightStrength", g_indirectLightStrength); assert(hResult == S_OK);
    hResult = g_pEffect2->SetFloat("g_indirectLightMaxContribution", g_indirectLightMaxContribution); assert(hResult == S_OK);
    hResult = g_pEffect2->SetInt("g_indirectLightBlendMode", g_indirectLightBlendMode); assert(hResult == S_OK);
    if (g_bLockSsaoRandomDirections)
    {
        hResult = g_pEffect2->SetBool("g_bLockSsaoRandomDirections", TRUE); assert(hResult == S_OK);
    }
    else
    {
        hResult = g_pEffect2->SetBool("g_bLockSsaoRandomDirections", FALSE); assert(hResult == S_OK);
    }
    hResult = g_pEffect2->SetTexture("texture1", g_pRenderTarget); assert(hResult == S_OK);
    hResult = g_pEffect2->SetTexture("depthTexture", g_pDepthRenderTarget); assert(hResult == S_OK);
    hResult = g_pEffect2->SetTexture("thicknessTexture", g_pThicknessRenderTarget); assert(hResult == S_OK);
    hResult = g_pEffect2->SetTexture("normalTexture", g_pNormalRenderTarget); assert(hResult == S_OK);
    hResult = g_pEffect2->CommitChanges(); assert(hResult == S_OK);
    DrawFullscreenQuad();

    hResult = g_pEffect2->EndPass(); assert(hResult == S_OK);
    hResult = g_pEffect2->End();     assert(hResult == S_OK);
    hResult = g_pd3dDevice->EndScene(); assert(hResult == S_OK);

    // 必要なら間接光を後段でぼかす。
    if (g_bEnableSsaoBlur)
    {
        hResult = g_pSsaoBlurRenderTarget->GetSurfaceLevel(0, &pSsaoBlurRT); assert(hResult == S_OK);
        hResult = g_pd3dDevice->SetRenderTarget(0, pSsaoBlurRT); assert(hResult == S_OK);

        hResult = g_pd3dDevice->Clear(0, NULL,
                                      D3DCLEAR_TARGET,
                                      D3DCOLOR_XRGB(255, 255, 255),
                                      1.0f, 0);
        assert(hResult == S_OK);

        hResult = g_pd3dDevice->BeginScene(); assert(hResult == S_OK);

        if (g_ssaoBlurKernelSize >= 21)
        {
            hResult = g_pEffect2->SetTechnique("TechniqueSsaoBlurXLarge"); assert(hResult == S_OK);
        }
        else if (g_ssaoBlurKernelSize >= 11)
        {
            hResult = g_pEffect2->SetTechnique("TechniqueSsaoBlurLarge"); assert(hResult == S_OK);
        }
        else
        {
            hResult = g_pEffect2->SetTechnique("TechniqueSsaoBlur"); assert(hResult == S_OK);
        }

    UINT blurNumPass = 0;
    hResult = g_pEffect2->Begin(&blurNumPass, 0); assert(hResult == S_OK);
    hResult = g_pEffect2->BeginPass(0);           assert(hResult == S_OK);

        hResult = g_pEffect2->SetTexture("ssaoTexture", g_pSsaoRenderTarget); assert(hResult == S_OK);
        hResult = g_pEffect2->SetTexture("depthTexture", g_pDepthRenderTarget); assert(hResult == S_OK);
        hResult = g_pEffect2->SetTexture("normalTexture", g_pNormalRenderTarget); assert(hResult == S_OK);
    hResult = g_pEffect2->CommitChanges(); assert(hResult == S_OK);
        DrawFullscreenQuad();

        hResult = g_pEffect2->EndPass(); assert(hResult == S_OK);
        hResult = g_pEffect2->End();     assert(hResult == S_OK);
        hResult = g_pd3dDevice->EndScene(); assert(hResult == S_OK);
    }

    hResult = g_pd3dDevice->SetRenderTarget(0, pOldRT0); assert(hResult == S_OK);
    SAFE_RELEASE(pSsaoRT);
    SAFE_RELEASE(pSsaoBlurRT);

    hResult = g_pd3dDevice->Clear(0, NULL,
                                  D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
                                  D3DCOLOR_XRGB(0, 0, 0),
                                  1.0f, 0);
    assert(hResult == S_OK);

    // ここからはフルスクリーンクアッドのみなので Z テストは不要。
    hResult = g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->BeginScene(); assert(hResult == S_OK);

    hResult = g_pEffect2->SetTechnique("TechniqueComposite"); assert(hResult == S_OK);

    UINT numPass = 0;
    hResult = g_pEffect2->Begin(&numPass, 0);               assert(hResult == S_OK);
    hResult = g_pEffect2->BeginPass(0);                     assert(hResult == S_OK);

    hResult = g_pEffect2->SetTexture("texture1", g_pRenderTarget); assert(hResult == S_OK);
    if (g_bEnableSsaoBlur)
    {
        hResult = g_pEffect2->SetTexture("ssaoTexture", g_pSsaoBlurRenderTarget); assert(hResult == S_OK);
    }
    else
    {
        hResult = g_pEffect2->SetTexture("ssaoTexture", g_pSsaoRenderTarget); assert(hResult == S_OK);
    }
    hResult = g_pEffect2->CommitChanges();                          assert(hResult == S_OK);

    DrawFullscreenQuad();

    hResult = g_pEffect2->EndPass(); assert(hResult == S_OK);
    hResult = g_pEffect2->End();     assert(hResult == S_OK);

    // 中間テクスチャのデバッグ表示。
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

        const bool isSingleChannelDebug = (g_debugViewMode != kDebugViewNormal);
        LPDIRECT3DTEXTURE9 pDebugTexture = g_pDepthRenderTarget;
        if (g_debugViewMode == kDebugViewNormal)
        {
            pDebugTexture = g_pNormalRenderTarget;
        }
        else if (g_debugViewMode == kDebugViewThickness)
        {
            pDebugTexture = g_pThicknessRenderTarget;
        }
        else if (g_debugViewMode == kDebugViewBackDepth)
        {
            pDebugTexture = g_pBackDepthRenderTarget;
        }

        if (isSingleChannelDebug)
        {
            hResult = g_pEffect2->SetBool("g_bSingleChannelInput", TRUE); assert(hResult == S_OK);
        }
        else
        {
            hResult = g_pEffect2->SetBool("g_bSingleChannelInput", FALSE); assert(hResult == S_OK);
        }
        hResult = g_pEffect2->SetTexture("texture1", pDebugTexture); assert(hResult == S_OK);
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

    UpdateAutoSsaoParametersFromCenterDepth();
    SAFE_RELEASE(pOldRT0);
}

// 画面全体を覆うフルスクリーンクアッド。
void DrawFullscreenQuad()
{
    QuadVertex v[4] { };

    // UV を texel 中心へ少し寄せる。
    float du = 0.5f / static_cast<float>(kRenderWidth);
    float dv = 0.5f / static_cast<float>(kRenderHeight);

    v[0].x = -1.0f; v[0].y = -1.0f; v[0].z = 0.0f; v[0].w = 1.0f; v[0].u = 0.0f + du; v[0].v = 1.0f - dv;
    v[1].x = -1.0f; v[1].y = 1.0f; v[1].z = 0.0f; v[1].w = 1.0f; v[1].u = 0.0f + du; v[1].v = 0.0f + dv;
    v[2].x = 1.0f; v[2].y = -1.0f; v[2].z = 0.0f; v[2].w = 1.0f; v[2].u = 1.0f - du; v[2].v = 1.0f - dv;
    v[3].x = 1.0f; v[3].y = 1.0f; v[3].z = 0.0f; v[3].w = 1.0f; v[3].u = 1.0f - du; v[3].v = 0.0f + dv;

    g_pd3dDevice->SetVertexDeclaration(g_pQuadDecl);
    g_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(QuadVertex));
}
