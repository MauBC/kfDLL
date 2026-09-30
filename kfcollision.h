#pragma once

#include <Windows.h>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "kfcamera.h"
#include "kfgamethread.h"
#include "kfmemory.h"
#include "kfoffsets.h"
#include "kfplayer.h"


namespace KFCollision
{
#if !defined(_M_IX86)
#error KFCollision currently requires Win32/x86.
#endif


    namespace Config
    {
        constexpr unsigned long TraceFlags =
            0x286;


        // Presupuesto conservador:
        //
        // 2 traces por frame + round-robin.
        //
        // Con 6 enemigos:
        //   60 FPS -> cada enemigo ~50 ms
        //   30 FPS -> cada enemigo ~100 ms
        //
        // Evitamos volver al antiguo burst de 24 traces/frame.
        constexpr size_t MaxTracesPerFrame =
            2;


        // Tiempo maximo de vida de un resultado LOS.
        constexpr ULONGLONG VisibilityTtlMs =
            500;


        constexpr char SingleLineCheckExport[] =
            "?SingleLineCheck@ULevel@@UAEHAAUFCheckResult@@PAVAActor@@ABVFVector@@2KV4@@Z";
    }


    // ============================================================
    // FCHECKRESULT
    //
    // SDK:
    //
    // FIteratorList
    //   +00 Next
    //
    // FIteratorActorList
    //   +04 Actor
    //
    // FCheckResult
    //   +08 Location
    //   +14 Normal
    //   +20 Primitive
    //   +24 Time
    //   +28 Item
    //
    // sizeof runtime = 0x30 (Engine.dll copia 12 DWORD)
    // ============================================================

    struct FCheckResultNative
    {
        void* next = nullptr;

        void* actor = nullptr;


        float location[3]
        {
            0.0f,
            0.0f,
            0.0f
        };


        float normal[3]
        {
            0.0f,
            0.0f,
            0.0f
        };


        void* primitive = nullptr;


        float time =
            1.0f;


        int item =
            -1;


        // Engine.dll ULevel::SingleLineCheck copia 12 DWORD
        // mediante REP MOVSD:
        //
        //     ECX = 0x0C
        //
        // 12 * 4 = 0x30 bytes.
        //
        // El SDK antiguo termina en Item (+0x28), pero el runtime
        // contiene un DWORD adicional en +0x2C.
        //
        // La semantica exacta de este campo todavia esta pendiente
        // de identificar; por ahora solo respetamos el ABI real.
        unsigned int unknown2C =
            0;
    };


    static_assert(
        sizeof(FCheckResultNative) == 0x30,
        "Unexpected runtime FCheckResult layout."
    );


    // ============================================================
    // TARGET
    // ============================================================

    struct TargetPoint
    {
        uintptr_t pawn = 0;

        KFCamera::Vec3 headWorld;
    };


    struct VisibilityRecord
    {
        bool visible = false;

        ULONGLONG updatedAt = 0;
    };


    // ============================================================
    // NATIVE FUNCTION
    // ============================================================

    using SingleLineCheckFn =
        int(__thiscall*)(
            void* level,
            FCheckResultNative& result,
            void* sourceActor,
            const KFCamera::Vec3& end,
            const KFCamera::Vec3& start,
            unsigned long traceFlags,
            KFCamera::Vec3 extent
        );


    // ============================================================
    // STATE
    // ============================================================

    inline std::atomic_bool
        gEnabled{ false };


    inline std::atomic<unsigned long long>
        gTraceCount{ 0 };


    inline std::atomic<unsigned long long>
        gClearCount{ 0 };


    inline std::atomic<unsigned long long>
        gBlockedCount{ 0 };


    inline std::atomic<unsigned long long>
        gFaultCount{ 0 };



    // Round-robin para repartir LOS entre todos los targets.
    inline std::atomic<size_t>
        gTraceCursor{ 0 };


    inline std::mutex
        gTargetsMutex;


    inline std::vector<TargetPoint>
        gTargets;


    inline std::mutex
        gVisibilityMutex;


