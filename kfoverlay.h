#pragma once

#include <Windows.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "kfesp.h"
#include "kftargeting.h"
#include "kfaimbot.h"


#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")


namespace KFOverlay
{
    // ============================================================
    // STATE
    // ============================================================

    inline std::atomic_bool
        gStopRequested{ false };


    inline std::atomic_bool
        gRunning{ false };


    inline std::atomic_bool
        gShowHeadMarker{ true };


    inline HANDLE
        gThread = nullptr;


    inline HWND
        gOverlayWindow = nullptr;


    // ============================================================
    // BACK BUFFER
    // ============================================================

    struct BackBuffer
    {
        HDC dc = nullptr;

        HBITMAP bitmap = nullptr;

        HGDIOBJ oldBitmap = nullptr;

        int width = 0;

        int height = 0;
    };


    inline BackBuffer
        gBackBuffer;


    // ============================================================
    // GDI RESOURCES
    // ============================================================

    struct RenderResources
    {
        HPEN enemyPen = nullptr;

        HPEN targetPen = nullptr;

        HPEN aimFovPen = nullptr;

        HPEN headPen = nullptr;


        HBRUSH healthBackground = nullptr;

        HBRUSH healthHigh = nullptr;

        HBRUSH healthMedium = nullptr;

        HBRUSH healthLow = nullptr;

        HBRUSH healthFrame = nullptr;


        HFONT font = nullptr;
    };


    inline RenderResources
        gResources;


    // ============================================================
    // CREATE RESOURCES
    // ============================================================

    inline bool CreateRenderResources()
    {
        gResources.enemyPen =
            CreatePen(
                PS_SOLID,
                2,
                RGB(255, 55, 55)
            );


        gResources.targetPen =
            CreatePen(
                PS_SOLID,
                2,
                RGB(255, 210, 35)
            );


        // Rojo tenue para el circulo del AimFOV.
        gResources.aimFovPen =
            CreatePen(
                PS_SOLID,
                1,
                RGB(170, 45, 45)
            );


        gResources.headPen =
            CreatePen(
                PS_SOLID,
                1,
                RGB(255, 220, 70)
            );


        gResources.healthBackground =
            CreateSolidBrush(
                RGB(32, 32, 32)
            );


        gResources.healthHigh =
            CreateSolidBrush(
                RGB(70, 220, 90)
            );


        gResources.healthMedium =
            CreateSolidBrush(
                RGB(230, 190, 50)
            );


        gResources.healthLow =
            CreateSolidBrush(
                RGB(235, 65, 65)
            );


        // No negro puro:
        // RGB(0,0,0) es transparente.
        gResources.healthFrame =
            CreateSolidBrush(
                RGB(2, 2, 2)
            );


        gResources.font =
            CreateFontW(
                -14,
                0,
                0,
                0,
                FW_SEMIBOLD,
                FALSE,
                FALSE,
                FALSE,
                DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                ANTIALIASED_QUALITY,
                DEFAULT_PITCH |
                FF_DONTCARE,
                L"Segoe UI"
            );


        return
            gResources.enemyPen != nullptr &&
            gResources.targetPen != nullptr &&
            gResources.aimFovPen != nullptr &&
            gResources.headPen != nullptr;
    }


    // ============================================================
    // DESTROY RESOURCES
    // ============================================================

    inline void DestroyRenderResources()
    {
        if (gResources.enemyPen != nullptr)
        {
            DeleteObject(
                gResources.enemyPen
            );
        }


        if (gResources.targetPen != nullptr)
        {
            DeleteObject(
                gResources.targetPen
            );
        }


        if (gResources.aimFovPen != nullptr)
        {
            DeleteObject(
                gResources.aimFovPen
            );
        }


        if (gResources.headPen != nullptr)
        {
            DeleteObject(
                gResources.headPen
            );
        }


        if (gResources.healthBackground != nullptr)
        {
            DeleteObject(
                gResources.healthBackground
            );
        }


        if (gResources.healthHigh != nullptr)
        {
            DeleteObject(
                gResources.healthHigh
            );
        }


        if (gResources.healthMedium != nullptr)
        {
            DeleteObject(
                gResources.healthMedium
            );
        }


        if (gResources.healthLow != nullptr)
        {
            DeleteObject(
                gResources.healthLow
            );
        }


        if (gResources.healthFrame != nullptr)
        {
            DeleteObject(
                gResources.healthFrame
            );
        }


        if (gResources.font != nullptr)
        {
            DeleteObject(
                gResources.font
            );
        }


        gResources = {};
    }


