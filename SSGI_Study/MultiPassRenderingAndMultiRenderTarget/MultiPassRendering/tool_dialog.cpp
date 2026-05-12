#include "app_shared.h"
// 実行中に SSAO パラメータを調整するためのダイアログ。
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
                                   340,
                                   972,
                                   g_hWnd,
                                   NULL,
                                   wc.hInstance,
                                   NULL);
    assert(g_hToolDialog != NULL);

    if (g_hToolDialogFont == NULL)
    {
        NONCLIENTMETRICS metrics = { };
        metrics.cbSize = sizeof(metrics);
        if (SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
        {
            LOGFONT logFont = metrics.lfMessageFont;
            if (logFont.lfHeight < 0)
            {
                logFont.lfHeight += 2;
            }
            else
            {
                if (logFont.lfHeight > 10)
                {
                    logFont.lfHeight = logFont.lfHeight - 2;
                }
                else
                {
                    logFont.lfHeight = 8;
                }
            }
            g_hToolDialogFont = CreateFontIndirect(&logFont);
        }
    }

    // 以降のコントロールは、ほぼそのままシェーダーパラメータに対応する。
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
    ApplyToolDialogFont(g_hOpenMeshButton);

    HWND hSsaoSampleLabel = CreateWindow(_T("STATIC"),
                                         _T("SSAO sample dist (m):"),
                                         WS_CHILD | WS_VISIBLE,
                                         20,
                                         72,
                                         130,
                                         20,
                                         g_hToolDialog,
                                         NULL,
                                         wc.hInstance,
                                         NULL);
    ApplyToolDialogFont(hSsaoSampleLabel);

    g_hSsaoSampleEdit = CreateWindow(_T("EDIT"),
                                     _T("1.0"),
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
    ApplyToolDialogFont(g_hSsaoSampleEdit);

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
    ApplyToolDialogFont(g_hApplySsaoButton);

    HWND hSampleCountLabel = CreateWindow(_T("STATIC"),
                                          _T("SSAO sample count:"),
                                          WS_CHILD | WS_VISIBLE,
                                          20,
                                          104,
                                          130,
                                          20,
                                          g_hToolDialog,
                                          NULL,
                                          wc.hInstance,
                                          NULL);
    ApplyToolDialogFont(hSampleCountLabel);

    g_hSampleCountEdit = CreateWindow(_T("EDIT"),
                                      _T("1"),
                                      WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                      160,
                                      100,
                                      60,
                                      24,
                                      g_hToolDialog,
                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogSampleCountEditId)),
                                      wc.hInstance,
                                      NULL);
    assert(g_hSampleCountEdit != NULL);
    ApplyToolDialogFont(g_hSampleCountEdit);

    g_hApplySampleCountButton = CreateWindow(_T("BUTTON"),
                                             _T("Apply"),
                                             WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                             230,
                                             98,
                                             60,
                                             28,
                                             g_hToolDialog,
                                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogApplySampleCountButtonId)),
                                             wc.hInstance,
                                             NULL);
    assert(g_hApplySampleCountButton != NULL);
    ApplyToolDialogFont(g_hApplySampleCountButton);

    HWND hSsaoDepthRangeLabel = CreateWindow(_T("STATIC"),
                                             _T("SSAO depth range (m):"),
                                             WS_CHILD | WS_VISIBLE,
                                             20,
                                             136,
                                             130,
                                             20,
                                             g_hToolDialog,
                                             NULL,
                                             wc.hInstance,
                                             NULL);
    ApplyToolDialogFont(hSsaoDepthRangeLabel);

    g_hSsaoDepthRangeEdit = CreateWindow(_T("EDIT"),
                                         _T("50.0"),
                                         WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                         160,
                                         132,
                                         60,
                                         24,
                                         g_hToolDialog,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogSsaoDepthRangeEditId)),
                                         wc.hInstance,
                                         NULL);
    assert(g_hSsaoDepthRangeEdit != NULL);
    ApplyToolDialogFont(g_hSsaoDepthRangeEdit);

    g_hApplySsaoDepthRangeButton = CreateWindow(_T("BUTTON"),
                                                _T("Apply"),
                                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                                230,
                                                130,
                                                60,
                                                28,
                                                g_hToolDialog,
                                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogApplySsaoDepthRangeButtonId)),
                                                wc.hInstance,
                                                NULL);
    assert(g_hApplySsaoDepthRangeButton != NULL);
    ApplyToolDialogFont(g_hApplySsaoDepthRangeButton);

    g_hUseThicknessCheckbox = CreateWindow(_T("BUTTON"),
                                           _T("Use thickness for SSAO"),
                                           WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                           20,
                                           170,
                                           180,
                                           24,
                                           g_hToolDialog,
                                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogUseThicknessCheckboxId)),
                                           wc.hInstance,
                                           NULL);
    assert(g_hUseThicknessCheckbox != NULL);
    ApplyToolDialogFont(g_hUseThicknessCheckbox);
    if (g_bUseThicknessForSsao)
    {
        SendMessage(g_hUseThicknessCheckbox, BM_SETCHECK, BST_CHECKED, 0);
    }
    else
    {
        SendMessage(g_hUseThicknessCheckbox, BM_SETCHECK, BST_UNCHECKED, 0);
    }

    HWND hThicknessScaleLabel = CreateWindow(_T("STATIC"),
                                             _T("Thickness scale:"),
                                             WS_CHILD | WS_VISIBLE,
                                             20,
                                             202,
                                             130,
                                             20,
                                             g_hToolDialog,
                                             NULL,
                                             wc.hInstance,
                                             NULL);
    ApplyToolDialogFont(hThicknessScaleLabel);

    g_hThicknessScaleEdit = CreateWindow(_T("EDIT"),
                                         _T("1.0"),
                                         WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                         160,
                                         198,
                                         60,
                                         24,
                                         g_hToolDialog,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogThicknessScaleEditId)),
                                         wc.hInstance,
                                         NULL);
    assert(g_hThicknessScaleEdit != NULL);
    ApplyToolDialogFont(g_hThicknessScaleEdit);

    g_hApplyThicknessScaleButton = CreateWindow(_T("BUTTON"),
                                                _T("Apply"),
                                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                                230,
                                                196,
                                                60,
                                                28,
                                                g_hToolDialog,
                                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogApplyThicknessScaleButtonId)),
                                                wc.hInstance,
                                                NULL);
    assert(g_hApplyThicknessScaleButton != NULL);
    ApplyToolDialogFont(g_hApplyThicknessScaleButton);

    g_hRemoteDesktopCheckbox = CreateWindow(_T("BUTTON"),
                                            _T("Remote Desktop"),
                                            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                            20,
                                            238,
                                            180,
                                            24,
                                            g_hToolDialog,
                                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogRemoteDesktopCheckboxId)),
                                            wc.hInstance,
                                            NULL);
    assert(g_hRemoteDesktopCheckbox != NULL);
    ApplyToolDialogFont(g_hRemoteDesktopCheckbox);
    if (g_bRemoteDesktopCameraMode)
    {
        SendMessage(g_hRemoteDesktopCheckbox, BM_SETCHECK, BST_CHECKED, 0);
    }
    else
    {
        SendMessage(g_hRemoteDesktopCheckbox, BM_SETCHECK, BST_UNCHECKED, 0);
    }

    HWND hNormalBiasScaleLabel = CreateWindow(_T("STATIC"),
                                              _T("Normal bias scale:"),
                                              WS_CHILD | WS_VISIBLE,
                                              20,
                                              270,
                                              130,
                                              20,
                                              g_hToolDialog,
                                              NULL,
                                              wc.hInstance,
                                              NULL);
    ApplyToolDialogFont(hNormalBiasScaleLabel);

    g_hNormalBiasScaleEdit = CreateWindow(_T("EDIT"),
                                          _T("1.0"),
                                          WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                          160,
                                          266,
                                          60,
                                          24,
                                          g_hToolDialog,
                                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogNormalBiasScaleEditId)),
                                          wc.hInstance,
                                          NULL);
    assert(g_hNormalBiasScaleEdit != NULL);
    ApplyToolDialogFont(g_hNormalBiasScaleEdit);

    g_hApplyNormalBiasScaleButton = CreateWindow(_T("BUTTON"),
                                                 _T("Apply"),
                                                 WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                                 230,
                                                 264,
                                                 60,
                                                 28,
                                                 g_hToolDialog,
                                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogApplyNormalBiasScaleButtonId)),
                                                 wc.hInstance,
                                                 NULL);
    assert(g_hApplyNormalBiasScaleButton != NULL);
    ApplyToolDialogFont(g_hApplyNormalBiasScaleButton);

    HWND hDepthBiasScaleLabel = CreateWindow(_T("STATIC"),
                                             _T("Depth bias scale:"),
                                             WS_CHILD | WS_VISIBLE,
                                             20,
                                             302,
                                             130,
                                             20,
                                             g_hToolDialog,
                                             NULL,
                                             wc.hInstance,
                                             NULL);
    ApplyToolDialogFont(hDepthBiasScaleLabel);

    g_hDepthBiasScaleEdit = CreateWindow(_T("EDIT"),
                                         _T("1.0"),
                                         WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                         160,
                                         298,
                                         60,
                                         24,
                                         g_hToolDialog,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogDepthBiasScaleEditId)),
                                         wc.hInstance,
                                         NULL);
    assert(g_hDepthBiasScaleEdit != NULL);
    ApplyToolDialogFont(g_hDepthBiasScaleEdit);

    g_hApplyDepthBiasScaleButton = CreateWindow(_T("BUTTON"),
                                                _T("Apply"),
                                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                                230,
                                                296,
                                                60,
                                                28,
                                                g_hToolDialog,
                                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogApplyDepthBiasScaleButtonId)),
                                                wc.hInstance,
                                                NULL);
    assert(g_hApplyDepthBiasScaleButton != NULL);
    ApplyToolDialogFont(g_hApplyDepthBiasScaleButton);

    HWND hDepthCompareDistanceLabel = CreateWindow(_T("STATIC"),
                                                   _T("Depth compare dist:"),
                                                   WS_CHILD | WS_VISIBLE,
                                                   20,
                                                   334,
                                                   130,
                                                   20,
                                                   g_hToolDialog,
                                                   NULL,
                                                   wc.hInstance,
                                                   NULL);
    ApplyToolDialogFont(hDepthCompareDistanceLabel);

    g_hDepthCompareDistanceEdit = CreateWindow(_T("EDIT"),
                                               _T("0.10"),
                                               WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                               160,
                                               330,
                                               60,
                                               24,
                                               g_hToolDialog,
                                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogDepthCompareDistanceEditId)),
                                               wc.hInstance,
                                               NULL);
    assert(g_hDepthCompareDistanceEdit != NULL);
    ApplyToolDialogFont(g_hDepthCompareDistanceEdit);

    g_hApplyDepthCompareDistanceButton = CreateWindow(_T("BUTTON"),
                                                      _T("Apply"),
                                                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                                      230,
                                                      328,
                                                      60,
                                                      28,
                                                      g_hToolDialog,
                                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogApplyDepthCompareDistanceButtonId)),
                                                      wc.hInstance,
                                                      NULL);
    assert(g_hApplyDepthCompareDistanceButton != NULL);
    ApplyToolDialogFont(g_hApplyDepthCompareDistanceButton);

    g_hAllowStraightUpDownCheckbox = CreateWindow(_T("BUTTON"),
                                                  _T("Allow straight up/down"),
                                                  WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                                  20,
                                                  366,
                                                  200,
                                                  24,
                                                  g_hToolDialog,
                                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogAllowStraightUpDownCheckboxId)),
                                                  wc.hInstance,
                                                  NULL);
    assert(g_hAllowStraightUpDownCheckbox != NULL);
    ApplyToolDialogFont(g_hAllowStraightUpDownCheckbox);
    if (g_bAllowStraightUpDown)
    {
        SendMessage(g_hAllowStraightUpDownCheckbox, BM_SETCHECK, BST_CHECKED, 0);
    }
    else
    {
        SendMessage(g_hAllowStraightUpDownCheckbox, BM_SETCHECK, BST_UNCHECKED, 0);
    }

    HWND hDepthBiasDistanceLabel = CreateWindow(_T("STATIC"),
                                                _T("Depth bias dist:"),
                                                WS_CHILD | WS_VISIBLE,
                                                20,
                                                398,
                                                130,
                                                20,
                                                g_hToolDialog,
                                                NULL,
                                                wc.hInstance,
                                                NULL);
    ApplyToolDialogFont(hDepthBiasDistanceLabel);

    g_hDepthBiasDistanceEdit = CreateWindow(_T("EDIT"),
                                            _T("0.10"),
                                            WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                            160,
                                            394,
                                            60,
                                            24,
                                            g_hToolDialog,
                                            reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogDepthBiasDistanceEditId)),
                                            wc.hInstance,
                                            NULL);
    assert(g_hDepthBiasDistanceEdit != NULL);
    ApplyToolDialogFont(g_hDepthBiasDistanceEdit);

    g_hApplyDepthBiasDistanceButton = CreateWindow(_T("BUTTON"),
                                                   _T("Apply"),
                                                   WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                                   230,
                                                   392,
                                                   60,
                                                   28,
                                                   g_hToolDialog,
                                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogApplyDepthBiasDistanceButtonId)),
                                                   wc.hInstance,
                                                   NULL);
    assert(g_hApplyDepthBiasDistanceButton != NULL);
    ApplyToolDialogFont(g_hApplyDepthBiasDistanceButton);

    g_hDepthScaledSampleDistanceCheckbox = CreateWindow(_T("BUTTON"),
                                                        _T("Scale sample dist by depth"),
                                                        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                                        20,
                                                        430,
                                                        220,
                                                        24,
                                                        g_hToolDialog,
                                                        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogDepthScaledSampleDistanceCheckboxId)),
                                                        wc.hInstance,
                                                        NULL);
    assert(g_hDepthScaledSampleDistanceCheckbox != NULL);
    ApplyToolDialogFont(g_hDepthScaledSampleDistanceCheckbox);
    if (g_bDepthScaledSampleDistance)
    {
        SendMessage(g_hDepthScaledSampleDistanceCheckbox, BM_SETCHECK, BST_CHECKED, 0);
    }
    else
    {
        SendMessage(g_hDepthScaledSampleDistanceCheckbox, BM_SETCHECK, BST_UNCHECKED, 0);
    }

    g_hAutoScaleSsaoByCenterCheckbox = CreateWindow(_T("BUTTON"),
                                                    _T("Auto scale SSAO by center depth"),
                                                    WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                                    20,
                                                    462,
                                                    240,
                                                    24,
                                                    g_hToolDialog,
                                                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogAutoScaleSsaoByCenterCheckboxId)),
                                                    wc.hInstance,
                                                    NULL);
    assert(g_hAutoScaleSsaoByCenterCheckbox != NULL);
    ApplyToolDialogFont(g_hAutoScaleSsaoByCenterCheckbox);
    if (g_bAutoScaleSsaoByCenterDepth)
    {
        SendMessage(g_hAutoScaleSsaoByCenterCheckbox, BM_SETCHECK, BST_CHECKED, 0);
    }
    else
    {
        SendMessage(g_hAutoScaleSsaoByCenterCheckbox, BM_SETCHECK, BST_UNCHECKED, 0);
    }

    g_hSmoothAutoSsaoCheckbox = CreateWindow(_T("BUTTON"),
                                             _T("Smooth auto SSAO change (0.5s)"),
                                             WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                             20,
                                             494,
                                             240,
                                             24,
                                             g_hToolDialog,
                                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogSmoothAutoSsaoCheckboxId)),
                                             wc.hInstance,
                                             NULL);
    assert(g_hSmoothAutoSsaoCheckbox != NULL);
    ApplyToolDialogFont(g_hSmoothAutoSsaoCheckbox);
    if (g_bSmoothAutoSsaoByCenterDepth)
    {
        SendMessage(g_hSmoothAutoSsaoCheckbox, BM_SETCHECK, BST_CHECKED, 0);
    }
    else
    {
        SendMessage(g_hSmoothAutoSsaoCheckbox, BM_SETCHECK, BST_UNCHECKED, 0);
    }

    g_hEnableSsaoBlurCheckbox = CreateWindow(_T("BUTTON"),
                                             _T("Enable SSAO blur"),
                                             WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                             20,
                                             526,
                                             180,
                                             24,
                                             g_hToolDialog,
                                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogEnableSsaoBlurCheckboxId)),
                                             wc.hInstance,
                                             NULL);
    assert(g_hEnableSsaoBlurCheckbox != NULL);
    ApplyToolDialogFont(g_hEnableSsaoBlurCheckbox);
    if (g_bEnableSsaoBlur)
    {
        SendMessage(g_hEnableSsaoBlurCheckbox, BM_SETCHECK, BST_CHECKED, 0);
    }
    else
    {
        SendMessage(g_hEnableSsaoBlurCheckbox, BM_SETCHECK, BST_UNCHECKED, 0);
    }

    g_hSsaoBlur5x5Radio = CreateWindow(_T("BUTTON"),
                                       _T("5x5 blur"),
                                       WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
                                       40,
                                       554,
                                       120,
                                       22,
                                       g_hToolDialog,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogSsaoBlur5x5RadioId)),
                                       wc.hInstance,
                                       NULL);
    assert(g_hSsaoBlur5x5Radio != NULL);
    ApplyToolDialogFont(g_hSsaoBlur5x5Radio);

    g_hSsaoBlur11x11Radio = CreateWindow(_T("BUTTON"),
                                         _T("11x11 blur"),
                                         WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                                         170,
                                         554,
                                         120,
                                         22,
                                         g_hToolDialog,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogSsaoBlur11x11RadioId)),
                                         wc.hInstance,
                                         NULL);
    assert(g_hSsaoBlur11x11Radio != NULL);
    ApplyToolDialogFont(g_hSsaoBlur11x11Radio);

    g_hSsaoBlur21x21Radio = CreateWindow(_T("BUTTON"),
                                         _T("21x21 blur"),
                                         WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
                                         40,
                                         578,
                                         120,
                                         22,
                                         g_hToolDialog,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogSsaoBlur21x21RadioId)),
                                         wc.hInstance,
                                         NULL);
    assert(g_hSsaoBlur21x21Radio != NULL);
    ApplyToolDialogFont(g_hSsaoBlur21x21Radio);
    if (g_ssaoBlurKernelSize >= 21)
    {
        SendMessage(g_hSsaoBlur5x5Radio, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessage(g_hSsaoBlur11x11Radio, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessage(g_hSsaoBlur21x21Radio, BM_SETCHECK, BST_CHECKED, 0);
    }
    else if (g_ssaoBlurKernelSize >= 11)
    {
        SendMessage(g_hSsaoBlur5x5Radio, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessage(g_hSsaoBlur11x11Radio, BM_SETCHECK, BST_CHECKED, 0);
        SendMessage(g_hSsaoBlur21x21Radio, BM_SETCHECK, BST_UNCHECKED, 0);
    }
    else
    {
        SendMessage(g_hSsaoBlur5x5Radio, BM_SETCHECK, BST_CHECKED, 0);
        SendMessage(g_hSsaoBlur11x11Radio, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessage(g_hSsaoBlur21x21Radio, BM_SETCHECK, BST_UNCHECKED, 0);
    }

    g_hFixedSsaoSampleDistanceCheckbox = CreateWindow(_T("BUTTON"),
                                                      _T("Use fixed SSAO sample dist"),
                                                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                                      20,
                                                      608,
                                                      220,
                                                      24,
                                                      g_hToolDialog,
                                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogFixedSsaoSampleDistanceCheckboxId)),
                                                      wc.hInstance,
                                                      NULL);
    assert(g_hFixedSsaoSampleDistanceCheckbox != NULL);
    ApplyToolDialogFont(g_hFixedSsaoSampleDistanceCheckbox);
    if (g_bUseFixedSsaoSampleDistance)
    {
        SendMessage(g_hFixedSsaoSampleDistanceCheckbox, BM_SETCHECK, BST_CHECKED, 0);
    }
    else
    {
        SendMessage(g_hFixedSsaoSampleDistanceCheckbox, BM_SETCHECK, BST_UNCHECKED, 0);
    }

    HWND hShadowStrengthLabel = CreateWindow(_T("STATIC"),
                                             _T("Shadow strength:"),
                                             WS_CHILD | WS_VISIBLE,
                                             20,
                                             640,
                                             130,
                                             20,
                                             g_hToolDialog,
                                             NULL,
                                             wc.hInstance,
                                             NULL);
    ApplyToolDialogFont(hShadowStrengthLabel);

    g_hShadowStrengthEdit = CreateWindow(_T("EDIT"),
                                         _T("1.00"),
                                         WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                         160,
                                         636,
                                         60,
                                         24,
                                         g_hToolDialog,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogShadowStrengthEditId)),
                                         wc.hInstance,
                                         NULL);
    assert(g_hShadowStrengthEdit != NULL);
    ApplyToolDialogFont(g_hShadowStrengthEdit);

    g_hApplyShadowStrengthButton = CreateWindow(_T("BUTTON"),
                                                _T("Apply"),
                                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                                230,
                                                634,
                                                60,
                                                28,
                                                g_hToolDialog,
                                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogApplyShadowStrengthButtonId)),
                                                wc.hInstance,
                                                NULL);
    assert(g_hApplyShadowStrengthButton != NULL);
    ApplyToolDialogFont(g_hApplyShadowStrengthButton);

    g_hSaturateShadowCheckbox = CreateWindow(_T("BUTTON"),
                                             _T("Boost saturation in shadow"),
                                             WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                             20,
                                             668,
                                             240,
                                             24,
                                             g_hToolDialog,
                                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogSaturateShadowCheckboxId)),
                                             wc.hInstance,
                                             NULL);
    assert(g_hSaturateShadowCheckbox != NULL);
    ApplyToolDialogFont(g_hSaturateShadowCheckbox);
    if (g_bUseShadowSaturation)
    {
        SendMessage(g_hSaturateShadowCheckbox, BM_SETCHECK, BST_CHECKED, 0);
    }
    else
    {
        SendMessage(g_hSaturateShadowCheckbox, BM_SETCHECK, BST_UNCHECKED, 0);
    }

    HWND hShadowSaturationStrengthLabel = CreateWindow(_T("STATIC"),
                                                       _T("Shadow saturation:"),
                                                       WS_CHILD | WS_VISIBLE,
                                                       20,
                                                       700,
                                                       130,
                                                       20,
                                                       g_hToolDialog,
                                                       NULL,
                                                       wc.hInstance,
                                                       NULL);
    ApplyToolDialogFont(hShadowSaturationStrengthLabel);

    g_hShadowSaturationStrengthEdit = CreateWindow(_T("EDIT"),
                                                   _T("1.00"),
                                                   WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                                   160,
                                                   696,
                                                   60,
                                                   24,
                                                   g_hToolDialog,
                                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogShadowSaturationStrengthEditId)),
                                                   wc.hInstance,
                                                   NULL);
    assert(g_hShadowSaturationStrengthEdit != NULL);
    ApplyToolDialogFont(g_hShadowSaturationStrengthEdit);

    g_hApplyShadowSaturationStrengthButton = CreateWindow(_T("BUTTON"),
                                                          _T("Apply"),
                                                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                                          230,
                                                          694,
                                                          60,
                                                          28,
                                                          g_hToolDialog,
                                                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogApplyShadowSaturationStrengthButtonId)),
                                                          wc.hInstance,
                                                          NULL);
    assert(g_hApplyShadowSaturationStrengthButton != NULL);
    ApplyToolDialogFont(g_hApplyShadowSaturationStrengthButton);

    g_hEnableThicknessCapCheckbox = CreateWindow(_T("BUTTON"),
                                                 _T("Enable thickness cap"),
                                                 WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                                 20,
                                                 740,
                                                 180,
                                                 24,
                                                 g_hToolDialog,
                                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogEnableThicknessCapCheckboxId)),
                                                 wc.hInstance,
                                                 NULL);
    assert(g_hEnableThicknessCapCheckbox != NULL);
    ApplyToolDialogFont(g_hEnableThicknessCapCheckbox);
    if (g_bEnableThicknessCap)
    {
        SendMessage(g_hEnableThicknessCapCheckbox, BM_SETCHECK, BST_CHECKED, 0);
    }
    else
    {
        SendMessage(g_hEnableThicknessCapCheckbox, BM_SETCHECK, BST_UNCHECKED, 0);
    }

    HWND hThicknessCapLabel = CreateWindow(_T("STATIC"),
                                           _T("Thickness cap (m):"),
                                           WS_CHILD | WS_VISIBLE,
                                           20,
                                           772,
                                           130,
                                           20,
                                           g_hToolDialog,
                                           NULL,
                                           wc.hInstance,
                                           NULL);
    ApplyToolDialogFont(hThicknessCapLabel);

    g_hThicknessCapEdit = CreateWindow(_T("EDIT"),
                                       _T("1.00"),
                                       WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                                       160,
                                       768,
                                       60,
                                       24,
                                       g_hToolDialog,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogThicknessCapEditId)),
                                       wc.hInstance,
                                       NULL);
    assert(g_hThicknessCapEdit != NULL);
    ApplyToolDialogFont(g_hThicknessCapEdit);

    g_hApplyThicknessCapButton = CreateWindow(_T("BUTTON"),
                                              _T("Apply"),
                                              WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                              230,
                                              766,
                                              60,
                                              28,
                                              g_hToolDialog,
                                              reinterpret_cast<HMENU>(static_cast<INT_PTR>(kToolDialogApplyThicknessCapButtonId)),
                                              wc.hInstance,
                                              NULL);
    assert(g_hApplyThicknessCapButton != NULL);
    ApplyToolDialogFont(g_hApplyThicknessCapButton);

    TCHAR depthRangeText[64] = { };
    _stprintf_s(depthRangeText, _T("%.1f"), g_ssaoDepthRange);
    SetWindowText(g_hSsaoDepthRangeEdit, depthRangeText);

    TCHAR sampleDistanceMetersText[64] = { };
    _stprintf_s(sampleDistanceMetersText, _T("%.2f"), g_simpleSsaoSampleDistanceMeters);
    SetWindowText(g_hSsaoSampleEdit, sampleDistanceMetersText);

    TCHAR sampleCountText[64] = { };
    _stprintf_s(sampleCountText, _T("%d"), g_simpleSsaoSampleCount);
    SetWindowText(g_hSampleCountEdit, sampleCountText);

    TCHAR shadowStrengthText[64] = { };
    _stprintf_s(shadowStrengthText, _T("%.2f"), g_shadowStrength);
    SetWindowText(g_hShadowStrengthEdit, shadowStrengthText);

    TCHAR shadowSaturationStrengthText[64] = { };
    _stprintf_s(shadowSaturationStrengthText, _T("%.2f"), g_shadowSaturationStrength);
    SetWindowText(g_hShadowSaturationStrengthEdit, shadowSaturationStrengthText);

    TCHAR normalBiasScaleText[64] = { };
    _stprintf_s(normalBiasScaleText, _T("%.5f"), g_targetNormalBiasScale);
    SetWindowText(g_hNormalBiasScaleEdit, normalBiasScaleText);

    TCHAR depthBiasScaleText[64] = { };
    _stprintf_s(depthBiasScaleText, _T("%.5f"), g_targetDepthBiasScale);
    SetWindowText(g_hDepthBiasScaleEdit, depthBiasScaleText);

    TCHAR depthCompareDistanceText[64] = { };
    _stprintf_s(depthCompareDistanceText, _T("%.5f"), g_depthCompareDistance);
    SetWindowText(g_hDepthCompareDistanceEdit, depthCompareDistanceText);

    TCHAR depthBiasDistanceText[64] = { };
    _stprintf_s(depthBiasDistanceText, _T("%.5f"), g_sampleDepthBiasDistance);
    SetWindowText(g_hDepthBiasDistanceEdit, depthBiasDistanceText);

    TCHAR thicknessCapText[64] = { };
    _stprintf_s(thicknessCapText, _T("%.2f"), g_thicknessCapMeters);
    SetWindowText(g_hThicknessCapEdit, thicknessCapText);
}