    inline std::unordered_map<
        uintptr_t,
        VisibilityRecord
    >
        gVisibility;


    inline SingleLineCheckFn
        gSingleLineCheck =
            nullptr;


    // ============================================================
    // RESOLVE
    // ============================================================

    inline SingleLineCheckFn ResolveSingleLineCheck()
    {
        if (gSingleLineCheck != nullptr)
        {
            return
                gSingleLineCheck;
        }


        HMODULE engine =
            GetModuleHandleA(
                "Engine.dll"
            );


        if (engine == nullptr)
        {
            return nullptr;
        }


        FARPROC address =
            GetProcAddress(
                engine,
                Config::SingleLineCheckExport
            );


        if (address == nullptr)
        {
            return nullptr;
        }


        gSingleLineCheck =
            reinterpret_cast<
                SingleLineCheckFn
            >(
                address
            );


        return
            gSingleLineCheck;
    }


    // ============================================================
    // SAFE NATIVE INVOCATION
    //
    // __try queda aislado en una funcion sin vector/mutex/RAII.
    // Evita C2712 en MSVC.
    // ============================================================

    inline bool TrySingleLineCheck(
        SingleLineCheckFn function,
        uintptr_t level,
        uintptr_t sourceActor,
        const KFCamera::Vec3& traceEnd,
        const KFCamera::Vec3& traceStart,
        bool& visible
    )
    {
        visible =
            false;


        if (
            function == nullptr ||
            level == 0 ||
            sourceActor == 0
            )
        {
            return false;
        }


        FCheckResultNative result;


        result.next =
            nullptr;


        result.actor =
            nullptr;


        result.location[0] =
            0.0f;

        result.location[1] =
            0.0f;

        result.location[2] =
            0.0f;


        result.normal[0] =
            0.0f;

        result.normal[1] =
            0.0f;

        result.normal[2] =
            0.0f;


        result.primitive =
            nullptr;


        result.time =
            1.0f;


        result.item =
            -1;


        KFCamera::Vec3 extent
        {
            0.0f,
            0.0f,
            0.0f
        };


        int nativeResult =
            0;


        __try
        {
            nativeResult =
                function(
                    reinterpret_cast<void*>(
                        level
                    ),

                    result,

                    reinterpret_cast<void*>(
                        sourceActor
                    ),

                    traceEnd,

                    traceStart,

                    Config::TraceFlags,

                    extent
                );
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }


        visible =
            nativeResult > 0;


        return true;
    }


    // ============================================================
    // RESET
    // ============================================================

    inline void Clear()
    {
        {
            std::lock_guard<std::mutex> lock(
                gTargetsMutex
            );


            gTargets.clear();
        }


        {
            std::lock_guard<std::mutex> lock(
                gVisibilityMutex
            );


            gVisibility.clear();
        }


        gTraceCursor.store(
            0,
            std::memory_order_release
        );
    }


    // ============================================================
    // ENABLE
    // ============================================================

    inline void SetEnabled(
        bool enabled
    )
    {
        gEnabled.store(
            enabled,
            std::memory_order_release
        );


        if (!enabled)
        {
            Clear();
        }
    }


    inline bool IsEnabled()
    {
        return
            gEnabled.load(
                std::memory_order_acquire
            );
    }


    // ============================================================
    // OVERLAY -> GAME THREAD
    // ============================================================

    inline bool ContainsTargetPawn(
        const std::vector<TargetPoint>& targets,
        uintptr_t pawn
    )
    {
        if (pawn == 0)
        {
            return false;
        }


        for (
            const TargetPoint& target :
            targets
            )
        {
            if (
                target.pawn ==
                pawn
                )
            {
                return true;
            }
        }


        return false;
    }


