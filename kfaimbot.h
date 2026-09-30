#pragma once

#include <Windows.h>

#include <cmath>
#include <cstdint>

#include "kftargeting.h"


namespace KFAimbot
{
    // ============================================================
    // CONFIG
    // ============================================================

    namespace Config
    {
        constexpr float Pi =
            3.14159265358979323846f;


        constexpr float UnrealRotatorScale =
            65536.0f /
            (2.0f * Pi);


        // --------------------------------------------------------
        // CAMERA SMOOTHING
        //
        // 0.0 = no movement
        // 1.0 = instant snap
        //
        // Conservamos 0.20 porque visualmente ya funciona bien.
        // El predictor compensara parte del retraso que introduce.
        // --------------------------------------------------------

        constexpr float SmoothFactor =
            0.20f;


        constexpr int MinPitch =
            -16000;


        constexpr int MaxPitch =
            16000;


        constexpr float MinimumHorizontalDistance =
            5.0f;


        // ========================================================
        // MOVEMENT PREDICTION
        //
        // No es prediccion balistica.
        //
        // Para hitscan adelantamos ligeramente el punto de aim
        // para compensar el lag generado por SmoothFactor.
        // ========================================================

        constexpr float VelocityBlend =
            0.45f;


        // Si Q estuvo suelto / cambiamos de estado durante mucho
        // tiempo, descartamos la muestra anterior.
        constexpr float MaxSampleSeconds =
            0.120f;


        // Safety contra divisiones por un delta demasiado pequeño.
        constexpr float MinSampleSeconds =
            0.004f;


        // Multiplicador del retraso teorico del smoothing.
        //
        // SmoothFactor 0.20:
        //
        // (1 - 0.20) / 0.20 = ~4 frames
        //
        // A 60 FPS:
        // 4 * 16.6 ms ~= 66 ms.
        constexpr float PredictionMultiplier =
            1.05f;


        // Nunca predecimos demasiado hacia adelante.
        constexpr float MaxPredictionSeconds =
            0.085f;


        // Tampoco permitimos un desplazamiento enorme por una
        // muestra mala, teleport o Pawn reciclado.
        constexpr float MaxPredictionDistance =
            100.0f;


        // Sanity para velocidades imposibles / lecturas corruptas.
        constexpr float MaxReasonableTargetSpeed =
            2500.0f;
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
    // AIM SOLUTION
    //
    // Separar:
    //
    //     calcular hacia donde apuntar
    //
    // de:
    //
    //     escribir la rotacion de la camara
    //
    // El futuro Silent Aim consumira una solucion sin necesidad
    // de llamar a la ruta que modifica Pitch/Yaw del controller.
    // ============================================================

    struct AimSolution
    {
        uintptr_t pawn = 0;

        KFCamera::Vec3 aimPoint;

        DesiredRotation desired;

        bool valid = false;
    };


    // ============================================================
    // MOTION STATE
    //
    // Solo mantenemos el target actualmente seguido.
    //
    // Esto es mas barato que un unordered_map porque el aimbot
    // solamente puede apuntar a un Pawn a la vez.
    // ============================================================

    struct MotionState
    {
        uintptr_t pawn = 0;


        KFCamera::Vec3 previousPosition{};


        KFCamera::Vec3 velocity{};


        ULONGLONG previousTick = 0;


        bool initialized = false;
    };


    inline MotionState
        gMotionState;


    // Debug / tuning.
    inline float
        gLastPredictionSeconds = 0.0f;


    inline float
        gLastPredictionDistance = 0.0f;


    // ============================================================
    // HELPERS
    // ============================================================

    inline float ClampFloat(
        float value,
        float minimum,
        float maximum
    )
    {
        if (value < minimum)
        {
            return minimum;
        }


        if (value > maximum)
        {
            return maximum;
        }


        return value;
    }


    inline float LengthSquared(
        const KFCamera::Vec3& value
    )
    {
        return
            value.x * value.x +
            value.y * value.y +
            value.z * value.z;
    }


    inline bool IsFinite(
        const KFCamera::Vec3& value
    )
    {
        return
            std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }


    inline void ResetMotionTracking()
    {
        gMotionState = {};

        gLastPredictionSeconds =
            0.0f;

        gLastPredictionDistance =
            0.0f;
    }


    inline void InitializeMotionTracking(
        uintptr_t pawn,
        const KFCamera::Vec3& position,
        ULONGLONG now
    )
    {
        gMotionState = {};


        gMotionState.pawn =
            pawn;


        gMotionState.previousPosition =
            position;


        gMotionState.previousTick =
            now;


        gMotionState.initialized =
            true;


        gLastPredictionSeconds =
            0.0f;


        gLastPredictionDistance =
            0.0f;
    }


