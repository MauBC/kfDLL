#pragma once

#include <Windows.h>
#include <iostream>
#include <cstdio>
#include <iomanip>
#include <cstdint>
#include "kfplayer.h"
#include "kfentities.h"
#include "kfcamera.h"
#include "kfesp.h"
#include "kfoverlay.h"

namespace KFHelper
{
    inline bool OpenConsole()
    {
        if (!AllocConsole())
        {
            return false;
        }

        FILE* stream = nullptr;

        freopen_s(
            &stream,
            "CONOUT$",
            "w",
            stdout
        );

        freopen_s(
            &stream,
            "CONOUT$",
            "w",
            stderr
        );

        SetConsoleTitleW(
            L"KFHelper Debug Console"
        );

        std::cout
            << "========================================\n"
            << "              KF HELPER\n"
            << "========================================\n"
            << "[+] DLL cargada correctamente\n"
            << "[+] K      = informacion del proceso\n"
            << "[+] P      = informacion del jugador\n"
            << "[+] E      = entidades vivas\n"
            << "[+] R      = camera info\n"
            << "[+] F6     = projection debug\n"
            << "[+] F7     = ESP + AimFOV ON/OFF\n"
            << "[+] F8     = head marker ON/OFF\n"
            << "[+] Q hold = smooth aimbot\n"
            << "[+] DELETE = descargar DLL\n"
            << "========================================\n\n";

        return true;
    }

    inline uintptr_t GetModuleBase(
        const char* moduleName
    )
    {
        HMODULE module =
            GetModuleHandleA(moduleName);

        return reinterpret_cast<uintptr_t>(
            module
            );
    }


    inline void PrintProcessInfo(
        HMODULE dllModule
    )
    {
        DWORD processId =
            GetCurrentProcessId();

        DWORD threadId =
            GetCurrentThreadId();

        HMODULE exeBase =
            GetModuleHandleW(nullptr);

        uintptr_t engineBase =
            GetModuleBase("Engine.dll");

        uintptr_t coreBase =
            GetModuleBase("Core.dll");

        wchar_t exePath[MAX_PATH]{};

        GetModuleFileNameW(
            nullptr,
            exePath,
            MAX_PATH
        );

        std::cout
            << "\n========================================\n"
            << "           RUNTIME SNAPSHOT\n"
            << "========================================\n";

        std::cout
            << "[PID]              "
            << processId
            << '\n';

        std::cout
            << "[Thread ID]        "
            << threadId
            << '\n';

        std::wcout
            << L"[Executable]       "
            << exePath
            << L'\n';

        std::cout
            << std::hex
            << std::uppercase
            << std::showbase;

        std::cout
            << "[EXE Base]         "
            << reinterpret_cast<uintptr_t>(
                exeBase
                )
            << '\n';

        std::cout
            << "[DLL Base]         "
            << reinterpret_cast<uintptr_t>(
                dllModule
                )
            << '\n';

        std::cout
            << "[Engine.dll Base]  "
            << engineBase
            << '\n';

        std::cout
            << "[Core.dll Base]    "
            << coreBase
            << '\n';

#ifdef _WIN64

        std::cout
            << "[Architecture]     x64\n";

#else

        std::cout
            << "[Architecture]     x86\n";

#endif

        /*
            Offset que estuvimos estudiando en Ghidra.

            IMPORTANTE:
            En TU Engine.dll, Ghidra identifico este RVA
            como USound::Audio, NO como GEngine.
        */

        constexpr uintptr_t TEST_ENGINE_RVA =
            0x004C6934;

        if (engineBase != 0)
        {
            uintptr_t testAddress =
                engineBase + TEST_ENGINE_RVA;

            std::cout
                << "[Engine+4C6934]   "
                << testAddress
                << '\n';
        }
        else
        {
            std::cout
                << "[Engine.dll]      NOT LOADED\n";
        }

        std::cout
            << std::dec
            << std::nouppercase
            << std::noshowbase;

        std::cout
            << "========================================\n\n";
    }


    // ============================================================
    // ROTATION PROBE
    //
    // Offsets candidatos tomados de AActor::GetViewRotation.
    // Todavia NO se consideran confirmados.
    // ============================================================