    inline void PublishTargets(
        const std::vector<TargetPoint>& targets
    )
    {
        if (!IsEnabled())
        {
            return;
        }


        // --------------------------------------------------------
        // OVERLAY -> GAME THREAD
        // --------------------------------------------------------

        {
            std::lock_guard<std::mutex> lock(
                gTargetsMutex
            );


            gTargets =
                targets;
        }


        // --------------------------------------------------------
        // PRUNE VISIBILITY CACHE
        //
        // Un VisibilityRecord solamente tiene sentido mientras
        // ese Pawn siga siendo publicado como target LOS.
        //
        // Esto evita:
        //
        // - Pawns muertos;
        // - actores que desaparecieron;
        // - direcciones recicladas;
        // - crecimiento historico de gVisibility.
        //
        // Los locks se toman POR SEPARADO para no introducir
        // lock-order inversions con OnGameFrame().
        // --------------------------------------------------------

        {
            std::lock_guard<std::mutex> lock(
                gVisibilityMutex
            );


            for (
                auto iterator =
                    gVisibility.begin();
                iterator !=
                    gVisibility.end();
                )
            {
                if (
                    !ContainsTargetPawn(
                        targets,
                        iterator->first
                    )
                    )
                {
                    iterator =
                        gVisibility.erase(
                            iterator
                        );
                }
                else
                {
                    ++iterator;
                }
            }
        }
    }


    inline void ForgetPawn(
        uintptr_t pawn
    )
    {
        if (pawn == 0)
        {
            return;
        }


        // Quitar del conjunto que consume el game thread.
        {
            std::lock_guard<std::mutex> lock(
                gTargetsMutex
            );


            gTargets.erase(
                std::remove_if(
                    gTargets.begin(),
                    gTargets.end(),

                    [pawn](
                        const TargetPoint& target
                    )
                    {
                        return
                            target.pawn ==
                            pawn;
                    }
                ),

                gTargets.end()
            );
        }


        // Quitar cualquier resultado LOS historico.
        {
            std::lock_guard<std::mutex> lock(
                gVisibilityMutex
            );


            gVisibility.erase(
                pawn
            );
        }
    }


    inline void ClearTargets()
    {
        {
            std::lock_guard<std::mutex> lock(
                gTargetsMutex
            );


            gTargets.clear();
        }


        // Sin targets tampoco necesitamos resultados LOS derivados.
        {
            std::lock_guard<std::mutex> lock(
                gVisibilityMutex
            );


            gVisibility.clear();
        }


        gTraceCursor.store(
            0,
            std::memory_order_release
        );
    }


    // ============================================================
    // GAME THREAD -> OVERLAY
    // ============================================================

    inline bool QueryVisibility(
        uintptr_t pawn,
        bool& known
    )
    {
        known =
            false;


        if (
            pawn == 0 ||
            !IsEnabled()
            )
        {
            return false;
        }


        const ULONGLONG now =
            GetTickCount64();


        std::lock_guard<std::mutex> lock(
            gVisibilityMutex
        );


        const auto iterator =
            gVisibility.find(
                pawn
            );


        if (
            iterator ==
            gVisibility.end()
            )
        {
            return false;
        }


        if (
            now -
                iterator->second.updatedAt >
            Config::VisibilityTtlMs
            )
        {
            return false;
        }


        known =
            true;


        return
            iterator->second.visible;
    }


    // ============================================================
    // GAME THREAD
    // ============================================================