    // ============================================================
    // PREDICT AIM POINT
    //
    // target.headWorld sigue siendo actualmente:
    //
    // Pawn XYZ + EyeHeight.
    //
    // Cuando más adelante recuperemos HeadBone de forma segura,
    // este predictor funcionara exactamente igual sobre el bone.
    // ============================================================

    inline KFCamera::Vec3 BuildPredictedAimPoint(
        const KFESP::Entry& target
    )
    {
        const KFCamera::Vec3 current =
            target.headWorld;


        gLastPredictionSeconds =
            0.0f;


        gLastPredictionDistance =
            0.0f;


        if (
            target.pawn == 0 ||
            !IsFinite(current)
            )
        {
            ResetMotionTracking();

            return current;
        }


        const ULONGLONG now =
            GetTickCount64();


        // Nuevo target:
        // todavía no tenemos velocidad.
        if (
            !gMotionState.initialized ||
            gMotionState.pawn !=
                target.pawn
            )
        {
            InitializeMotionTracking(
                target.pawn,
                current,
                now
            );


            return current;
        }


        const ULONGLONG elapsedMs =
            now -
            gMotionState.previousTick;


        if (elapsedMs == 0)
        {
            return current;
        }


        const float deltaSeconds =
            static_cast<float>(
                elapsedMs
            ) *
            0.001f;


        // Q pudo haberse soltado o el juego pudo congelarse.
        //
        // No usamos una muestra vieja para calcular velocidad.
        if (
            deltaSeconds <
                Config::MinSampleSeconds ||
            deltaSeconds >
                Config::MaxSampleSeconds
            )
        {
            InitializeMotionTracking(
                target.pawn,
                current,
                now
            );


            return current;
        }


        // ========================================================
        // RAW VELOCITY
        // ========================================================

        KFCamera::Vec3 rawVelocity
        {
            (
                current.x -
                gMotionState.previousPosition.x
            ) /
            deltaSeconds,

            (
                current.y -
                gMotionState.previousPosition.y
            ) /
            deltaSeconds,

            (
                current.z -
                gMotionState.previousPosition.z
            ) /
            deltaSeconds
        };


        if (!IsFinite(rawVelocity))
        {
            InitializeMotionTracking(
                target.pawn,
                current,
                now
            );


            return current;
        }


        const float rawSpeedSq =
            LengthSquared(
                rawVelocity
            );


        const float maxSpeedSq =
            Config::MaxReasonableTargetSpeed *
            Config::MaxReasonableTargetSpeed;


        // Teleport / pointer reciclado / lectura mala.
        if (rawSpeedSq > maxSpeedSq)
        {
            InitializeMotionTracking(
                target.pawn,
                current,
                now
            );


            return current;
        }


        // ========================================================
        // VELOCITY LOW-PASS FILTER
        //
        // Reduce jitter de la animacion / lectura de posiciones.
        // ========================================================

        const float blend =
            Config::VelocityBlend;


        const float inverseBlend =
            1.0f -
            blend;


        gMotionState.velocity.x =
            gMotionState.velocity.x *
                inverseBlend +
            rawVelocity.x *
                blend;


        gMotionState.velocity.y =
            gMotionState.velocity.y *
                inverseBlend +
            rawVelocity.y *
                blend;


        gMotionState.velocity.z =
            gMotionState.velocity.z *
                inverseBlend +
            rawVelocity.z *
                blend;


        // Actualizamos muestra para el siguiente frame.
        gMotionState.previousPosition =
            current;


        gMotionState.previousTick =
            now;


        // ========================================================
        // ESTIMAR EL RETRASO QUE INTRODUCE EL SMOOTHING
        //
        // Aproximacion de first-order smoothing:
        //
        // lagFrames ~= (1-alpha) / alpha
        // ========================================================

        const float smoothingLagFrames =
            (
                1.0f -
                Config::SmoothFactor
            ) /
            Config::SmoothFactor;


        float predictionSeconds =
            deltaSeconds *
            smoothingLagFrames *
            Config::PredictionMultiplier;


        predictionSeconds =
            ClampFloat(
                predictionSeconds,
                0.0f,
                Config::MaxPredictionSeconds
            );


        // ========================================================
        // PREDICTION VECTOR
        // ========================================================

        KFCamera::Vec3 lead
        {
            gMotionState.velocity.x *
                predictionSeconds,

            gMotionState.velocity.y *
                predictionSeconds,

            gMotionState.velocity.z *
                predictionSeconds
        };


        float leadDistanceSq =
            LengthSquared(
                lead
            );


        const float maxLead =
            Config::MaxPredictionDistance;


        const float maxLeadSq =
            maxLead *
            maxLead;


        // Clamp espacial.
        if (
            leadDistanceSq >
                maxLeadSq &&
            leadDistanceSq >
                0.0001f
            )
        {
            const float leadDistance =
                std::sqrt(
                    leadDistanceSq
                );


            const float scale =
                maxLead /
                leadDistance;


            lead.x *=
                scale;


            lead.y *=
                scale;


            lead.z *=
                scale;


            leadDistanceSq =
                maxLeadSq;
        }


        KFCamera::Vec3 predicted
        {
            current.x +
                lead.x,

            current.y +
                lead.y,

            current.z +
                lead.z
        };


        if (!IsFinite(predicted))
        {
            return current;
        }


        gLastPredictionSeconds =
            predictionSeconds;


        gLastPredictionDistance =
            std::sqrt(
                leadDistanceSq
            );


        return predicted;
    }


