#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "kfesp.h"


namespace KFTargeting
{
    // ============================================================
    // CONFIGURACION
    // ============================================================

    namespace Config
    {
        // Radio normal para ADQUIRIR un target.
        constexpr float AimRadiusRatio =
            0.22f;


        constexpr float MinimumAimRadius =
            45.0f;


        // Una vez bloqueado permitimos que el target se aleje
        // un poco mas del centro antes de liberarlo.
        //
        // Esto evita target switching / jitter.
        constexpr float LockRadiusMultiplier =
            1.40f;
    }


    // ============================================================
    // GLOBAL TARGET LOCK
    // ============================================================

    inline uintptr_t
        gLockedPawn = 0;


    // ============================================================
    // RESULTADO
    // ============================================================

    struct Selection
    {
        bool found = false;

        bool locked = false;


        size_t entryIndex =
            std::numeric_limits<size_t>::max();


        uintptr_t pawn = 0;


        float centerX = 0.0f;

        float centerY = 0.0f;


        float radius = 0.0f;

        float radiusSq = 0.0f;


        float lockRadius = 0.0f;

        float lockRadiusSq = 0.0f;


        float scoreSq =
            std::numeric_limits<float>::max();
    };


    // ============================================================
    // RESET
    // ============================================================

    inline void ResetLock()
    {
        gLockedPawn = 0;
    }


    // ============================================================
    // AIM RADIUS
    // ============================================================

    inline float GetAimRadius(
        const KFCamera::Viewport& viewport
    )
    {
        const int smallestSide =
            viewport.width <
            viewport.height
                ?
                viewport.width
                :
                viewport.height;


        float radius =
            static_cast<float>(
                smallestSide
            ) *
            Config::AimRadiusRatio;


        if (
            radius <
            Config::MinimumAimRadius
            )
        {
            radius =
                Config::MinimumAimRadius;
        }


        return radius;
    }


    // ============================================================
    // CROSSHAIR DISTANCE SQUARED
    //
    // Sin sqrt.
    // ============================================================

    inline float GetCrosshairDistanceSq(
        const KFESP::Entry& entry,
        float centerX,
        float centerY
    )
    {
        const float dx =
            entry.headScreen.x -
            centerX;


        const float dy =
            entry.headScreen.y -
            centerY;


        return
            dx * dx +
            dy * dy;
    }


    // ============================================================
    // VALIDACION BARATA
    // ============================================================

    inline bool IsValidCandidate(
        const KFESP::Entry& entry
    )
    {
        // KFESP cachea exclusivamente Relation::Enemy.


        if (!entry.headProjected)
        {
            return false;
        }


        // WorldToScreen ya descarta lo que queda detras,
        // pero conservamos la comprobacion explicita.
        if (
            entry.headScreen.depth <=
            1.0f
            )
        {
            return false;
        }


        if (!entry.headScreen.onScreen)
        {
            return false;
        }


        // Si Health es confiable y <= 0,
        // no deberia ser candidato.
        if (
            entry.healthReliable &&
            entry.health <= 0
            )
        {
            return false;
        }


        return true;
    }


    // ============================================================
    // PREPARAR SELECTION
    // ============================================================

    inline Selection CreateSelectionBase(
        const KFCamera::Viewport& viewport
    )
    {
        Selection result;


        result.centerX =
            static_cast<float>(
                viewport.width
            ) *
            0.5f;


        result.centerY =
            static_cast<float>(
                viewport.height
            ) *
            0.5f;


        result.radius =
            GetAimRadius(
                viewport
            );


        result.radiusSq =
            result.radius *
            result.radius;


        result.lockRadius =
            result.radius *
            Config::LockRadiusMultiplier;


        result.lockRadiusSq =
            result.lockRadius *
            result.lockRadius;


        return result;
    }


    // ============================================================
    // FILL
    // ============================================================