    // ============================================================
    // BACKBUFFER
    // ============================================================

    inline void DestroyBackBuffer()
    {
        if (gBackBuffer.dc != nullptr)
        {
            if (
                gBackBuffer.oldBitmap !=
                nullptr
                )
            {
                SelectObject(
                    gBackBuffer.dc,
                    gBackBuffer.oldBitmap
                );
            }


            if (
                gBackBuffer.bitmap !=
                nullptr
                )
            {
                DeleteObject(
                    gBackBuffer.bitmap
                );
            }


            DeleteDC(
                gBackBuffer.dc
            );
        }


        gBackBuffer = {};
    }


    inline bool EnsureBackBuffer(
        HDC referenceDC,
        int width,
        int height
    )
    {
        if (
            gBackBuffer.dc != nullptr &&
            gBackBuffer.width == width &&
            gBackBuffer.height == height
            )
        {
            return true;
        }


        DestroyBackBuffer();


        gBackBuffer.dc =
            CreateCompatibleDC(
                referenceDC
            );


        if (gBackBuffer.dc == nullptr)
        {
            return false;
        }


        gBackBuffer.bitmap =
            CreateCompatibleBitmap(
                referenceDC,
                width,
                height
            );


        if (
            gBackBuffer.bitmap ==
            nullptr
            )
        {
            DestroyBackBuffer();

            return false;
        }


        gBackBuffer.oldBitmap =
            SelectObject(
                gBackBuffer.dc,
                gBackBuffer.bitmap
            );


        gBackBuffer.width =
            width;


        gBackBuffer.height =
            height;


        return true;
    }


    // ============================================================
    // STRING
    // ============================================================

    inline std::wstring ToWide(
        const std::string& text
    )
    {
        if (text.empty())
        {
            return {};
        }


        const int count =
            MultiByteToWideChar(
                CP_UTF8,
                0,
                text.c_str(),
                static_cast<int>(
                    text.size()
                ),
                nullptr,
                0
            );


        if (count <= 0)
        {
            return std::wstring(
                text.begin(),
                text.end()
            );
        }


        std::wstring output(
            static_cast<size_t>(
                count
            ),
            L'\0'
        );


        MultiByteToWideChar(
            CP_UTF8,
            0,
            text.c_str(),
            static_cast<int>(
                text.size()
            ),
            output.data(),
            count
        );


        return output;
    }


    // ============================================================
    // TEXT
    // ============================================================

    inline void DrawTextShadow(
        HDC hdc,
        int x,
        int y,
        const std::wstring& text,
        COLORREF color
    )
    {
        if (text.empty())
        {
            return;
        }


        // Negro puro seria transparente.
        SetTextColor(
            hdc,
            RGB(1, 1, 1)
        );


        TextOutW(
            hdc,
            x + 1,
            y + 1,
            text.c_str(),
            static_cast<int>(
                text.size()
            )
        );


        SetTextColor(
            hdc,
            color
        );


        TextOutW(
            hdc,
            x,
            y,
            text.c_str(),
            static_cast<int>(
                text.size()
            )
        );
    }


    inline void DrawCenteredText(
        HDC hdc,
        int centerX,
        int y,
        const std::wstring& text,
        COLORREF color
    )
    {
        SIZE size{};


        GetTextExtentPoint32W(
            hdc,
            text.c_str(),
            static_cast<int>(
                text.size()
            ),
            &size
        );


        DrawTextShadow(
            hdc,
            centerX -
                size.cx / 2,
            y,
            text,
            color
        );
    }


    // ============================================================
    // AIM FOV CIRCLE
    // ============================================================

