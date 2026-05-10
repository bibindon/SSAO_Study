#include "app_shared.h"

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
    LoadSceneMeshInstance(_T("resource\\cube_red.x"), D3DXVECTOR3(-8.0f, 0.5f, -6.0f), 0.0f);
    LoadSceneMeshInstance(_T("resource\\cube_green.x"), D3DXVECTOR3(-3.5f, 0.5f, -7.5f), 0.0f);
    LoadSceneMeshInstance(_T("resource\\cube_blue.x"), D3DXVECTOR3(2.5f, 0.5f, -6.5f), -0.4f);
    LoadSceneMeshInstance(_T("resource\\sphere_orange.x"), D3DXVECTOR3(-6.5f, 1.2f, 3.0f), 0.0f);
    LoadSceneMeshInstance(_T("resource\\sphere_pink.x"), D3DXVECTOR3(-1.5f, 1.2f, 5.0f), 0.0f);
    LoadSceneMeshInstance(_T("resource\\sphere_yellowgreen.x"), D3DXVECTOR3(4.5f, 1.2f, 4.0f), 0.0f);
    LoadSceneMeshInstance(_T("resource\\cube_white.x"), D3DXVECTOR3(-8.8f, 0.5f, -1.0f), 0.1f);
    LoadSceneMeshInstance(_T("resource\\cube_black.x"), D3DXVECTOR3(8.6f, 0.5f, 2.5f), -0.2f);
    LoadSceneMeshInstance(_T("resource\\sphere_orange.x"), D3DXVECTOR3(-8.6f, 8.4f, 5.5f), 0.0f);
    LoadSceneMeshInstance(_T("resource\\sphere_pink.x"), D3DXVECTOR3(0.0f, 8.5f, -7.2f), 0.0f);
    LoadSceneMeshInstance(_T("resource\\cube_red.x"), D3DXVECTOR3(6.8f, 8.3f, 9.0f), 0.0f);
    LoadSceneMeshInstance(_T("resource\\cube_green.x"), D3DXVECTOR3(-4.0f, 8.2f, -9.0f), 0.0f);

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
                                D3DFMT_R32F,
                                D3DPOOL_DEFAULT,
                                &g_pRawDepthRenderTarget);
    assert(hResult == S_OK);

    hResult = D3DXCreateTexture(g_pd3dDevice,
                                kRenderWidth, kRenderHeight,
                                1,
                                D3DUSAGE_RENDERTARGET,
                                D3DFMT_R32F,
                                D3DPOOL_DEFAULT,
                                &g_pBackDepthRenderTarget);
    assert(hResult == S_OK);

    hResult = D3DXCreateTexture(g_pd3dDevice,
                                kRenderWidth, kRenderHeight,
                                1,
                                D3DUSAGE_RENDERTARGET,
                                D3DFMT_A16B16G16R16F,
                                D3DPOOL_DEFAULT,
                                &g_pNormalRenderTarget);
    assert(hResult == S_OK);

    hResult = D3DXCreateTexture(g_pd3dDevice,
                                kRenderWidth, kRenderHeight,
                                1,
                                D3DUSAGE_RENDERTARGET,
                                D3DFMT_R32F,
                                D3DPOOL_DEFAULT,
                                &g_pThicknessRenderTarget);
    assert(hResult == S_OK);

    hResult = D3DXCreateTexture(g_pd3dDevice,
                                kRenderWidth, kRenderHeight,
                                1,
                                D3DUSAGE_RENDERTARGET,
                                D3DFMT_A8R8G8B8,
                                D3DPOOL_DEFAULT,
                                &g_pSsaoRenderTarget);
    assert(hResult == S_OK);

    hResult = D3DXCreateTexture(g_pd3dDevice,
                                kRenderWidth, kRenderHeight,
                                1,
                                D3DUSAGE_RENDERTARGET,
                                D3DFMT_A8R8G8B8,
                                D3DPOOL_DEFAULT,
                                &g_pSsaoBlurRenderTarget);
    assert(hResult == S_OK);

    hResult = D3DXCreateTexture(g_pd3dDevice,
                                1, 1,
                                1,
                                D3DUSAGE_RENDERTARGET,
                                D3DFMT_R32F,
                                D3DPOOL_DEFAULT,
                                &g_pCenterDepthResolveTexture);
    assert(hResult == S_OK);

    hResult = g_pd3dDevice->CreateOffscreenPlainSurface(1,
                                                        1,
                                                        D3DFMT_R32F,
                                                        D3DPOOL_SYSTEMMEM,
                                                        &g_pCenterDepthReadbackSurface,
                                                        NULL);
    assert(hResult == S_OK);

    D3DVERTEXELEMENT9 elems[] =
    {
        { 0,  0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        D3DDECL_END()
    };
    hResult = g_pd3dDevice->CreateVertexDeclaration(elems, &g_pQuadDecl);
    assert(hResult == S_OK);

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
    SAFE_RELEASE(g_pRenderTarget);
    SAFE_RELEASE(g_pDepthRenderTarget);
    SAFE_RELEASE(g_pRawDepthRenderTarget);
    SAFE_RELEASE(g_pBackDepthRenderTarget);
    SAFE_RELEASE(g_pNormalRenderTarget);
    SAFE_RELEASE(g_pThicknessRenderTarget);
    SAFE_RELEASE(g_pSsaoRenderTarget);
    SAFE_RELEASE(g_pSsaoBlurRenderTarget);
    SAFE_RELEASE(g_pCenterDepthResolveTexture);
    SAFE_RELEASE(g_pCenterDepthReadbackSurface);
    SAFE_RELEASE(g_pQuadDecl);
    SAFE_RELEASE(g_pSprite);
    if (g_hToolDialog)
    {
        DestroyWindow(g_hToolDialog);
        g_hToolDialog = NULL;
    }
    if (g_hToolDialogFont)
    {
        DeleteObject(g_hToolDialogFont);
        g_hToolDialogFont = NULL;
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

void ApplyToolDialogFont(HWND controlHandle)
{
    if (controlHandle != NULL && g_hToolDialogFont != NULL)
    {
        SendMessage(controlHandle, WM_SETFONT, reinterpret_cast<WPARAM>(g_hToolDialogFont), TRUE);
    }
}
