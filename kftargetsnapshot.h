#pragma once

#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <mutex>

#include "kfcamera.h"


namespace KFTargetSnapshot
{
    namespace Config
    {
        constexpr unsigned int ReaderRetries =
            3;


        // Un futuro fire hook nunca debe utilizar un target que el
        // productor dejo de actualizar.
        constexpr ULONGLONG FreshnessMs =
            250;
    }


    enum Flag : uint32_t
    {
        Valid =
            1u << 0,

        VisibilityKnown =
            1u << 1,

        Visible =
            1u << 2,

        Locked =
            1u << 3,

        HealthReliable =
            1u << 4
    };


    struct Snapshot
    {
        bool valid = false;

        uintptr_t pawn = 0;


        KFCamera::Vec3 headWorld;


        bool visibilityKnown = false;

        bool visible = false;

        bool locked = false;


        bool healthReliable = false;

        int health = 0;


        float scoreSq = 0.0f;


        ULONGLONG publishedAt = 0;

        ULONGLONG ageMs = 0;


        uint32_t sequence = 0;
    };


    // ============================================================
    // STATE
    //
    // Readers nunca toman mutex.
    //
    // El mutex existe solamente para serializar writers:
    //
    // OverlayThread
    // MainThread cleanup
    // ============================================================

    inline std::mutex
        gWriterMutex;


    inline std::atomic<uint32_t>
        gSequence{ 0 };


    inline std::atomic<uint32_t>
        gFlags{ 0 };


    inline std::atomic<uintptr_t>
        gPawn{ 0 };


    inline std::atomic<float>
        gHeadX{ 0.0f };


    inline std::atomic<float>
        gHeadY{ 0.0f };


    inline std::atomic<float>
        gHeadZ{ 0.0f };


    inline std::atomic<int>
        gHealth{ 0 };


    inline std::atomic<float>
        gScoreSq{ 0.0f };


    inline std::atomic<ULONGLONG>
        gPublishedAt{ 0 };


    inline std::atomic<unsigned long long>
        gPublishCount{ 0 };


    inline std::atomic<unsigned long long>
        gClearCount{ 0 };


    // ============================================================
    // WRITER INTERNAL
    // ============================================================

    inline void BeginWrite()
    {
        // even -> odd
        gSequence.fetch_add(
            1,
            std::memory_order_acq_rel
        );
    }


    inline void EndWrite()
    {
        // odd -> even
        gSequence.fetch_add(
            1,
            std::memory_order_release
        );
    }


    inline void ClearNoLock()
    {
        BeginWrite();


        gFlags.store(
            0,
            std::memory_order_relaxed
        );


        gPawn.store(
            0,
            std::memory_order_relaxed
        );


        gHeadX.store(
            0.0f,
            std::memory_order_relaxed
        );


        gHeadY.store(
            0.0f,
            std::memory_order_relaxed
        );


        gHeadZ.store(
            0.0f,
            std::memory_order_relaxed
        );


        gHealth.store(
            0,
            std::memory_order_relaxed
        );


        gScoreSq.store(
            0.0f,
            std::memory_order_relaxed
        );


        gPublishedAt.store(
            GetTickCount64(),
            std::memory_order_relaxed
        );


        EndWrite();


        gClearCount.fetch_add(
            1,
            std::memory_order_relaxed
        );
    }


    inline void Clear()
    {
        // Evitar tomar el writer mutex 60 veces por segundo cuando
        // ya estamos vacios.
        if (
            (
                gFlags.load(
                    std::memory_order_acquire
                ) &
                Flag::Valid
            ) == 0
            )
        {
            return;
        }


        std::lock_guard<std::mutex> lock(
            gWriterMutex
        );


        if (
            (
                gFlags.load(
                    std::memory_order_relaxed
                ) &
                Flag::Valid
            ) == 0
            )
        {
            return;
        }


        ClearNoLock();
    }


    // ============================================================
    // PUBLISH
    // ============================================================

    inline void Publish(
        uintptr_t pawn,
        const KFCamera::Vec3& headWorld,
        bool visibilityKnown,
        bool visible,
        bool locked,
        bool healthReliable,
        int health,
        float scoreSq
    )
    {
        if (pawn == 0)
        {
            Clear();

            return;
        }


        uint32_t flags =
            Flag::Valid;


        if (visibilityKnown)
        {
            flags |=
                Flag::VisibilityKnown;
        }


        if (visible)
        {
            flags |=
                Flag::Visible;
        }


        if (locked)
        {
            flags |=
                Flag::Locked;
        }


        if (healthReliable)
        {
            flags |=
                Flag::HealthReliable;
        }


        const ULONGLONG now =
            GetTickCount64();


        std::lock_guard<std::mutex> lock(
            gWriterMutex
        );


        BeginWrite();


        gPawn.store(
            pawn,
            std::memory_order_relaxed
        );


        gHeadX.store(
            headWorld.x,
            std::memory_order_relaxed
        );


        gHeadY.store(
            headWorld.y,
            std::memory_order_relaxed
        );


        gHeadZ.store(
            headWorld.z,
            std::memory_order_relaxed
        );


        gHealth.store(
            health,
            std::memory_order_relaxed
        );


        gScoreSq.store(
            scoreSq,
            std::memory_order_relaxed
        );


        gPublishedAt.store(
            now,
            std::memory_order_relaxed
        );


        // Flags al final del payload.
        gFlags.store(
            flags,
            std::memory_order_relaxed
        );


        EndWrite();


        gPublishCount.fetch_add(
            1,
            std::memory_order_relaxed
        );
    }