    inline void DrawAimFov(
        HDC hdc,
        const KFTargeting::Selection& selection
    )
    {
        const int centerX =
            static_cast<int>(
                std::lround(
                    selection.centerX
                )
            );


        const int centerY =
            static_cast<int>(
                std::lround(
                    selection.centerY
                )
            );


        const int radius =
            static_cast<int>(
                std::lround(
                    selection.radius
                )
            );


        HGDIOBJ oldPen =
            SelectObject(
                hdc,
                gResources.aimFovPen
            );


        HGDIOBJ oldBrush =
            SelectObject(
                hdc,
                GetStockObject(
                    NULL_BRUSH
                )
            );


        Ellipse(
            hdc,
            centerX - radius,
            centerY - radius,
            centerX + radius,
            centerY + radius
        );


        // Pequeno crosshair central de referencia.
        MoveToEx(
            hdc,
            centerX - 3,
            centerY,
            nullptr
        );


        LineTo(
            hdc,
            centerX + 4,
            centerY
        );


        MoveToEx(
            hdc,
            centerX,
            centerY - 3,
            nullptr
        );


        LineTo(
            hdc,
            centerX,
            centerY + 4
        );


        SelectObject(
            hdc,
            oldBrush
        );


        SelectObject(
            hdc,
            oldPen
        );
    }


    // ============================================================
    // HEALTH
    // ============================================================

    inline void DrawHealthBar(
        HDC hdc,
        const KFESP::Entry& entry
    )
    {
        if (
            !entry.healthReliable ||
            entry.maxObservedHealth <= 0
            )
        {
            return;
        }


        const int top =
            static_cast<int>(
                std::lround(
                    entry.boxTop
                )
            );


        const int bottom =
            static_cast<int>(
                std::lround(
                    entry.boxBottom
                )
            );


        int barHeight =
            bottom -
            top;


        if (barHeight < 1)
        {
            barHeight = 1;
        }


        const int left =
            static_cast<int>(
                std::lround(
                    entry.boxLeft
                )
            ) -
            9;


        RECT background
        {
            left,
            top,
            left + 5,
            bottom
        };


        FillRect(
            hdc,
            &background,
            gResources.healthBackground
        );


        HBRUSH fillBrush =
            gResources.healthHigh;


        if (
            entry.healthRatio <=
            0.30f
            )
        {
            fillBrush =
                gResources.healthLow;
        }
        else if (
            entry.healthRatio <=
            0.60f
            )
        {
            fillBrush =
                gResources.healthMedium;
        }


        const int filledHeight =
            static_cast<int>(
                std::lround(
                    static_cast<float>(
                        barHeight
                    ) *
                    entry.healthRatio
                )
            );


        RECT healthRect
        {
            left + 1,
            bottom - filledHeight,
            left + 4,
            bottom
        };


        FillRect(
            hdc,
            &healthRect,
            fillBrush
        );


        FrameRect(
            hdc,
            &background,
            gResources.healthFrame
        );
    }


    // ============================================================
    // TARGET LINE
    // ============================================================

    inline void DrawTargetLine(
        HDC hdc,
        const KFESP::Entry& entry,
        const KFTargeting::Selection& selection
    )
    {
        if (!selection.found)
        {
            return;
        }


        const int centerX =
            static_cast<int>(
                std::lround(
                    selection.centerX
                )
            );


        const int centerY =
            static_cast<int>(
                std::lround(
                    selection.centerY
                )
            );


        const int targetX =
            static_cast<int>(
                std::lround(
                    entry.headScreen.x
                )
            );


        const int targetY =
            static_cast<int>(
                std::lround(
                    entry.headScreen.y
                )
            );


        HGDIOBJ oldPen =
            SelectObject(
                hdc,
                gResources.targetPen
            );


        MoveToEx(
            hdc,
            centerX,
            centerY,
            nullptr
        );


        LineTo(
            hdc,
            targetX,
            targetY
        );


        SelectObject(
            hdc,
            oldPen
        );
    }


    // ============================================================
    // DRAW ENEMY
    // ============================================================