    inline void PrintRotationProbe()
    {
        KFPlayer::Context context;

        if (!KFPlayer::ResolveContext(context))
        {
            std::cout << "`n[ROTATION] No se pudo resolver el jugador local.`n";
            return;
        }

        constexpr uintptr_t CandidatePitch = 0x158;
        constexpr uintptr_t CandidateYaw   = 0x15C;
        constexpr uintptr_t CandidateRoll  = 0x160;

        int pawnPitch = 0;
        int pawnYaw   = 0;
        int pawnRoll  = 0;

        int controllerPitch = 0;
        int controllerYaw   = 0;
        int controllerRoll  = 0;

        const bool pawnPitchOk =
            KFMemory::Read(
                context.pawn + CandidatePitch,
                pawnPitch
            );

        const bool pawnYawOk =
            KFMemory::Read(
                context.pawn + CandidateYaw,
                pawnYaw
            );

        const bool pawnRollOk =
            KFMemory::Read(
                context.pawn + CandidateRoll,
                pawnRoll
            );

        const bool controllerPitchOk =
            KFMemory::Read(
                context.controller + CandidatePitch,
                controllerPitch
            );

        const bool controllerYawOk =
            KFMemory::Read(
                context.controller + CandidateYaw,
                controllerYaw
            );

        const bool controllerRollOk =
            KFMemory::Read(
                context.controller + CandidateRoll,
                controllerRoll
            );

        std::cout << "`n========================================`n";
        std::cout << "          ROTATION PROBE`n";
        std::cout << "========================================`n";

        std::cout
            << std::hex
            << std::uppercase
            << std::showbase;

        std::cout
            << "[Pawn]       "
            << context.pawn
            << '\n';

        std::cout
            << "[Controller] "
            << context.controller
            << '\n';

        std::cout
            << std::dec
            << std::noshowbase
            << std::nouppercase;

        std::cout << "`n--- Pawn + 0x158 ---`n";

        if (pawnPitchOk)
            std::cout << "Pitch : " << pawnPitch << '\n';

        if (pawnYawOk)
            std::cout << "Yaw   : " << pawnYaw << '\n';

        if (pawnRollOk)
            std::cout << "Roll  : " << pawnRoll << '\n';

        std::cout << "`n--- Controller + 0x158 ---`n";

        if (controllerPitchOk)
            std::cout << "Pitch : " << controllerPitch << '\n';

        if (controllerYawOk)
            std::cout << "Yaw   : " << controllerYaw << '\n';

        if (controllerRollOk)
            std::cout << "Roll  : " << controllerRoll << '\n';

        std::cout << "========================================`n`n";
    }

    inline void CloseConsole()
    {
        DWORD processList[16]{};

        DWORD processCount =
            GetConsoleProcessList(
                processList,
                16
            );

        std::cout
            << "\n========================================\n"
            << "[-] Descargando DLL...\n"
            << "[i] Procesos conectados a consola: "
            << processCount
            << '\n'
            << "========================================\n";

        std::cout.flush();
        std::cerr.flush();

        fclose(stdout);
        fclose(stderr);

        FreeConsole();
    }


    inline DWORD WINAPI MainThread(
        LPVOID parameter
    )
    {
        HMODULE dllModule =
            static_cast<HMODULE>(
                parameter
                );

        if (!OpenConsole())
        {
            FreeLibraryAndExitThread(
                dllModule,
                0
            );
        }

        PrintProcessInfo(
            dllModule
        );

        while (true)
        {
            if (GetAsyncKeyState('K') & 1)
            {
                PrintProcessInfo(
                    dllModule
                );
            }

            if (GetAsyncKeyState('P') & 1)
            {
                /*
                    Aquí irá PrintPlayerInfo()
                    cuando tengamos el puntero correcto.
                */

                KFPlayer::PrintPlayerInfo();
            }
            if (GetAsyncKeyState('E') & 1)
            {
                KFEntities::PrintLivingEntities();
            }

            if (GetAsyncKeyState('R') & 1)
            {
                KFCamera::PrintCameraInfo();
            }


            if (GetAsyncKeyState(VK_F6) & 1)
            {
                KFESP::PrintProjectionDebug();
            }


            if (GetAsyncKeyState(VK_F7) & 1)
            {
                const bool active =
                    KFOverlay::Toggle(
                        dllModule
                    );


                std::cout
                    << (
                        active ?
                        "\n[ESP] Overlay activado.\n" :
                        "\n[ESP] Overlay desactivado.\n"
                    );
            }


            if (GetAsyncKeyState(VK_F8) & 1)
            {
                const bool markerEnabled =
                    KFOverlay::ToggleHeadMarker();


                std::cout
                    << "\n[ESP] Head marker: "
                    << (
                        markerEnabled ?
                        "ON" :
                        "OFF"
                    )
                    << '\n';
            }

if (GetAsyncKeyState(VK_DELETE) & 1)
            {
                KFOverlay::Stop();
                break;
            }

            Sleep(50);
        }

        CloseConsole();

        FreeLibraryAndExitThread(
            dllModule,
            0
        );
    }

}