#pragma once

#include <Windows.h>
#include <iostream>
#include <cstdio>
#include <iomanip>
#include <cstdint>

#include "kfplayer.h"
#include "kfentities.h"
#include "kfcamera.h"
#include "kfcollision.h"
#include "kfgamethread.h"
#include "kfesp.h"
#include "kfxray.h"
#include "kftargetsnapshot.h"
#include "kfoverlay.h"


namespace KFHelper
{
    // ============================================================
    // CONSOLE
    // ============================================================

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
            << "[+] F6     = debug ciclico\n"
            << "             Player -> Entities -> Camera\n"
            << "             -> Projection -> LOS -> Runtime -> Glow\n"
            << "[+] F7     = ESP + LOS + Glow + AimFOV ON/OFF\n"
            << "[+] F8     = head marker ON/OFF\n"
            << "[+] Q hold = smooth aimbot\n"
            << "[+] DELETE = descarga segura\n"
            << "========================================\n\n";


        return true;
    }


    inline void CloseConsole()
    {
        DWORD processList[16]{};


        const DWORD processCount =
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


    // ============================================================
    // MODULE
    // ============================================================

    inline uintptr_t GetModuleBase(
        const char* moduleName
    )
    {
        const HMODULE module =
            GetModuleHandleA(
                moduleName
            );


        return
            reinterpret_cast<uintptr_t>(
                module
            );
    }


    // ============================================================
    // RUNTIME INFO
    // ============================================================

    inline void PrintProcessInfo(
        HMODULE dllModule
    )
    {
        const DWORD processId =
            GetCurrentProcessId();


        const DWORD threadId =
            GetCurrentThreadId();


        const HMODULE exeBase =
            GetModuleHandleW(
                nullptr
            );


        const uintptr_t engineBase =
            GetModuleBase(
                "Engine.dll"
            );


        const uintptr_t coreBase =
            GetModuleBase(
                "Core.dll"
            );


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


        // En este build:
        //
        // Engine + 0x4C6934 = USound::Audio
        //
        // Se conserva solamente como referencia historica
        // del laboratorio.

        constexpr uintptr_t TestEngineRva =
            0x004C6934;


        if (engineBase != 0)
        {
            std::cout
                << "[Engine+4C6934]   "
                << engineBase +
                    TestEngineRva
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
    // KEY EDGE
    //
    // GetAsyncKeyState & 1 depende del bit de transicion global
    // de Windows y puede perder eventos.
    //
    // Aqui usamos el estado fisico (0x8000) y hacemos nuestro
    // propio rising-edge dentro de MainThread.
    // ============================================================

    inline bool KeyPressed(
        int virtualKey
    )
    {
        if (
            virtualKey < 0 ||
            virtualKey > 255
            )
        {
            return false;
        }


        // MainThread es el unico consumidor.
        static bool previousState[256]{};


        const bool isDown =
            (
                GetAsyncKeyState(
                    virtualKey
                ) &
                0x8000
            ) != 0;


        const bool pressed =
            isDown &&
            !previousState[virtualKey];


        previousState[virtualKey] =
            isDown;


        return pressed;
    }


    // ============================================================
    // DEBUG PAGES
    // ============================================================

    inline void PrintNextDiagnostic(
        HMODULE dllModule
    )
    {
        // MainThread es el unico escritor.
        static unsigned int page =
            0;


        std::cout
            << "\n========================================\n"
            << "             DEBUG PAGE "
            << (page + 1)
            << "/7\n"
            << "========================================\n";


        switch (page)
        {
        case 0:

            std::cout
                << "[DEBUG] PLAYER\n";


            KFPlayer::PrintPlayerInfo();

            break;


        case 1:

            std::cout
                << "[DEBUG] ENTITIES\n";


            KFEntities::PrintLivingEntities();

            break;


        case 2:

            std::cout
                << "[DEBUG] CAMERA\n";


            KFCamera::PrintCameraInfo();

            break;


        case 3:

            std::cout
                << "[DEBUG] PROJECTION\n";


            KFESP::PrintProjectionDebug();

            break;


        case 4:

            std::cout
                << "[DEBUG] GAME THREAD / LOS\n";


            KFCollision::PrintStatus();

            break;


        case 5:

            std::cout
                << "[DEBUG] RUNTIME\n";


            PrintProcessInfo(
                dllModule
            );

            break;


        default:

            std::cout
                << "[DEBUG] GLOW SNAPSHOT\n";


            KFXRay::PrintStatus();

            KFXRay::PrintBridgeStatus();

            KFXRay::PrintDipStatus();

            KFTargetSnapshot::PrintStatus();

            break;
        }


        page =
            (page + 1) %
            7;
    }


    // ============================================================
    // MAIN THREAD
    // ============================================================

    inline DWORD WINAPI MainThread(
        LPVOID parameter
    )
    {
        const HMODULE dllModule =
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


        // SingleLineCheck / GetBoneCoords y otras operaciones UE2
        // sensibles deben ejecutarse desde el game thread.

        KFGameThread::SetFrameCallback(
            &KFCollision::OnGameFrame
        );


        const bool hookInstalled =
            KFGameThread::Install();


        std::cout
            << "[GAME THREAD] MasterProcessPostRender: "
            << (
                hookInstalled
                    ?
                    "INSTALLED"
                    :
                    "FAILED"
            )
            << '\n';


        const bool bridgeInstalled =
            KFXRay::InstallBridge();


        std::cout
            << "[GLOW] UE2 render bridge: "
            << (
                bridgeInstalled
                    ?
                    "INSTALLED"
                    :
                    "FAILED"
            )
            << '\n';


        // --------------------------------------------------------
        // SHUTDOWN STATE
        //
        // Cuando DELETE inicia el unload no volvemos a aceptar
        // F6/F7/F8. Esperamos solamente a que el game thread
        // restaure los bytes originales del hook.
        // --------------------------------------------------------

        bool shutdownPending =
            false;


        bool gameThreadUninstallRequested =
            false;


        while (true)
        {
            if (shutdownPending)
            {
                // Primero debe desaparecer el bridge UE2.
                //
                // Mientras siga instalado conservamos el callback
                // del game thread para que OnGameFrame pueda
                // atender RequestBridgeUninstall().
                if (
                    !gameThreadUninstallRequested &&
                    !KFXRay::IsBridgeInstalled() &&
                    !KFXRay::IsDipInstalled()
                    )
                {
                    KFGameThread::SetFrameCallback(
                        nullptr
                    );


                    KFGameThread::RequestUninstall();


                    gameThreadUninstallRequested =
                        true;
                }


                if (
                    gameThreadUninstallRequested &&
                    KFXRay::IsBridgeSafeToUnload() &&
                    KFXRay::IsDipSafeToUnload() &&
                    KFGameThread::IsSafeToUnload()
                    )
                {
                    break;
                }


                Sleep(
                    20
                );


                continue;
            }


            // ====================================================
            // F6 - DEBUG CYCLER
            // ====================================================

            if (KeyPressed(VK_F6))
            {
                PrintNextDiagnostic(
                    dllModule
                );
            }


            // ====================================================
            // F7 - OVERLAY + LOS
            // ====================================================

            if (KeyPressed(VK_F7))
            {
                const bool active =
                    KFOverlay::Toggle(
                        dllModule
                    );


                KFCollision::SetEnabled(
                    active &&
                    KFGameThread::IsInstalled()
                );


                KFXRay::SetEnabled(
                    active &&
                    KFGameThread::IsInstalled() &&
                    KFXRay::IsBridgeInstalled()
                );


                std::cout
                    << (
                        active
                            ?
                            "\n[ESP] Overlay + LOS + Glow activados.\n"
                            :
                            "\n[ESP] Overlay + LOS + Glow desactivados.\n"
                    );


                if (
                    active &&
                    !KFGameThread::IsInstalled()
                    )
                {
                    std::cout
                        << "[LOS] Hook no disponible; "
                        << "ESP clasico activo.\n";
                }


                if (
                    active &&
                    !KFXRay::IsBridgeInstalled()
                    )
                {
                    std::cout
                        << "[GLOW] Render bridge no disponible; "
                        << "Glow desactivado.\n";
                }
            }


            // ====================================================
            // F8 - HEAD MARKER
            // ====================================================

            if (KeyPressed(VK_F8))
            {
                const bool markerEnabled =
                    KFOverlay::ToggleHeadMarker();


                std::cout
                    << "\n[ESP] Head marker: "
                    << (
                        markerEnabled
                            ?
                            "ON"
                            :
                            "OFF"
                    )
                    << '\n';
            }


            // ====================================================
            // DELETE - SAFE UNLOAD
            // ====================================================

            if (KeyPressed(VK_DELETE))
            {
                // 1. No publicar ni ejecutar nuevo trabajo LOS.

                KFCollision::SetEnabled(
                    false
                );


                KFXRay::SetEnabled(
                    false
                );


                KFTargetSnapshot::Clear();


                // 2. Detener OverlayThread.
                //
                // Si sigue vivo NO podemos descargar MemoryDll,
                // porque ese thread podria seguir ejecutando codigo
                // perteneciente al modulo descargado.

                if (!KFOverlay::Stop())
                {
                    std::cout
                        << "\n[UNLOAD] OverlayThread aun esta activo.\n"
                        << "[UNLOAD] DLL NO descargada. "
                        << "Pulsa DELETE nuevamente.\n";


                    continue;
                }


                // 3. Pedimos retirar primero el bridge UE2.
                //
                // KFCollision::OnGameFrame lo procesara desde el
                // game thread incluso aunque LOS ya este OFF.

                KFXRay::RequestDipUninstall();

                KFXRay::RequestBridgeUninstall();


                shutdownPending =
                    true;


                std::cout
                    << "\n[UNLOAD] Esperando PostRender para retirar "
                    << "UE2 render bridge...\n";
            }


            Sleep(
                20
            );
        }


        // ========================================================
        // FINAL CLEANUP
        // ========================================================

        KFXRay::SetEnabled(
            false
        );


        KFXRay::ClearBlockedPawns();


        KFTargetSnapshot::Clear();


        KFXRay::FinalizeDipHook();


        KFXRay::FinalizeBridge();


        KFCollision::Clear();


        KFGameThread::SetFrameCallback(
            nullptr
        );


        CloseConsole();


        FreeLibraryAndExitThread(
            dllModule,
            0
        );
    }

}