#include "pch.h"
#include "kfhelper.h"

BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD reason,
    LPVOID reserved
)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
    {
        DisableThreadLibraryCalls(hModule);

        HANDLE thread =
            CreateThread(
                nullptr,
                0,
                KFHelper::MainThread,
                hModule,
                0,
                nullptr
            );

        if (thread != nullptr)
        {
            CloseHandle(thread);
        }

        break;
    }

    case DLL_PROCESS_DETACH:
        break;
    }

    return TRUE;
}