    // ============================================================
    // LOCK-FREE READ
    // ============================================================

    inline bool TryReadSnapshot(
        Snapshot& snapshot,
        ULONGLONG maxAgeMs =
            Config::FreshnessMs
    )
    {
        snapshot = {};


        for (
            unsigned int attempt = 0;
            attempt < Config::ReaderRetries;
            ++attempt
            )
        {
            const uint32_t before =
                gSequence.load(
                    std::memory_order_acquire
                );


            if ((before & 1u) != 0)
            {
                continue;
            }


            const uint32_t flags =
                gFlags.load(
                    std::memory_order_relaxed
                );


            const uintptr_t pawn =
                gPawn.load(
                    std::memory_order_relaxed
                );


            const float headX =
                gHeadX.load(
                    std::memory_order_relaxed
                );


            const float headY =
                gHeadY.load(
                    std::memory_order_relaxed
                );


            const float headZ =
                gHeadZ.load(
                    std::memory_order_relaxed
                );


            const int health =
                gHealth.load(
                    std::memory_order_relaxed
                );


            const float scoreSq =
                gScoreSq.load(
                    std::memory_order_relaxed
                );


            const ULONGLONG publishedAt =
                gPublishedAt.load(
                    std::memory_order_relaxed
                );


            const uint32_t after =
                gSequence.load(
                    std::memory_order_acquire
                );


            if (
                before != after ||
                (after & 1u) != 0
                )
            {
                continue;
            }


            if (
                (flags & Flag::Valid) == 0 ||
                pawn == 0
                )
            {
                return false;
            }


            const ULONGLONG now =
                GetTickCount64();


            const ULONGLONG age =
                now >= publishedAt
                    ?
                    now - publishedAt
                    :
                    maxAgeMs + 1;


            if (age > maxAgeMs)
            {
                return false;
            }


            snapshot.valid =
                true;


            snapshot.pawn =
                pawn;


            snapshot.headWorld =
            {
                headX,
                headY,
                headZ
            };


            snapshot.visibilityKnown =
                (
                    flags &
                    Flag::VisibilityKnown
                ) != 0;


            snapshot.visible =
                (
                    flags &
                    Flag::Visible
                ) != 0;


            snapshot.locked =
                (
                    flags &
                    Flag::Locked
                ) != 0;


            snapshot.healthReliable =
                (
                    flags &
                    Flag::HealthReliable
                ) != 0;


            snapshot.health =
                health;


            snapshot.scoreSq =
                scoreSq;


            snapshot.publishedAt =
                publishedAt;


            snapshot.ageMs =
                age;


            snapshot.sequence =
                after;


            return true;
        }


        return false;
    }


    // ============================================================
    // FUTURE SILENT AIM POLICY
    // ============================================================

    inline bool TryReadStrictVisible(
        Snapshot& snapshot,
        ULONGLONG maxAgeMs =
            Config::FreshnessMs
    )
    {
        if (!TryReadSnapshot(
            snapshot,
            maxAgeMs
        ))
        {
            return false;
        }


        return
            snapshot.visibilityKnown &&
            snapshot.visible;
    }


    // ============================================================
    // DEBUG
    // ============================================================

    inline void PrintStatus()
    {
        Snapshot snapshot;


        const bool fresh =
            TryReadSnapshot(
                snapshot
            );


        std::cout
            << "\n========================================\n"
            << "           TARGET SNAPSHOT\n"
            << "========================================\n"
            << "[Fresh]           "
            << (
                fresh
                    ?
                    "YES"
                    :
                    "NO"
            )
            << '\n'
            << "[Publishes]       "
            << gPublishCount.load()
            << '\n'
            << "[Clears]          "
            << gClearCount.load()
            << '\n'
            << "[Sequence]        "
            << gSequence.load()
            << '\n';


        if (fresh)
        {
            std::cout
                << std::hex
                << std::uppercase
                << std::showbase
                << "[Pawn]            "
                << snapshot.pawn
                << '\n'
                << std::dec
                << std::nouppercase
                << std::noshowbase
                << "[Visible known]   "
                << (
                    snapshot.visibilityKnown
                        ?
                        "YES"
                        :
                        "NO"
                )
                << '\n'
                << "[Visible]         "
                << (
                    snapshot.visible
                        ?
                        "YES"
                        :
                        "NO"
                )
                << '\n'
                << "[Silent eligible] "
                << (
                    snapshot.visibilityKnown &&
                    snapshot.visible
                        ?
                        "YES"
                        :
                        "NO"
                )
                << '\n'
                << "[Locked]          "
                << (
                    snapshot.locked
                        ?
                        "YES"
                        :
                        "NO"
                )
                << '\n'
                << "[Age]             "
                << snapshot.ageMs
                << " ms\n"
                << std::fixed
                << std::setprecision(2)
                << "[Head]            "
                << snapshot.headWorld.x
                << ", "
                << snapshot.headWorld.y
                << ", "
                << snapshot.headWorld.z
                << '\n';
        }


        std::cout
            << "========================================\n\n";
    }
}