// ダイアログの表示/非表示を切り替える。
void ToggleToolDialog()
{
    CreateToolDialog();

    const bool showDialog = !IsWindowVisible(g_hToolDialog);
    if (showDialog)
    {
        ShowWindow(g_hToolDialog, SW_SHOW);
    }
    else
    {
        ShowWindow(g_hToolDialog, SW_HIDE);
    }
    if (showDialog)
    {
        SetMouseCursorVisible(true);
        SetForegroundWindow(g_hToolDialog);
    }
}

// .x メッシュを読み込み、現在視線の先へ配置する。
void OpenMeshFileDialog()
{
    OPENFILENAME ofn = { };
    TCHAR filePath[MAX_PATH] = { };

    ofn.lStructSize = sizeof(ofn);
    if (g_hToolDialog != NULL)
    {
        ofn.hwndOwner = g_hToolDialog;
    }
    else
    {
        ofn.hwndOwner = g_hWnd;
    }
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

// ダイアログ内コントロールのイベント処理。
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
            float sampleDistanceMeters = g_simpleSsaoSampleDistanceMeters;
            if (_stscanf_s(buffer, _T("%f"), &sampleDistanceMeters) == 1)
            {
                if (sampleDistanceMeters < 0.0f)
                {
                    sampleDistanceMeters = 0.0f;
                }
                g_simpleSsaoSampleDistanceMeters = sampleDistanceMeters;

                TCHAR normalizedText[64] = { };
                _stprintf_s(normalizedText, _T("%.2f"), g_simpleSsaoSampleDistanceMeters);
                SetWindowText(g_hSsaoSampleEdit, normalizedText);
            }
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogApplySampleCountButtonId)
        {
            TCHAR buffer[64] = { };
            GetWindowText(g_hSampleCountEdit, buffer, _countof(buffer));
            int sampleCount = g_simpleSsaoSampleCount;
            if (_stscanf_s(buffer, _T("%d"), &sampleCount) == 1)
            {
                if (sampleCount < 1)
                {
                    sampleCount = 1;
                }
                if (sampleCount > 128)
                {
                    sampleCount = 128;
                }
                g_simpleSsaoSampleCount = sampleCount;

                TCHAR normalizedText[64] = { };
                _stprintf_s(normalizedText, _T("%d"), g_simpleSsaoSampleCount);
                SetWindowText(g_hSampleCountEdit, normalizedText);
            }
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogApplySsaoDepthRangeButtonId)
        {
            TCHAR buffer[64] = { };
            GetWindowText(g_hSsaoDepthRangeEdit, buffer, _countof(buffer));
            float ssaoDepthRange = g_ssaoDepthRange;
            if (_stscanf_s(buffer, _T("%f"), &ssaoDepthRange) == 1)
            {
                if (ssaoDepthRange < kMinSsaoDepthRange)
                {
                    ssaoDepthRange = kMinSsaoDepthRange;
                }
                if (ssaoDepthRange > kCameraFarPlane)
                {
                    ssaoDepthRange = kCameraFarPlane;
                }
                g_ssaoDepthRange = ssaoDepthRange;

                TCHAR normalizedText[64] = { };
                _stprintf_s(normalizedText, _T("%.1f"), g_ssaoDepthRange);
                SetWindowText(g_hSsaoDepthRangeEdit, normalizedText);
            }
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogUseThicknessCheckboxId)
        {
            g_bUseThicknessForSsao = (SendMessage(g_hUseThicknessCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogRemoteDesktopCheckboxId)
        {
            g_bRemoteDesktopCameraMode = (SendMessage(g_hRemoteDesktopCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
            ResetMouseLookTracking();
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogAllowStraightUpDownCheckboxId)
        {
            g_bAllowStraightUpDown = (SendMessage(g_hAllowStraightUpDownCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogDepthScaledSampleDistanceCheckboxId)
        {
            g_bDepthScaledSampleDistance = (SendMessage(g_hDepthScaledSampleDistanceCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogAutoScaleSsaoByCenterCheckboxId)
        {
            g_bAutoScaleSsaoByCenterDepth = (SendMessage(g_hAutoScaleSsaoByCenterCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
            if (g_bAutoScaleSsaoByCenterDepth)
            {
                g_autoSsaoSampleDistanceMeters = g_simpleSsaoSampleDistanceMeters;
                g_autoSsaoDepthRange = g_ssaoDepthRange;
                g_targetAutoSsaoSampleDistanceMeters = g_autoSsaoSampleDistanceMeters;
                g_targetAutoSsaoDepthRange = g_autoSsaoDepthRange;
            }
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogSmoothAutoSsaoCheckboxId)
        {
            g_bSmoothAutoSsaoByCenterDepth = (SendMessage(g_hSmoothAutoSsaoCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
            g_targetAutoSsaoSampleDistanceMeters = g_autoSsaoSampleDistanceMeters;
            g_targetAutoSsaoDepthRange = g_autoSsaoDepthRange;
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogEnableSsaoBlurCheckboxId)
        {
            g_bEnableSsaoBlur = (SendMessage(g_hEnableSsaoBlurCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogSsaoBlur5x5RadioId)
        {
            g_ssaoBlurKernelSize = 5;
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogSsaoBlur11x11RadioId)
        {
            g_ssaoBlurKernelSize = 11;
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogSsaoBlur21x21RadioId)
        {
            g_ssaoBlurKernelSize = 21;
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogFixedSsaoSampleDistanceCheckboxId)
        {
            g_bUseFixedSsaoSampleDistance = (SendMessage(g_hFixedSsaoSampleDistanceCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogApplyShadowStrengthButtonId)
        {
            TCHAR buffer[64] = { };
            GetWindowText(g_hShadowStrengthEdit, buffer, _countof(buffer));
            float shadowStrength = g_shadowStrength;
            if (_stscanf_s(buffer, _T("%f"), &shadowStrength) == 1)
            {
                if (shadowStrength < 0.0f)
                {
                    shadowStrength = 0.0f;
                }
                if (shadowStrength > 5.0f)
                {
                    shadowStrength = 5.0f;
                }
                g_shadowStrength = shadowStrength;

                TCHAR normalizedText[64] = { };
                _stprintf_s(normalizedText, _T("%.2f"), g_shadowStrength);
                SetWindowText(g_hShadowStrengthEdit, normalizedText);
            }
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogSaturateShadowCheckboxId)
        {
            g_bUseShadowSaturation = (SendMessage(g_hSaturateShadowCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogApplyShadowSaturationStrengthButtonId)
        {
            TCHAR buffer[64] = { };
            GetWindowText(g_hShadowSaturationStrengthEdit, buffer, _countof(buffer));
            float shadowSaturationStrength = g_shadowSaturationStrength;
            if (_stscanf_s(buffer, _T("%f"), &shadowSaturationStrength) == 1)
            {
                if (shadowSaturationStrength < 0.0f)
                {
                    shadowSaturationStrength = 0.0f;
                }
                if (shadowSaturationStrength > 5.0f)
                {
                    shadowSaturationStrength = 5.0f;
                }
                g_shadowSaturationStrength = shadowSaturationStrength;

                TCHAR normalizedText[64] = { };
                _stprintf_s(normalizedText, _T("%.2f"), g_shadowSaturationStrength);
                SetWindowText(g_hShadowSaturationStrengthEdit, normalizedText);
            }
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogEnableThicknessCapCheckboxId)
        {
            g_bEnableThicknessCap = (SendMessage(g_hEnableThicknessCapCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED);
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogApplyThicknessScaleButtonId)
        {
            TCHAR buffer[64] = { };
            GetWindowText(g_hThicknessScaleEdit, buffer, _countof(buffer));
            float thicknessScale = g_thicknessScale;
            if (_stscanf_s(buffer, _T("%f"), &thicknessScale) == 1)
            {
                if (thicknessScale < 0.0f)
                {
                    thicknessScale = 0.0f;
                }
                g_thicknessScale = thicknessScale;

                TCHAR normalizedText[64] = { };
                _stprintf_s(normalizedText, _T("%.5f"), g_thicknessScale);
                SetWindowText(g_hThicknessScaleEdit, normalizedText);
            }
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogApplyNormalBiasScaleButtonId)
        {
            TCHAR buffer[64] = { };
            GetWindowText(g_hNormalBiasScaleEdit, buffer, _countof(buffer));
            float normalBiasScale = g_targetNormalBiasScale;
            if (_stscanf_s(buffer, _T("%f"), &normalBiasScale) == 1)
            {
                if (normalBiasScale < 0.0f)
                {
                    normalBiasScale = 0.0f;
                }
                g_targetNormalBiasScale = normalBiasScale;

                TCHAR normalizedText[64] = { };
                _stprintf_s(normalizedText, _T("%.5f"), g_targetNormalBiasScale);
                SetWindowText(g_hNormalBiasScaleEdit, normalizedText);
            }
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogApplyDepthBiasScaleButtonId)
        {
            TCHAR buffer[64] = { };
            GetWindowText(g_hDepthBiasScaleEdit, buffer, _countof(buffer));
            float depthBiasScale = g_targetDepthBiasScale;
            if (_stscanf_s(buffer, _T("%f"), &depthBiasScale) == 1)
            {
                if (depthBiasScale < 0.0f)
                {
                    depthBiasScale = 0.0f;
                }
                g_targetDepthBiasScale = depthBiasScale;

                TCHAR normalizedText[64] = { };
                _stprintf_s(normalizedText, _T("%.5f"), g_targetDepthBiasScale);
                SetWindowText(g_hDepthBiasScaleEdit, normalizedText);
            }
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogApplyDepthCompareDistanceButtonId)
        {
            TCHAR buffer[64] = { };
            GetWindowText(g_hDepthCompareDistanceEdit, buffer, _countof(buffer));
            float depthCompareDistance = g_depthCompareDistance;
            if (_stscanf_s(buffer, _T("%f"), &depthCompareDistance) == 1)
            {
                if (depthCompareDistance < 0.0f)
                {
                    depthCompareDistance = 0.0f;
                }
                g_depthCompareDistance = depthCompareDistance;

                TCHAR normalizedText[64] = { };
                _stprintf_s(normalizedText, _T("%.5f"), g_depthCompareDistance);
                SetWindowText(g_hDepthCompareDistanceEdit, normalizedText);
            }
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogApplyDepthBiasDistanceButtonId)
        {
            TCHAR buffer[64] = { };
            GetWindowText(g_hDepthBiasDistanceEdit, buffer, _countof(buffer));
            float depthBiasDistance = g_sampleDepthBiasDistance;
            if (_stscanf_s(buffer, _T("%f"), &depthBiasDistance) == 1)
            {
                if (depthBiasDistance < 0.0f)
                {
                    depthBiasDistance = 0.0f;
                }
                g_sampleDepthBiasDistance = depthBiasDistance;

                TCHAR normalizedText[64] = { };
                _stprintf_s(normalizedText, _T("%.5f"), g_sampleDepthBiasDistance);
                SetWindowText(g_hDepthBiasDistanceEdit, normalizedText);
            }
            return 0;
        }
        if (LOWORD(wParam) == kToolDialogApplyThicknessCapButtonId)
        {
            TCHAR buffer[64] = { };
            GetWindowText(g_hThicknessCapEdit, buffer, _countof(buffer));
            float thicknessCapMeters = g_thicknessCapMeters;
            if (_stscanf_s(buffer, _T("%f"), &thicknessCapMeters) == 1)
            {
                if (thicknessCapMeters < 0.0f)
                {
                    thicknessCapMeters = 0.0f;
                }
                g_thicknessCapMeters = thicknessCapMeters;

                TCHAR normalizedText[64] = { };
                _stprintf_s(normalizedText, _T("%.2f"), g_thicknessCapMeters);
                SetWindowText(g_hThicknessCapEdit, normalizedText);
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