    inline void DrawEnemy(
        HDC hdc,
        const KFESP::Entry& entry,
        bool isTarget
    )
    {
        if (
            !entry.onScreen ||
            !entry.headProjected ||
            !entry.feetProjected
            )
        {
            return;
        }


        const int left =
            static_cast<int>(
                std::lround(
                    entry.boxLeft
                )
            );


        const int top =
            static_cast<int>(
                std::lround(
                    entry.boxTop
                )
            );


        const int right =
            static_cast<int>(
                std::lround(
                    entry.boxRight
                )
            );


        const int bottom =
            static_cast<int>(
                std::lround(
                    entry.boxBottom
                )
            );


        const int centerX =
            (
                left +
                right
            ) /
            2;


        HPEN boxPen =
            isTarget
                ?
                gResources.targetPen
                :
                gResources.enemyPen;


        HGDIOBJ oldPen =
            SelectObject(
                hdc,
                boxPen
            );


        HGDIOBJ oldBrush =
            SelectObject(
                hdc,
                GetStockObject(
                    NULL_BRUSH
                )
            );


        Rectangle(
            hdc,
            left,
            top,
            right,
            bottom
        );


        // --------------------------------------------------------
        // HEAD MARKER
        // --------------------------------------------------------

        if (gShowHeadMarker.load())
        {
            SelectObject(
                hdc,
                isTarget
                    ?
                    gResources.targetPen
                    :
                    gResources.headPen
            );


            const int headX =
                static_cast<int>(
                    std::lround(
                        entry.headScreen.x
                    )
                );


            const int headY =
                static_cast<int>(
                    std::lround(
                        entry.headScreen.y
                    )
                );


            MoveToEx(
                hdc,
                headX - 4,
                headY,
                nullptr
            );


            LineTo(
                hdc,
                headX + 5,
                headY
            );


            MoveToEx(
                hdc,
                headX,
                headY - 4,
                nullptr
            );


            LineTo(
                hdc,
                headX,
                headY + 5
            );
        }


        SelectObject(
            hdc,
            oldBrush
        );


        SelectObject(
            hdc,
            oldPen
        );


        DrawHealthBar(
            hdc,
            entry
        );


        // --------------------------------------------------------
        // TOP TEXT
        // --------------------------------------------------------

        std::wostringstream topText;


        if (isTarget)
        {
            topText
                << L"[TARGET] ";
        }


        topText
            << ToWide(
                entry.displayName
            );


        if (entry.healthReliable)
        {
            topText
                << L" | "
                << entry.health
                << L" HP";
        }


        DrawCenteredText(
            hdc,
            centerX,
            top - 17,
            topText.str(),
            isTarget
                ?
                RGB(255, 220, 60)
                :
                RGB(255, 225, 225)
        );


        // --------------------------------------------------------
        // DISTANCE
        // --------------------------------------------------------

        std::wostringstream distanceText;


        distanceText
            << std::fixed
            << std::setprecision(0)
            << entry.distance
            << L" uu";


        DrawCenteredText(
            hdc,
            centerX,
            bottom + 3,
            distanceText.str(),
            RGB(230, 230, 230)
        );
    }


    // ============================================================
    // FRAME
    // ============================================================