    // ============================================================
    // NORMALIZE ROTATOR
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
    // DESIRED ROTATION TO WORLD POINT
    // ============================================================

    inline DesiredRotation CalculateDesiredRotationToPoint(
        const KFCamera::Snapshot& camera,
        const KFCamera::Vec3& aimPoint
    )
    {
        DesiredRotation result;


        const float dx =
            aimPoint.x -
            camera.eyeLocation.x;


        const float dy =
            aimPoint.y -
            camera.eyeLocation.y;


        const float dz =
            aimPoint.z -
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
    // COMPATIBILITY
    // ============================================================

    inline DesiredRotation CalculateDesiredRotation(
        const KFCamera::Snapshot& camera,
        const KFESP::Entry& target
    )
    {
        return
            CalculateDesiredRotationToPoint(
                camera,
                target.headWorld
            );
    }


    // ============================================================
    // BUILD DIRECT SOLUTION
    //
    // No smoothing.
    // No memory writes.
    // No prediction state.
    // ============================================================

    inline AimSolution BuildAimSolutionToPoint(
        const KFCamera::Snapshot& camera,
        uintptr_t pawn,
        const KFCamera::Vec3& aimPoint
    )
    {
        AimSolution result;


        if (
            camera.controller == 0 ||
            pawn == 0
            )
        {
            return result;
        }


        result.pawn =
            pawn;


        result.aimPoint =
            aimPoint;


        result.desired =
            CalculateDesiredRotationToPoint(
                camera,
                aimPoint
            );


        result.valid =
            result.desired.valid;


        return result;
    }


    // ============================================================
    // BUILD NORMAL AIM SOLUTION
    //
    // Conserva exactamente la prediccion actual.
    // ============================================================

    inline AimSolution BuildAimSolution(
        const KFCamera::Snapshot& camera,
        const KFESP::Entry& target
    )
    {
        if (
            camera.controller == 0 ||
            target.pawn == 0 ||
            (
                target.visibilityKnown &&
                !target.visible
            )
            )
        {
            ResetMotionTracking();

            return {};
        }


        const KFCamera::Vec3 aimPoint =
            BuildPredictedAimPoint(
                target
            );


        return
            BuildAimSolutionToPoint(
                camera,
                target.pawn,
                aimPoint
            );
    }


    // ============================================================
    // APPLY SOLUTION TO CAMERA
    //
    // Esta es la parte que Silent Aim NO utilizara.
    // ============================================================

    inline bool ApplyAimSolution(
        const KFCamera::Snapshot& camera,
        const AimSolution& solution
    )
    {
        if (
            camera.controller == 0 ||
            !solution.valid
            )
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


        int nextYaw =
            SmoothRotation(
                currentYaw,
                solution.desired.yaw
            );


        int nextPitch =
            SmoothRotation(
                currentPitch,
                solution.desired.pitch
            );


        nextPitch =
            ClampPitch(
                nextPitch
            );


        const int writeYaw =
            nextYaw &
            0xFFFF;


        const int writePitch =
            nextPitch &
            0xFFFF;


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


    // ============================================================
    // COMPATIBILITY: NORMAL SMOOTH AIM
    // ============================================================

    inline bool ApplyAim(
        const KFCamera::Snapshot& camera,
        const KFESP::Entry& target
    )
    {
        const AimSolution solution =
            BuildAimSolution(
                camera,
                target
            );


        return
            ApplyAimSolution(
                camera,
                solution
            );
    }
}