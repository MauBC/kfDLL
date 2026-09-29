
#pragma once

#ifdef MEMORYDLL_EXPORTS
#define MEMORYDLL_API __declspec(dllexport)
#else
#define MEMORYDLL_API __declspec(dllimport)
#endif

#include <Windows.h>
#include <cstdint>
#include <string>

// ------------------------------------------------


#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
// ----------------------


extern "C"
{
    MEMORYDLL_API const char* HelloFromDll();

    MEMORYDLL_API void ModifyValueDll(int* address);

    MEMORYDLL_API bool SetTargetVida(int nuevaVida);


}



// --------------------------------------------

LRESULT CALLBACK OverlayWndProc(
    HWND hwnd,
    UINT msg,
    WPARAM wParam,
    LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rect{};
        GetClientRect(hwnd, &rect);

        // El negro se vuelve transparente por LWA_COLORKEY.
        HBRUSH blackBrush =
            CreateSolidBrush(RGB(0, 0, 0));

        FillRect(
            hdc,
            &rect,
            blackBrush
        );

        DeleteObject(blackBrush);

        HPEN greenPen =
            CreatePen(
                PS_SOLID,
                4,
                RGB(0, 255, 0)
            );

        HGDIOBJ oldPen =
            SelectObject(hdc, greenPen);

        HGDIOBJ oldBrush =
            SelectObject(
                hdc,
                GetStockObject(NULL_BRUSH)
            );

        Rectangle(
            hdc,
            2,
            2,
            rect.right - 2,
            rect.bottom - 2
        );

        SetBkMode(
            hdc,
            TRANSPARENT
        );

        SetTextColor(
            hdc,
            RGB(0, 255, 0)
        );

        const wchar_t* text =
            L"MauroBC Overlay";

        TextOutW(
            hdc,
            15,
            15,
            text,
            lstrlenW(text)
        );

        SelectObject(
            hdc,
            oldBrush
        );

        SelectObject(
            hdc,
            oldPen
        );

        DeleteObject(greenPen);

        EndPaint(hwnd, &ps);

        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(
        hwnd,
        msg,
        wParam,
        lParam
    );
}

// ------------------##############################################------------------

HWND FindVisibleConsoleWindow()
{
    wchar_t consoleTitle[512]{};

    if (GetConsoleTitleW(
        consoleTitle,
        static_cast<DWORD>(std::size(consoleTitle))
    ) == 0)
    {
        return nullptr;
    }

    struct SearchData
    {
        const wchar_t* title;
        HWND result;
    };

    SearchData data{
        consoleTitle,
        nullptr
    };

    EnumWindows(
        [](HWND hwnd, LPARAM lParam) -> BOOL
        {
            auto* data =
                reinterpret_cast<SearchData*>(lParam);

            if (!IsWindowVisible(hwnd))
                return TRUE;

            wchar_t title[512]{};

            GetWindowTextW(
                hwnd,
                title,
                static_cast<int>(std::size(title))
            );

            if (wcscmp(title, data->title) == 0)
            {
                data->result = hwnd;
                return FALSE;
            }

            return TRUE;
        },
        reinterpret_cast<LPARAM>(&data)
    );

    return data.result;
}


// -------------------##############################################-------------

DWORD WINAPI OverlayThread(LPVOID parameter)
{
    HMODULE dllModule =
        reinterpret_cast<HMODULE>(parameter);

    SetTargetVida(777);

    const wchar_t CLASS_NAME[] =
        L"MauroBCOverlayClass";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = OverlayWndProc;
    wc.hInstance = dllModule;
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClassExW(&wc))
    {
        DWORD error = GetLastError();

        // 1410 = clase ya registrada; eso no nos preocupa.
        if (error != ERROR_CLASS_ALREADY_EXISTS)
            return 1;
    }
    HWND targetWindow =
        FindVisibleConsoleWindow();

    if (targetWindow == nullptr)
        return 1;


    RECT targetRect{};

    if (!GetWindowRect(
        targetWindow,
        &targetRect))
    {
        return 1;
    }

    int width =
        targetRect.right -
        targetRect.left;

    int height =
        targetRect.bottom -
        targetRect.top;


    HWND overlay =
        CreateWindowExW(
            WS_EX_TOPMOST |
            WS_EX_LAYERED |
            WS_EX_TRANSPARENT |
            WS_EX_TOOLWINDOW |
            WS_EX_NOACTIVATE,

            CLASS_NAME,
            L"MauroBC Overlay",

            WS_POPUP,

            targetRect.left,
            targetRect.top,

            width,
            height,

            nullptr,
            nullptr,
            dllModule,
            nullptr
        );

    if (overlay == nullptr)
        return 2;

    SetLayeredWindowAttributes(
        overlay,
        RGB(0, 0, 0),
        0,
        LWA_COLORKEY
    );

    ShowWindow(overlay, SW_SHOW);
    UpdateWindow(overlay);

    MSG msg{};

    while (IsWindow(targetWindow))
    {
        // 1. Obtener posición y tamaño actual del Target
        RECT rect{};

        if (GetWindowRect(targetWindow, &rect))
        {
            int newWidth =
                rect.right - rect.left;

            int newHeight =
                rect.bottom - rect.top;

            // 2. Mover/redimensionar el overlay
            SetWindowPos(
                overlay,
                HWND_TOPMOST,

                rect.left,
                rect.top,

                newWidth,
                newHeight,

                SWP_NOACTIVATE |
                SWP_SHOWWINDOW
            );

            // 3. Pedir que vuelva a dibujarse
            InvalidateRect(
                overlay,
                nullptr,
                FALSE
            );
        }

        // 4. Procesar mensajes de la ventana overlay
        while (PeekMessageW(
            &msg,
            nullptr,
            0,
            0,
            PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                return 0;

            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        Sleep(16);
    }

    DestroyWindow(overlay);

    return 0;


}



// ---------------------------------------------------------
bool SetTargetVida(int nuevaVida)
{
    uintptr_t base =
        reinterpret_cast<uintptr_t>(
            GetModuleHandleW(nullptr)
            );

    constexpr uintptr_t gPlayerOffset = 0x244d8;

    uintptr_t playerAddress =
        *reinterpret_cast<uintptr_t*>(
            base + gPlayerOffset
            );

    if (playerAddress == 0)
        return false;

    int* vidaAddress =
        reinterpret_cast<int*>(
            playerAddress + 0x0
            );

    *vidaAddress = nuevaVida;

    return true;
}