    inline void DrawESP(
        HDC hdc,
        const RECT& clientRect
    )
    {
        const int width =
            clientRect.right -
            clientRect.left;


        const int height =
            clientRect.bottom -
            clientRect.top;


        if (
            width <= 0 ||
            height <= 0
            )
        {
            return;
        }


        // Color-key background.
        PatBlt(
            hdc,
            0,
            0,
            width,
            height,
            BLACKNESS
        );


        KFCamera::Viewport viewport;


        viewport.width =
            width;


        viewport.height =
            height;


        std::vector<KFESP::Entry> entries;


        KFCamera::Snapshot camera;


        if (!KFESP::BuildEntriesFast(
            viewport,
            entries,
            &camera
        ))
        {
            return;
        }


        const bool aimHeld =
            (
                GetAsyncKeyState('Q') &
                0x8000
            ) != 0;


        const KFTargeting::Selection selection =
            KFTargeting::FindBestTarget(
                entries,
                viewport,
                aimHeld
            );


        // ========================================================
        // AIMBOT
        //
        // Reutiliza exactamente el mismo target que vemos amarillo.
        //
        // Q HOLD:
        //      aplica Pitch/Yaw suavizado
        //
        // Q RELEASE:
        //      no modifica la camara
        // ========================================================

        if (
            aimHeld &&
            selection.found &&
            selection.entryIndex <
                entries.size()
            )
        {
            KFAimbot::ApplyAim(
                camera,
                entries[
                    selection.entryIndex
                ]
            );
        }

        SetBkMode(
            hdc,
            TRANSPARENT
        );


        HGDIOBJ oldFont =
            SelectObject(
                hdc,
                gResources.font != nullptr
                    ?
                    static_cast<HGDIOBJ>(
                        gResources.font
                    )
                    :
                    GetStockObject(
                        DEFAULT_GUI_FONT
                    )
            );


        // Primero circulo AimFOV.
        DrawAimFov(
            hdc,
            selection
        );


        // Despues boxes.
        for (
            const KFESP::Entry& entry :
            entries
            )
        {
            const bool isTarget =
                KFTargeting::IsSelected(
                    selection,
                    entry
                );


            DrawEnemy(
                hdc,
                entry,
                isTarget
            );
        }


        // Linea al target al final para que quede visible.
        if (
            selection.found &&
            selection.entryIndex <
                entries.size()
            )
        {
            DrawTargetLine(
                hdc,
                entries[
                    selection.entryIndex
                ],
                selection
            );
        }


        SelectObject(
            hdc,
            oldFont
        );
    }


    // ============================================================
    // WINDOW PROC
    // ============================================================