    inline void OnGameFrame()
    {
        if (
            !IsEnabled() ||
            !KFGameThread::IsGameThread()
            )
        {
            return;
        }


        std::vector<TargetPoint>
            targets;


        {
            std::lock_guard<std::mutex> lock(
                gTargetsMutex
            );


            targets =
                gTargets;
        }


        if (targets.empty())
        {
            return;
        }


        KFCamera::Snapshot camera;


        if (!KFCamera::ReadSnapshot(
            camera
        ))
        {
            return;
        }


        if (
            camera.pawn == 0 ||
            !camera.hasPawnLocation ||
            !camera.hasEyeHeight
            )
        {
            return;
        }


        uintptr_t level =
            0;


        if (!KFMemory::Read(
            camera.pawn +
                KFOffsets::Pawn::Level,
            level
        ))
        {
            return;
        }


        if (level == 0)
        {
            return;
        }


        const SingleLineCheckFn function =
            ResolveSingleLineCheck();


        if (function == nullptr)
        {
            return;
        }


        const size_t total =
            targets.size();


        const size_t traceCount =
            std::min(
                total,
                Config::MaxTracesPerFrame
            );


        size_t start =
            gTraceCursor.load(
                std::memory_order_acquire
            );


        if (start >= total)
        {
            start =
                0;
        }


        const ULONGLONG now =
            GetTickCount64();


        std::vector<
            std::pair<
                uintptr_t,
                VisibilityRecord
            >
        >
            updates;


        updates.reserve(
            traceCount
        );


        for (
            size_t offset = 0;
            offset < traceCount;
            ++offset
            )
        {
            const size_t index =
                (
                    start +
                    offset
                ) %
                total;


            const TargetPoint& target =
                targets[index];


            if (target.pawn == 0)
            {
                continue;
            }


            bool visible =
                false;


            const bool callOk =
                TrySingleLineCheck(
                    function,

                    level,

                    camera.pawn,

                    target.headWorld,

                    camera.eyeLocation,

                    visible
                );


            if (!callOk)
            {
                gFaultCount.fetch_add(
                    1,
                    std::memory_order_relaxed
                );


                continue;
            }


            gTraceCount.fetch_add(
                1,
                std::memory_order_relaxed
            );


            if (visible)
            {
                gClearCount.fetch_add(
                    1,
                    std::memory_order_relaxed
                );
            }
            else
            {
                gBlockedCount.fetch_add(
                    1,
                    std::memory_order_relaxed
                );
            }


            updates.emplace_back(
                target.pawn,

                VisibilityRecord
                {
                    visible,
                    now
                }
            );
        }


        gTraceCursor.store(
            (
                start +
                traceCount
            ) %
            total,
            std::memory_order_release
        );


        if (updates.empty())
        {
            return;
        }


        std::lock_guard<std::mutex> lock(
            gVisibilityMutex
        );


        for (
            const auto& update :
            updates
            )
        {
            gVisibility[
                update.first
            ] =
                update.second;
        }
    }


    // ============================================================
    // DEBUG
    // ============================================================

    inline void PrintStatus()
    {
        size_t targetCount =
            0;


        size_t visibilityCount =
            0;


        {
            std::lock_guard<std::mutex> lock(
                gTargetsMutex
            );


            targetCount =
                gTargets.size();
        }


        {
            std::lock_guard<std::mutex> lock(
                gVisibilityMutex
            );


            visibilityCount =
                gVisibility.size();
        }


        std::cout
            << "\n========================================\n"
            << "            GAME THREAD / LOS\n"
            << "========================================\n";


        std::cout
            << "[Hook Installed] "
            << (
                KFGameThread::IsInstalled()
                    ?
                    "YES"
                    :
                    "NO"
                )
            << '\n';


        std::cout
            << "[Bound Thread]   "
            << KFGameThread::GameThreadId()
            << '\n';


        std::cout
            << "[Frames]         "
            << KFGameThread::FrameCount()
            << '\n';


        std::cout
            << "[LOS Enabled]    "
            << (
                IsEnabled()
                    ?
                    "YES"
                    :
                    "NO"
                )
            << '\n';


        std::cout
            << "[Targets]        "
            << targetCount
            << '\n';


        std::cout
            << "[Cached LOS]     "
            << visibilityCount
            << '\n';


        std::cout
            << "[Trace total]    "
            << gTraceCount.load()
            << '\n';


        std::cout
            << "[Trace CLEAR]    "
            << gClearCount.load()
            << '\n';


        std::cout
            << "[Trace BLOCKED]  "
            << gBlockedCount.load()
            << '\n';


        std::cout
            << "[Trace faults]   "
            << gFaultCount.load()
            << '\n';


        std::cout
            << "[Callback faults]"
            << ' '
            << KFGameThread::CallbackFaultCount()
            << '\n';



        std::cout
            << "========================================\n\n";
    }
}