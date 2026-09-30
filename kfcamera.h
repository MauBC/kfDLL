#pragma once

#include <Windows.h>
#include <cstdint>
#include <cmath>
#include <iomanip>
#include <iostream>

#include "kfoffsets.h"
#include "kfmemory.h"
#include "kfplayer.h"


namespace KFCamera
{
    // ============================================================
    // VECTOR 3D
    // ============================================================

    struct Vec3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };


    // ============================================================
    // ROTACION UE2
    //
    // Una vuelta completa:
    //
    // 65536 unidades = 360 grados.
    // ============================================================

    struct Rotation
    {
        int pitch = 0;
        int yaw = 0;
        int roll = 0;
    };


    // ============================================================
    // CAMERA SNAPSHOT
    // ============================================================

    struct Snapshot
    {
        uintptr_t controller = 0;
        uintptr_t pawn = 0;

        // Posicion base del Pawn.
        Vec3 pawnLocation;

        // Origen visual candidato:
        //
        // X = Pawn.X
        // Y = Pawn.Y
        // Z = Pawn.Z + EyeHeight
        Vec3 eyeLocation;

        float baseEyeHeight = 0.0f;
        float eyeHeight = 0.0f;

        Rotation rotation;

        float fov = 0.0f;

        bool hasPawnLocation = false;
        bool hasEyeHeight = false;
        bool hasRotation = false;
        bool hasFov = false;
    };


    // ============================================================
    // NORMALIZAR ROTATOR 16-BIT
    // ============================================================

    inline int NormalizeRotation16(
        int value
    )
    {
        int normalized =
            value & 0xFFFF;

        if (normalized >= 32768)
        {
            normalized -= 65536;
        }

        return normalized;
    }


    // ============================================================
    // ROTATOR UE2 -> GRADOS
    // ============================================================

    inline float RotationToDegrees(
        int value
    )
    {
        return
            static_cast<float>(
                NormalizeRotation16(value)
            ) *
            (360.0f / 65536.0f);
    }


    // ============================================================
    // ROTATOR UE2 -> RADIANES
    // ============================================================

    inline float RotationToRadians(
        int value
    )
    {
        constexpr float Pi =
            3.14159265358979323846f;

        return
            static_cast<float>(
                NormalizeRotation16(value)
            ) *
            ((2.0f * Pi) / 65536.0f);
    }



    // ============================================================
    // VIEWPORT
    // ============================================================

    struct Viewport
    {
        int width = 0;
        int height = 0;
    };


    // ============================================================
    // SCREEN POINT
    // ============================================================

    struct ScreenPoint
    {
        float x = 0.0f;
        float y = 0.0f;

        // Distancia sobre el eje forward de la camara.
        float depth = 0.0f;

        bool onScreen = false;
    };


    // ============================================================
    // BUSCAR VENTANA PRINCIPAL DEL JUEGO
    //
    // Elegimos la ventana visible de mayor area perteneciente
    // al proceso actual.
    // ============================================================

    struct WindowSearchContext
    {
        DWORD processId = 0;

        HWND bestWindow = nullptr;

        long long bestArea = 0;
    };


    inline BOOL CALLBACK EnumGameWindowsProc(
        HWND hwnd,
        LPARAM parameter
    )
    {
        WindowSearchContext* context =
            reinterpret_cast<WindowSearchContext*>(
                parameter
            );


        if (context == nullptr)
        {
            return TRUE;
        }


        DWORD windowProcessId = 0;


        GetWindowThreadProcessId(
            hwnd,
            &windowProcessId
        );


        if (
            windowProcessId != context->processId ||
            !IsWindowVisible(hwnd)
            )
        {
            return TRUE;
        }


        // --------------------------------------------------------
        // IGNORAR CONSOLA
        //
        // AllocConsole() crea una ventana que pertenece al MISMO
        // KillingFloor.exe.
        //
        // En modo ventana pequeño la consola puede tener mayor area
        // que el cliente del juego y anteriormente terminaba siendo
        // seleccionada como "gameWindow".
        // --------------------------------------------------------

        const HWND consoleWindow =
            GetConsoleWindow();


        if (
            consoleWindow != nullptr &&
            hwnd == consoleWindow
            )
        {
            return TRUE;
        }


        // Defensa adicional para consolas Win32 clasicas.
        wchar_t windowClass[128]{};


        if (
            GetClassNameW(
                hwnd,
                windowClass,
                static_cast<int>(
                    std::size(
                        windowClass
                    )
                )
            ) > 0
            )
        {
            if (
                lstrcmpiW(
                    windowClass,
                    L"ConsoleWindowClass"
                ) == 0
                )
            {
                return TRUE;
            }
        }


        // Tambien ignoramos explicitamente nuestra consola debug
        // por titulo en caso de que el host cambie.
        wchar_t windowTitle[256]{};


        if (
            GetWindowTextW(
                hwnd,
                windowTitle,
                static_cast<int>(
                    std::size(
                        windowTitle
                    )
                )
            ) > 0
            )
        {
            if (
                lstrcmpiW(
                    windowTitle,
                    L"KFHelper Debug Console"
                ) == 0
                )
            {
                return TRUE;
            }
        }


        // Ignorar overlays/tool windows del propio proceso.
        const LONG_PTR exStyle =
            GetWindowLongPtrW(
                hwnd,
                GWL_EXSTYLE
            );

        if ((exStyle & WS_EX_TOOLWINDOW) != 0)
        {
            return TRUE;
        }


        // Ignorar ventanas owned/secundarias.
        if (GetWindow(hwnd, GW_OWNER) != nullptr)
        {
            return TRUE;
        }


        RECT rect{};


        if (!GetClientRect(
            hwnd,
            &rect
        ))
        {
            return TRUE;
        }


        const int width =
            rect.right -
            rect.left;


        const int height =
            rect.bottom -
            rect.top;


        if (
            width <= 0 ||
            height <= 0
            )
        {
            return TRUE;
        }


        const long long area =
            static_cast<long long>(width) *
            static_cast<long long>(height);


        if (area > context->bestArea)
        {
            context->bestArea =
                area;

            context->bestWindow =
                hwnd;
        }


        return TRUE;
    }


    inline HWND FindGameWindow()
    {
        WindowSearchContext context;

        context.processId =
            GetCurrentProcessId();


        EnumWindows(
            EnumGameWindowsProc,
            reinterpret_cast<LPARAM>(
                &context
            )
        );


        return context.bestWindow;
    }


    inline bool ReadViewport(
        Viewport& viewport
    )
    {
        viewport = {};


        const HWND gameWindow =
            FindGameWindow();


        if (gameWindow == nullptr)
        {
            return false;
        }


        RECT rect{};


        if (!GetClientRect(
            gameWindow,
            &rect
        ))
        {
            return false;
        }


        viewport.width =
            rect.right -
            rect.left;


        viewport.height =
            rect.bottom -
            rect.top;


        return
            viewport.width > 0 &&
            viewport.height > 0;
    }


    // ============================================================
    // WORLD TO SCREEN
    //
    // Convencion UE2 utilizada:
    //
    // X = forward
    // Y = right
    // Z = up
    //
    // El FOV utilizado es el valor efectivo:
    //
    // Controller +0x36C
    //
    // ============================================================

    inline bool WorldToScreen(
        const Vec3& worldPosition,
        const Snapshot& camera,
        const Viewport& viewport,
        ScreenPoint& screen
    )
    {
        screen = {};


        if (
            viewport.width <= 0 ||
            viewport.height <= 0
            )
        {
            return false;
        }


        if (
            camera.fov <= 1.0f ||
            camera.fov >= 179.0f
            )
        {
            return false;
        }


        // --------------------------------------------------------
        // Delta mundo -> camara
        // --------------------------------------------------------

        const float dx =
            worldPosition.x -
            camera.eyeLocation.x;


        const float dy =
            worldPosition.y -
            camera.eyeLocation.y;


        const float dz =
            worldPosition.z -
            camera.eyeLocation.z;


        // --------------------------------------------------------
        // Rotacion UE2 -> radianes
        // --------------------------------------------------------

        const float pitch =
            RotationToRadians(
                camera.rotation.pitch
            );


        const float yaw =
            RotationToRadians(
                camera.rotation.yaw
            );


        // Roll actualmente es 0 en nuestras pruebas.
        // No es necesario para la primera validacion W2S.


        const float cosPitch =
            std::cos(pitch);


        const float sinPitch =
            std::sin(pitch);


        const float cosYaw =
            std::cos(yaw);


        const float sinYaw =
            std::sin(yaw);


        // --------------------------------------------------------
        // Camera basis
        //
        // Forward
        // Right
        // Up
        // --------------------------------------------------------

        const Vec3 forward
        {
            cosPitch * cosYaw,
            cosPitch * sinYaw,
            sinPitch
        };


        const Vec3 right
        {
            -sinYaw,
            cosYaw,
            0.0f
        };


        const Vec3 up
        {
            -sinPitch * cosYaw,
            -sinPitch * sinYaw,
            cosPitch
        };


        // --------------------------------------------------------
        // Transformar el punto a camera space
        // --------------------------------------------------------

        const float cameraForward =
            dx * forward.x +
            dy * forward.y +
            dz * forward.z;


        const float cameraRight =
            dx * right.x +
            dy * right.y +
            dz * right.z;


        const float cameraUp =
            dx * up.x +
            dy * up.y +
            dz * up.z;


        screen.depth =
            cameraForward;


        // Detras de la camara.
        if (cameraForward <= 1.0f)
        {
            return false;
        }


        // --------------------------------------------------------
        // Perspective projection
        //
        // Asumimos FOV horizontal.
        // --------------------------------------------------------

        constexpr float Pi =
            3.14159265358979323846f;


        const float fovRadians =
            camera.fov *
            (Pi / 180.0f);


        const float halfWidth =
            static_cast<float>(
                viewport.width
            ) * 0.5f;


        const float halfHeight =
            static_cast<float>(
                viewport.height
            ) * 0.5f;


        const float focalLength =
            halfWidth /
            std::tan(
                fovRadians * 0.5f
            );


        screen.x =
            halfWidth +
            (
                cameraRight *
                focalLength /
                cameraForward
            );


        screen.y =
            halfHeight -
            (
                cameraUp *
                focalLength /
                cameraForward
            );


        screen.onScreen =
            screen.x >= 0.0f &&
            screen.x <=
                static_cast<float>(
                    viewport.width
                ) &&
            screen.y >= 0.0f &&
            screen.y <=
                static_cast<float>(
                    viewport.height
                );


        return true;
    }

    // ============================================================
    // LEER ROTACION
    // ============================================================

    inline bool ReadRotation(
        uintptr_t controller,
        Rotation& rotation
    )
    {
        rotation = {};

        if (controller == 0)
        {
            return false;
        }


        const bool pitchOk =
            KFMemory::Read(
                controller +
                KFOffsets::Actor::Pitch,
                rotation.pitch
            );


        const bool yawOk =
            KFMemory::Read(
                controller +
                KFOffsets::Actor::Yaw,
                rotation.yaw
            );


        const bool rollOk =
            KFMemory::Read(
                controller +
                KFOffsets::Actor::Roll,
                rotation.roll
            );


        return
            pitchOk &&
            yawOk &&
            rollOk;
    }


    // ============================================================
    // LEER CAMERA SNAPSHOT
    // ============================================================

    inline bool ReadSnapshot(
        Snapshot& snapshot
    )
    {
        snapshot = {};


        KFPlayer::Context context;

        if (!KFPlayer::ResolveContext(
            context
        ))
        {
            return false;
        }


        snapshot.controller =
            context.controller;

        snapshot.pawn =
            context.pawn;


        if (
            snapshot.controller == 0 ||
            snapshot.pawn == 0
            )
        {
            return false;
        }


        // --------------------------------------------------------
        // Posicion Pawn
        // --------------------------------------------------------

        const bool xOk =
            KFMemory::Read(
                snapshot.pawn +
                KFOffsets::Pawn::X,
                snapshot.pawnLocation.x
            );


        const bool yOk =
            KFMemory::Read(
                snapshot.pawn +
                KFOffsets::Pawn::Y,
                snapshot.pawnLocation.y
            );


        const bool zOk =
            KFMemory::Read(
                snapshot.pawn +
                KFOffsets::Pawn::Z,
                snapshot.pawnLocation.z
            );


        snapshot.hasPawnLocation =
            xOk &&
            yOk &&
            zOk;


        // --------------------------------------------------------
        // Eye Height
        // --------------------------------------------------------

        const bool baseEyeOk =
            KFMemory::Read(
                snapshot.pawn +
                KFOffsets::Pawn::BaseEyeHeight,
                snapshot.baseEyeHeight
            );


        const bool eyeOk =
            KFMemory::Read(
                snapshot.pawn +
                KFOffsets::Pawn::EyeHeight,
                snapshot.eyeHeight
            );


        snapshot.hasEyeHeight =
            baseEyeOk &&
            eyeOk;


        if (
            snapshot.hasPawnLocation &&
            snapshot.hasEyeHeight
            )
        {
            snapshot.eyeLocation.x =
                snapshot.pawnLocation.x;

            snapshot.eyeLocation.y =
                snapshot.pawnLocation.y;

            snapshot.eyeLocation.z =
                snapshot.pawnLocation.z +
                snapshot.eyeHeight;
        }


        // --------------------------------------------------------
        // Rotacion
        // --------------------------------------------------------

        snapshot.hasRotation =
            ReadRotation(
                snapshot.controller,
                snapshot.rotation
            );


        // --------------------------------------------------------
        // FOV
        //
        // Controller +0x36C
        //
        // Confirmado mediante:
        //
        // FOV 80 / 110
        // ADS de pistola
        // Crossbow zoom 110 -> 22 -> 110
        // --------------------------------------------------------

        snapshot.hasFov =
            KFMemory::Read(
                snapshot.controller +
                KFOffsets::Controller::FOV,
                snapshot.fov
            );


        return
            snapshot.hasPawnLocation &&
            snapshot.hasEyeHeight &&
            snapshot.hasRotation &&
            snapshot.hasFov;
    }


    // ============================================================
    // DEBUG
    // ============================================================

    inline void PrintCameraInfo()
    {
        Snapshot camera;


        if (!ReadSnapshot(camera))
        {
            std::cout
                << "\n[CAMERA] No se pudo leer el snapshot.\n";

            return;
        }


        std::cout
            << "\n========================================\n"
            << "              CAMERA INFO\n"
            << "========================================\n";


        std::cout
            << std::hex
            << std::uppercase
            << std::showbase;


        std::cout
            << "[Controller]       "
            << camera.controller
            << '\n';


        std::cout
            << "[Pawn]             "
            << camera.pawn
            << '\n';


        std::cout
            << std::dec
            << std::noshowbase
            << std::nouppercase;


        std::cout
            << std::fixed
            << std::setprecision(3);


        // --------------------------------------------------------
        // Localizacion
        // --------------------------------------------------------

        std::cout
            << "\n------------- LOCATION -----------------\n";


        std::cout
            << "[Pawn X]           "
            << camera.pawnLocation.x
            << '\n';


        std::cout
            << "[Pawn Y]           "
            << camera.pawnLocation.y
            << '\n';


        std::cout
            << "[Pawn Z]           "
            << camera.pawnLocation.z
            << '\n';


        std::cout
            << "[BaseEyeHeight]    "
            << camera.baseEyeHeight
            << '\n';


        std::cout
            << "[EyeHeight]        "
            << camera.eyeHeight
            << '\n';


        std::cout
            << "[Eye X]            "
            << camera.eyeLocation.x
            << '\n';


        std::cout
            << "[Eye Y]            "
            << camera.eyeLocation.y
            << '\n';


        std::cout
            << "[Eye Z]            "
            << camera.eyeLocation.z
            << '\n';


        // --------------------------------------------------------
        // Rotacion
        // --------------------------------------------------------

        std::cout
            << "\n------------- ROTATION -----------------\n";


        std::cout
            << "[Pitch]            "
            << camera.rotation.pitch
            << " | "
            << RotationToDegrees(
                camera.rotation.pitch
            )
            << " deg\n";


        std::cout
            << "[Yaw]              "
            << camera.rotation.yaw
            << " | "
            << RotationToDegrees(
                camera.rotation.yaw
            )
            << " deg\n";


        std::cout
            << "[Roll]             "
            << camera.rotation.roll
            << " | "
            << RotationToDegrees(
                camera.rotation.roll
            )
            << " deg\n";


        // --------------------------------------------------------
        // FOV
        // --------------------------------------------------------

        std::cout
            << "\n--------------- FOV --------------------\n";


        std::cout
            << "[FOV]              "
            << camera.fov
            << " deg\n";


        std::cout
            << "========================================\n\n";
    }
}