    inline LRESULT CALLBACK OverlayWndProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam
    )
    {
        switch (message)
        {
        case WM_ERASEBKGND:

            return 1;


        case WM_PAINT:
        {
            PAINTSTRUCT paint{};


            HDC windowDC =
                BeginPaint(
                    hwnd,
                    &paint
                );


            RECT clientRect{};


            GetClientRect(
                hwnd,
                &clientRect
            );


            const int width =
                clientRect.right -
                clientRect.left;


            const int height =
                clientRect.bottom -
                clientRect.top;


            if (
                width > 0 &&
                height > 0 &&
                EnsureBackBuffer(
                    windowDC,
                    width,
                    height
                )
                )
            {
                DrawESP(
                    gBackBuffer.dc,
                    clientRect
                );


                BitBlt(
                    windowDC,
                    0,
                    0,
                    width,
                    height,
                    gBackBuffer.dc,
                    0,
                    0,
                    SRCCOPY
                );
            }


            EndPaint(
                hwnd,
                &paint
            );


            return 0;
        }


        case WM_DESTROY:

            // Los recursos GDI pertenecen al OverlayThread.
            //
            // No los destruimos aqui para evitar dos rutas de
            // cleanup actuando sobre el mismo estado.
            PostQuitMessage(0);

            return 0;
        }


        return DefWindowProcW(
            hwnd,
            message,
            wParam,
            lParam
        );
    }


    // ============================================================
    // THREAD
    // ============================================================

    inline DWORD WINAPI OverlayThread(
        LPVOID parameter
    )
    {
        HMODULE dllModule =
            reinterpret_cast<HMODULE>(
                parameter
            );


        DPI_AWARENESS_CONTEXT oldDpi =
            SetThreadDpiAwarenessContext(
                DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
            );


        HWND gameWindow =
            KFCamera::FindGameWindow();


        if (gameWindow == nullptr)
        {
            gRunning = false;

            return 1;
        }


        constexpr wchar_t ClassName[] =
            L"KFProjectESPOverlayClass";


        WNDCLASSEXW windowClass{};


        windowClass.cbSize =
            sizeof(
                WNDCLASSEXW
            );


        windowClass.lpfnWndProc =
            OverlayWndProc;


        windowClass.hInstance =
            dllModule;


        windowClass.lpszClassName =
            ClassName;


        windowClass.hCursor =
            LoadCursorW(
                nullptr,
                IDC_ARROW
            );


        windowClass.hbrBackground =
            nullptr;


        if (!RegisterClassExW(
            &windowClass
        ))
        {
            const DWORD error =
                GetLastError();


            if (
                error !=
                ERROR_CLASS_ALREADY_EXISTS
                )
            {
                gRunning = false;

                return 2;
            }
        }


        if (!CreateRenderResources())
        {
            gRunning = false;

            return 3;
        }


        RECT gameClient{};


        GetClientRect(
            gameWindow,
            &gameClient
        );


        POINT position
        {
            0,
            0
        };


        ClientToScreen(
            gameWindow,
            &position
        );


        const int width =
            gameClient.right -
            gameClient.left;


        const int height =
            gameClient.bottom -
            gameClient.top;


        HWND overlay =
            CreateWindowExW(
                WS_EX_TOPMOST |
                WS_EX_LAYERED |
                WS_EX_TRANSPARENT |
                WS_EX_TOOLWINDOW |
                WS_EX_NOACTIVATE,

                ClassName,
                L"KF-PROJECT ESP",

                WS_POPUP,

                position.x,
                position.y,

                width,
                height,

                nullptr,
                nullptr,
                dllModule,
                nullptr
            );


        if (overlay == nullptr)
        {
            DestroyRenderResources();

            gRunning = false;

            return 4;
        }


        gOverlayWindow =
            overlay;


        SetLayeredWindowAttributes(
            overlay,
            RGB(0, 0, 0),
            0,
            LWA_COLORKEY
        );


        ShowWindow(
            overlay,
            SW_SHOWNOACTIVATE
        );


        UpdateWindow(
            overlay
        );


        gRunning =
            true;


        MSG message{};


        int previousX =
            -999999;


        int previousY =
            -999999;


        int previousWidth =
            -1;


        int previousHeight =
            -1;


        bool overlayVisible =
            true;


        // ========================================================
        // ~60 FPS
        // ========================================================

        while (
            !gStopRequested &&
            IsWindow(gameWindow)
            )
        {
            const bool shouldBeVisible =
                !IsIconic(
                    gameWindow
                ) &&
                IsWindowVisible(
                    gameWindow
                ) &&
                GetForegroundWindow() ==
                    gameWindow;


            if (
                shouldBeVisible !=
                overlayVisible
                )
            {
                ShowWindow(
                    overlay,
                    shouldBeVisible
                        ?
                        SW_SHOWNOACTIVATE
                        :
                        SW_HIDE
                );


                overlayVisible =
                    shouldBeVisible;
            }


            RECT client{};


            if (GetClientRect(
                gameWindow,
                &client
            ))
            {
                POINT clientPosition
                {
                    0,
                    0
                };


                if (ClientToScreen(
                    gameWindow,
                    &clientPosition
                ))
                {
                    const int newWidth =
                        client.right -
                        client.left;


                    const int newHeight =
                        client.bottom -
                        client.top;


                    if (
                        newWidth > 0 &&
                        newHeight > 0 &&
                        (
                            clientPosition.x !=
                                previousX ||
                            clientPosition.y !=
                                previousY ||
                            newWidth !=
                                previousWidth ||
                            newHeight !=
                                previousHeight
                        )
                        )
                    {
                        SetWindowPos(
                            overlay,
                            HWND_TOPMOST,

                            clientPosition.x,
                            clientPosition.y,

                            newWidth,
                            newHeight,

                            SWP_NOACTIVATE
                        );


                        previousX =
                            clientPosition.x;


                        previousY =
                            clientPosition.y;


                        previousWidth =
                            newWidth;


                        previousHeight =
                            newHeight;
                    }
                }
            }


            while (PeekMessageW(
                &message,
                nullptr,
                0,
                0,
                PM_REMOVE
            ))
            {
                if (
                    message.message ==
                    WM_QUIT
                    )
                {
                    gStopRequested = true;

                    break;
                }


                TranslateMessage(
                    &message
                );


                DispatchMessageW(
                    &message
                );
            }


            if (shouldBeVisible)
            {
                InvalidateRect(
                    overlay,
                    nullptr,
                    FALSE
                );


                UpdateWindow(
                    overlay
                );
            }


            Sleep(16);
        }


        if (IsWindow(overlay))
        {
            DestroyWindow(
                overlay
            );
        }


        DestroyBackBuffer();

        DestroyRenderResources();


        gOverlayWindow =
            nullptr;


        gRunning =
            false;


        if (oldDpi != nullptr)
        {
            SetThreadDpiAwarenessContext(
                oldDpi
            );
        }


        // La clase no debe quedarse registrada entre toggles.
        UnregisterClassW(
            ClassName,
            dllModule
        );


        return 0;
    }


    // ============================================================
    // START / STOP
    // ============================================================

    // ============================================================
    // THREAD LIFECYCLE
    // ============================================================

    inline void ReleaseStoppedThreadHandle()
    {
        if (gThread == nullptr)
        {
            return;
        }


        const DWORD result =
            WaitForSingleObject(
                gThread,
                0
            );


        if (result != WAIT_OBJECT_0)
        {
            return;
        }


        CloseHandle(
            gThread
        );


        gThread =
            nullptr;


        gRunning =
            false;
    }


    inline bool IsThreadAlive()
    {
        if (gThread == nullptr)
        {
            return false;
        }


        return
            WaitForSingleObject(
                gThread,
                0
            ) ==
            WAIT_TIMEOUT;
    }


    // ============================================================
    // START
    // ============================================================

    inline bool Start(
        HMODULE dllModule
    )
    {
        // Un thread que ya terminó puede seguir teniendo un HANDLE
        // válido hasta que nosotros lo cerremos.
        ReleaseStoppedThreadHandle();


        // Nunca crear dos OverlayThread simultáneos.
        if (gThread != nullptr)
        {
            return true;
        }


        gStopRequested =
            false;


        gRunning =
            false;


        // Un target anterior no debe sobrevivir a un reinicio
        // completo del overlay.
        KFTargeting::ResetLock();


        KFAimbot::ResetMotionTracking();


        KFESP::ForceRefresh();


        gThread =
            CreateThread(
                nullptr,
                0,
                OverlayThread,
                dllModule,
                0,
                nullptr
            );


        if (gThread == nullptr)
        {
            return false;
        }


        return true;
    }


    // ============================================================
    // STOP
    //
    // IMPORTANTE:
    //
    // No cerramos el HANDLE hasta confirmar que el thread terminó.
    //
    // Si el timeout ocurre mantenemos gThread intacto. Así Toggle
    // NO puede crear accidentalmente un segundo OverlayThread.
    // ============================================================

    inline bool Stop()
    {
        ReleaseStoppedThreadHandle();


        if (gThread == nullptr)
        {
            gRunning =
                false;


            return true;
        }


        gStopRequested =
            true;


        const DWORD result =
            WaitForSingleObject(
                gThread,
                5000
            );


        if (result != WAIT_OBJECT_0)
        {
            // El thread sigue siendo dueño de sus recursos.
            //
            // NO CloseHandle.
            // NO gThread = nullptr.
            //
            // De esta manera no puede arrancar una segunda copia.
            return false;
        }


        CloseHandle(
            gThread
        );


        gThread =
            nullptr;


        gRunning =
            false;


        KFTargeting::ResetLock();


        KFAimbot::ResetMotionTracking();


        KFESP::ForceRefresh();


        return true;
    }


    // ============================================================
    // TOGGLE
    // ============================================================

    inline bool Toggle(
        HMODULE dllModule
    )
    {
        ReleaseStoppedThreadHandle();


        if (gThread != nullptr)
        {
            const bool stopped =
                Stop();


            // Si no terminó dentro del timeout consideramos que
            // sigue activo y, crucialmente, NO creamos otro.
            if (!stopped)
            {
                return true;
            }


            return false;
        }


        return Start(
            dllModule
        );
    }

    // ============================================================
    // HEAD MARKER
    // ============================================================

    inline bool ToggleHeadMarker()
    {
        const bool enabled =
            !gShowHeadMarker.load();


        gShowHeadMarker =
            enabled;


        return enabled;
    }


    inline bool IsActive()
    {
        return
            gThread != nullptr;
    }
}