    inline void SetSelectedTarget(
        Selection& result,
        size_t index,
        const KFESP::Entry& entry,
        float scoreSq,
        bool locked
    )
    {
        result.found =
            true;


        result.locked =
            locked;


        result.entryIndex =
            index;


        result.pawn =
            entry.pawn;


        result.scoreSq =
            scoreSq;
    }


    // ============================================================
    // INTENTAR CONSERVAR LOCK
    // ============================================================

    inline bool TryLockedTarget(
        const std::vector<KFESP::Entry>& entries,
        Selection& result
    )
    {
        if (gLockedPawn == 0)
        {
            return false;
        }


        for (
            size_t i = 0;
            i < entries.size();
            ++i
            )
        {
            const KFESP::Entry& entry =
                entries[i];


            if (
                entry.pawn !=
                gLockedPawn
                )
            {
                continue;
            }


            // El Pawn existe pero ya no es usable.
            if (!IsValidCandidate(
                entry
            ))
            {
                return false;
            }


            const float scoreSq =
                GetCrosshairDistanceSq(
                    entry,
                    result.centerX,
                    result.centerY
                );


            // Hysteresis:
            //
            // Adquisicion:
            //      radius
            //
            // Mantener target:
            //      radius * 1.40
            //
            // Evita perder el lock por movimientos pequenos.
            if (
                scoreSq >
                result.lockRadiusSq
                )
            {
                return false;
            }


            SetSelectedTarget(
                result,
                i,
                entry,
                scoreSq,
                true
            );


            return true;
        }


        return false;
    }


    // ============================================================
    // ADQUIRIR TARGET NUEVO
    // ============================================================

    inline void AcquireBestTarget(
        const std::vector<KFESP::Entry>& entries,
        Selection& result
    )
    {
        for (
            size_t i = 0;
            i < entries.size();
            ++i
            )
        {
            const KFESP::Entry& entry =
                entries[i];


            if (!IsValidCandidate(
                entry
            ))
            {
                continue;
            }


            const float scoreSq =
                GetCrosshairDistanceSq(
                    entry,
                    result.centerX,
                    result.centerY
                );


            // Para adquirir uno nuevo usamos solamente
            // el AimFOV normal.
            if (
                scoreSq >
                result.radiusSq
                )
            {
                continue;
            }


            // ====================================================
            // FUTURO LOS
            //
            // Aqui ira:
            //
            // if (!HasLineOfSight(entry.pawn))
            //     continue;
            // ====================================================


            if (
                !result.found ||
                scoreSq <
                result.scoreSq
                )
            {
                SetSelectedTarget(
                    result,
                    i,
                    entry,
                    scoreSq,
                    false
                );
            }
        }
    }


    // ============================================================
    // FIND TARGET
    //
    // lockRequested = Q mantenido
    // ============================================================

    inline Selection FindBestTarget(
        const std::vector<KFESP::Entry>& entries,
        const KFCamera::Viewport& viewport,
        bool lockRequested
    )
    {
        Selection result =
            CreateSelectionBase(
                viewport
            );


        // ========================================================
        // Q RELEASE
        //
        // Sin Q no conservamos target anterior.
        // Solo mostramos el mejor candidato visual.
        // ========================================================

        if (!lockRequested)
        {
            ResetLock();


            AcquireBestTarget(
                entries,
                result
            );


            return result;
        }


        // ========================================================
        // Q HOLD
        //
        // Intentar conservar Pawn anterior.
        // ========================================================

        if (TryLockedTarget(
            entries,
            result
        ))
        {
            return result;
        }


        // Perdimos el target anterior.
        ResetLock();


        // Buscar uno nuevo dentro del FOV normal.
        AcquireBestTarget(
            entries,
            result
        );


        if (result.found)
        {
            gLockedPawn =
                result.pawn;


            result.locked =
                true;
        }


        return result;
    }


    inline bool IsSelected(
        const Selection& selection,
        const KFESP::Entry& entry
    )
    {
        return
            selection.found &&
            selection.pawn != 0 &&
            selection.pawn ==
                entry.pawn;
    }
}