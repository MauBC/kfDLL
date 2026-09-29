#pragma once

#include <cmath>
#include <cstdint>

#include "kftargeting.h"


namespace KFAimbot
{
    // ============================================================
    // CONFIGURACION
    // ============================================================

    namespace Config
    {
        constexpr float Pi =
            3.14159265358979323846f;


        constexpr float UnrealRotatorScale =
            65536.0f /
            (2.0f * Pi);


        // 0.0  = no se mueve
        // 1.0  = snap instantaneo
        //
        // 0.20 = movimiento suave pero suficientemente rapido.
        constexpr float SmoothFactor =
            0.20f;


        // Limite vertical aproximado utilizado por UE2.
        constexpr int MinPitch =
            -16000;


        constexpr int MaxPitch =
            16000;


        constexpr float MinimumHorizontalDistance =
            5.0f;
    }


    // ============================================================
    // ROTATION
    // ============================================================

    struct DesiredRotation
    {
        int pitch = 0;

        int yaw = 0;

        bool valid = false;
    };


    // ============================================================
    // NORMALIZAR ROTATOR
    //
    // Convierte cualquier valor al rango:
    //
    // -32768 .. 32767
    // ============================================================

    inline int NormalizeSigned16(
        int value
    )
    {
        int result =
            value &
            0xFFFF;


        if (result >= 32768)
        {
            result -=
                65536;
        }


        return result;
    }


    // ============================================================
    // RAD -> UE2 ROTATOR
    // ============================================================

    inline int RadiansToRotator(
        float radians
    )
    {
        return
            static_cast<int>(
                std::lround(
                    radians *
                    Config::UnrealRotatorScale
                )
            );
    }


    // ============================================================
    // SHORTEST DELTA
    // ============================================================

    inline int ShortestDelta(
        int current,
        int desired
    )
    {
        return
            NormalizeSigned16(
                desired -
                current
            );
    }


    // ============================================================
    // CLAMP PITCH
    // ============================================================

    inline int ClampPitch(
        int pitch
    )
    {
        if (
            pitch >
            Config::MaxPitch
            )
        {
            return
                Config::MaxPitch;
        }


        if (
            pitch <
            Config::MinPitch
            )
        {
            return
                Config::MinPitch;
        }


        return pitch;
    }


    // ============================================================
    // SMOOTH
    // ============================================================

    inline int SmoothRotation(
        int current,
        int desired
    )
    {
        const int delta =
            ShortestDelta(
                current,
                desired
            );


        if (delta == 0)
        {
            return current;
        }


        int step =
            static_cast<int>(
                std::lround(
                    static_cast<float>(
                        delta
                    ) *
                    Config::SmoothFactor
                )
            );


        // Evita quedarse eternamente a 1-2 rotator units
        // del objetivo por redondeo.
        if (step == 0)
        {
            step =
                delta > 0
                    ?
                    1
                    :
                    -1;
        }


        return
            NormalizeSigned16(
                current +
                step
            );
    }


    // ============================================================
    // CALCULAR ROTACION DESEADA
    //
    // Convencion confirmada experimentalmente:
    //
    // X = forward
    // Y = right
    // Z = up
    //
    // Yaw:
    //
    // atan2(dY, dX)
    //
    // Pitch:
    //
    // atan2(dZ, horizontal)
    // ============================================================

    inline DesiredRotation CalculateDesiredRotation(
        const KFCamera::Snapshot& camera,
        const KFESP::Entry& target
    )
    {
        DesiredRotation result;


        const float dx =
            target.headWorld.x -
            camera.eyeLocation.x;


        const float dy =
            target.headWorld.y -
            camera.eyeLocation.y;


        const float dz =
            target.headWorld.z -
            camera.eyeLocation.z;


        const float horizontalDistance =
            std::sqrt(
                dx * dx +
                dy * dy
            );


        if (
            horizontalDistance <
            Config::MinimumHorizontalDistance
            )
        {
            return result;
        }


        const float yawRadians =
            std::atan2(
                dy,
                dx
            );


        const float pitchRadians =
            std::atan2(
                dz,
                horizontalDistance
            );


        result.yaw =
            NormalizeSigned16(
                RadiansToRotator(
                    yawRadians
                )
            );


        result.pitch =
            ClampPitch(
                NormalizeSigned16(
                    RadiansToRotator(
                        pitchRadians
                    )
                )
            );


        result.valid =
            true;


        return result;
    }


    // ============================================================
    // APPLY AIM
    //
    // Solamente debe llamarse mientras Q esta presionado.
    // ============================================================

    inline bool ApplyAim(
        const KFCamera::Snapshot& camera,
        const KFESP::Entry& target
    )
    {
        if (
            camera.controller == 0 ||
            target.pawn == 0
            )
        {
            return false;
        }


        const DesiredRotation desired =
            CalculateDesiredRotation(
                camera,
                target
            );


        if (!desired.valid)
        {
            return false;
        }


        const int currentPitch =
            NormalizeSigned16(
                camera.rotation.pitch
            );


        const int currentYaw =
            NormalizeSigned16(
                camera.rotation.yaw
            );


        // --------------------------------------------------------
        // SUAVIZADO
        // --------------------------------------------------------

        int nextYaw =
            SmoothRotation(
                currentYaw,
                desired.yaw
            );


        int nextPitch =
            SmoothRotation(
                currentPitch,
                desired.pitch
            );


        nextPitch =
            ClampPitch(
                nextPitch
            );


        // --------------------------------------------------------
        // UE2 usa los 16 bits bajos del Rotator.
        //
        // Tus lecturas reales lo confirmaron:
        //
        // -604 aparecia como 64932, etc.
        // --------------------------------------------------------

        const int writeYaw =
            nextYaw &
            0xFFFF;


        const int writePitch =
            nextPitch &
            0xFFFF;


        // Escribimos Pitch primero y Yaw despues.
        const bool pitchOk =
            KFMemory::Write(
                camera.controller +
                KFOffsets::Actor::Pitch,
                writePitch
            );


        const bool yawOk =
            KFMemory::Write(
                camera.controller +
                KFOffsets::Actor::Yaw,
                writeYaw
            );


        return
            pitchOk &&
            yawOk;
    }
}