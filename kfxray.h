#pragma once

#include <Windows.h>
#include <d3d9.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <iostream>
#include <mutex>

#include "kfgamethread.h"


namespace KFXRay
{
#if !defined(_M_IX86)
#error KFXRay currently requires Win32/x86.
#endif


    namespace Config
    {
        // Limite deliberadamente superior a una wave normal.
        //
        // El futuro DIP hook hara solamente una busqueda lineal
        // sobre este array pequeno, sin allocations ni mutexes.
        constexpr size_t MaxBlockedPawns =
            64;


        // Si el writer cambia el snapshot mientras lo leemos,
        // reintentamos unas pocas veces y despues fallamos seguro:
        //
        // no aplicar Glow.
        constexpr unsigned int ReaderRetries =
            3;


        // --------------------------------------------------------
        // UE2 skeletal render bridge
        //
        // USkeletalMeshInstance::Render + 0xFB7
        //
        // Engine.dll + 0x1EB687:
        //
        //     push 00
        //     call [edx+5C]
        //
        // El call real empieza en +0x1EB689.
        // --------------------------------------------------------

        constexpr uintptr_t BridgeSiteRva =
            0x001EB687;


        constexpr size_t BridgeHookLength =
            5;


        constexpr size_t BridgeStubSize =
            64;


        // Despues de restaurar los bytes originales dejamos un
        // periodo de gracia antes de liberar el codigo auxiliar.
        constexpr ULONGLONG BridgeUnloadGraceMs =
            250;


        // --------------------------------------------------------
        // FRenderInterface -> Direct3D9
        // --------------------------------------------------------

        constexpr uintptr_t
            FRenderInterfaceRenderDeviceOffset =
                0x04;


        constexpr uintptr_t
            RenderDeviceD3DDeviceOffset =
                0x495C;


        // IDirect3DDevice9 vtable:
        //
        // DrawIndexedPrimitive = index 82
        // 82 * 4 = 0x148
        constexpr uintptr_t
            DipVTableOffset =
                0x148;


        constexpr size_t
            DipHookLength =
                5;


        constexpr size_t
            DipTrampolineSize =
                DipHookLength + 5;


        constexpr ULONGLONG
            DipUnloadGraceMs =
                250;


        // Contexto UE2 activo por thread.
        //
        // Mucho mayor que la profundidad esperada.
        constexpr size_t
            RenderContextMaxDepth =
                16;
    }


    // ============================================================
    // SNAPSHOT PUBLICADO
    // ============================================================

    inline std::atomic_bool
        gEnabled{ false };


    // Par:
    //     snapshot estable.
    //
    // Impar:
    //     writer actualizando.
    inline std::atomic<uint32_t>
        gSequence{ 0 };


    inline std::array<
        std::atomic<uintptr_t>,
        Config::MaxBlockedPawns
    >
        gBlockedPawns{};


    inline std::atomic<size_t>
        gBlockedCount{ 0 };


    // Solo los writers usan este mutex.
    //
    // El futuro render hook NO lo toca.
    inline std::mutex
        gPublishMutex;


    // ============================================================
    // DIAGNOSTICO
    // ============================================================

    inline std::atomic<unsigned long long>
        gPublishCount{ 0 };


    inline std::atomic<unsigned long long>
        gOverflowCount{ 0 };


    inline std::atomic<ULONGLONG>
        gLastPublishAt{ 0 };


    // ============================================================
    // UE2 RENDER BRIDGE STATE
    // ============================================================

    inline std::atomic_bool
        gBridgeInstalled{ false };


    inline std::atomic_bool
        gBridgeUninstallRequested{ false };


    inline std::atomic<unsigned int>
        gBridgeInFlight{ 0 };


    inline std::atomic<unsigned long long>
        gBridgeHits{ 0 };


    inline std::atomic<unsigned long long>
        gBridgeObservedEnabled{ 0 };


    inline std::atomic<unsigned long long>
        gBridgeBlockedMatches{ 0 };


    inline std::atomic<unsigned long long>
        gBridgeValidationFailures{ 0 };


    inline std::atomic<unsigned long long>
        gBridgeUnhookFailures{ 0 };


    inline std::atomic<uintptr_t>
        gBridgeLastActor{ 0 };


    inline std::atomic<uintptr_t>
        gBridgeLastBlockedActor{ 0 };


    inline std::atomic<ULONGLONG>
        gBridgeUnhookCompletedAt{ 0 };


    inline BYTE*
        gBridgeTarget = nullptr;


    inline BYTE*
        gBridgeStub = nullptr;


    inline BYTE
        gBridgeOriginalBytes[
            Config::BridgeHookLength
        ]{};


    // ============================================================
    // ACTIVE UE2 RENDER CONTEXT
    //
    // TLS:
    // cada render thread conserva su propio actor actual.
    // ============================================================

    struct RenderContext
    {
        uintptr_t actor = 0;

        bool blocked = false;
    };


    inline thread_local std::array<
        RenderContext,
        Config::RenderContextMaxDepth
    >
        gRenderContextStack{};


    inline thread_local size_t
        gRenderContextDepth = 0;


    inline thread_local size_t
        gRenderContextOverflowDepth = 0;


    inline std::atomic<unsigned long long>
        gRenderContextOverflows{ 0 };


    // ============================================================
    // D3D9 DIP STATE
    // ============================================================

    using DrawIndexedPrimitiveFn =
        HRESULT (WINAPI*)(
            IDirect3DDevice9* device,
            D3DPRIMITIVETYPE primitiveType,
            INT baseVertexIndex,
            UINT minVertexIndex,
            UINT numVertices,
            UINT startIndex,
            UINT primitiveCount
        );


    inline std::atomic<uintptr_t>
        gDipCandidate{ 0 };


    inline std::atomic<uintptr_t>
        gDipDevice{ 0 };


    inline std::atomic_bool
        gDipInstalled{ false };


    inline std::atomic_bool
        gDipInstallAttempted{ false };


    inline std::atomic_bool
        gDipUninstallRequested{ false };


    inline std::atomic<unsigned int>
        gDipInFlight{ 0 };


    inline std::atomic<unsigned long long>
        gDipHits{ 0 };


    inline std::atomic<unsigned long long>
        gDipContextHits{ 0 };


    inline std::atomic<unsigned long long>
        gDipBlockedHits{ 0 };


    inline std::atomic<unsigned long long>
        gDipResolveFaults{ 0 };


    inline std::atomic<unsigned long long>
        gDipCandidateChanges{ 0 };


    inline std::atomic<unsigned long long>
        gDipValidationFailures{ 0 };


    inline std::atomic<unsigned long long>
        gDipRejectedCandidates{ 0 };


    inline std::atomic<unsigned long long>
        gDipUnhookFailures{ 0 };


    // ============================================================
    // RED X-RAY DIAGNOSTICS
    // ============================================================

    inline std::atomic<unsigned long long>
        gGlowDraws{ 0 };


    inline std::atomic<unsigned long long>
        gGlowCaptureFailures{ 0 };


    inline std::atomic<unsigned long long>
        gGlowApplyFailures{ 0 };


    inline std::atomic<unsigned long long>
        gGlowRestoreFailures{ 0 };


    // Si una restauracion D3D falla dejamos de aplicar Glow
    // hasta el siguiente ciclo F7 OFF -> ON.
    //
    // Fail-safe:
    //
    //     perder Glow
    //
    // es preferible a seguir modificando estados sobre un device
    // cuyo estado anterior ya no pudimos restaurar con certeza.
    inline std::atomic_bool
        gGlowFaulted{ false };


    inline std::atomic<unsigned long long>
        gGlowStageBusySkips{ 0 };


    inline std::atomic<uintptr_t>
        gGlowLastActor{ 0 };


    inline std::atomic<uintptr_t>
        gDipLastActor{ 0 };


    inline std::atomic<uintptr_t>
        gDipLastBlockedActor{ 0 };


    inline std::atomic<ULONGLONG>
        gDipUnhookCompletedAt{ 0 };


    inline BYTE*
        gDipTarget = nullptr;


    inline BYTE*
        gDipTrampoline = nullptr;


    inline DrawIndexedPrimitiveFn
        gDipOriginal = nullptr;


    inline BYTE
        gDipOriginalBytes[
            Config::DipHookLength
        ]{};


    // ============================================================
    // SNAPSHOT DEBUG
    // ============================================================

    struct Snapshot
    {
        std::array<
            uintptr_t,
            Config::MaxBlockedPawns
        >
            pawns{};


        size_t count = 0;

        uint32_t sequence = 0;
    };


    // ============================================================
    // STATE
    // ============================================================

    inline bool IsEnabled()
    {
        return
            gEnabled.load(
                std::memory_order_acquire
            );
    }


    // ============================================================
    // WRITER INTERNAL
    //
    // gPublishMutex debe estar tomado.
    // ============================================================

    inline void ClearBlockedPawnsNoLock()
    {
        // even -> odd
        gSequence.fetch_add(
            1,
            std::memory_order_acq_rel
        );


        gBlockedCount.store(
            0,
            std::memory_order_relaxed
        );


        for (
            auto& slot :
            gBlockedPawns
            )
        {
            slot.store(
                0,
                std::memory_order_relaxed
            );
        }


        // odd -> even
        gSequence.fetch_add(
            1,
            std::memory_order_release
        );


        gLastPublishAt.store(
            GetTickCount64(),
            std::memory_order_release
        );
    }


    // ============================================================
    // CLEAR
    // ============================================================

    inline void ClearBlockedPawns()
    {
        std::lock_guard<std::mutex> lock(
            gPublishMutex
        );


        ClearBlockedPawnsNoLock();
    }


    // ============================================================
    // ENABLE / DISABLE
    //
    // Cada transicion empieza con snapshot vacio.
    // ============================================================

    inline void SetEnabled(
        bool enabled
    )
    {
        std::lock_guard<std::mutex> lock(
            gPublishMutex
        );


        // Evita que un reader acepte datos viejos durante
        // una transicion de estado.
        gEnabled.store(
            false,
            std::memory_order_release
        );


        ClearBlockedPawnsNoLock();


        // Un ciclo OFF -> ON rearma el Glow.
        //
        // No lo rearmamos automaticamente mientras sigue activo:
        // un restore failure requiere una transicion explicita.
        if (!enabled)
        {
            gGlowFaulted.store(
                false,
                std::memory_order_release
            );
        }


        gEnabled.store(
            enabled,
            std::memory_order_release
        );
    }


    // ============================================================
    // PUBLISH
    //
    // Llamado desde el lado de alto nivel, nunca desde DIP.
    // ============================================================

    inline void PublishBlockedPawns(
        const uintptr_t* pawns,
        size_t count
    )
    {
        if (!IsEnabled())
        {
            return;
        }


        std::array<
            uintptr_t,
            Config::MaxBlockedPawns
        >
            unique{};


        size_t uniqueCount = 0;


        if (pawns != nullptr)
        {
            for (
                size_t index = 0;
                index < count;
                ++index
                )
            {
                const uintptr_t pawn =
                    pawns[index];


                if (pawn == 0)
                {
                    continue;
                }


                bool duplicate =
                    false;


                for (
                    size_t existing = 0;
                    existing < uniqueCount;
                    ++existing
                    )
                {
                    if (
                        unique[existing] ==
                        pawn
                        )
                    {
                        duplicate =
                            true;

                        break;
                    }
                }


                if (duplicate)
                {
                    continue;
                }


                if (
                    uniqueCount >=
                    Config::MaxBlockedPawns
                    )
                {
                    gOverflowCount.fetch_add(
                        1,
                        std::memory_order_relaxed
                    );

                    break;
                }


                unique[
                    uniqueCount++
                ] =
                    pawn;
            }
        }


        std::lock_guard<std::mutex> lock(
            gPublishMutex
        );


        // SetEnabled(false) pudo ejecutarse mientras
        // preparabamos el array local.
        if (!IsEnabled())
        {
            return;
        }


        // even -> odd
        gSequence.fetch_add(
            1,
            std::memory_order_acq_rel
        );


        for (
            size_t index = 0;
            index <
                Config::MaxBlockedPawns;
            ++index
            )
        {
            const uintptr_t value =
                index < uniqueCount
                    ?
                    unique[index]
                    :
                    0;


            gBlockedPawns[index].store(
                value,
                std::memory_order_relaxed
            );
        }


        gBlockedCount.store(
            uniqueCount,
            std::memory_order_relaxed
        );


        // odd -> even
        gSequence.fetch_add(
            1,
            std::memory_order_release
        );


        gLastPublishAt.store(
            GetTickCount64(),
            std::memory_order_release
        );


        gPublishCount.fetch_add(
            1,
            std::memory_order_relaxed
        );
    }


    // ============================================================
    // READER RAPIDO
    //
    // Esta funcion esta diseñada para poder usarse mas adelante
    // dentro del render hook.
    //
    // SIN mutex.
    // SIN allocation.
    // SIN llamadas UE2.
    // ============================================================

    inline bool IsBlockedPawn(
        uintptr_t pawn
    )
    {
        if (
            pawn == 0 ||
            !IsEnabled()
            )
        {
            return false;
        }


        for (
            unsigned int attempt = 0;
            attempt <
                Config::ReaderRetries;
            ++attempt
            )
        {
            const uint32_t before =
                gSequence.load(
                    std::memory_order_acquire
                );


            if (
                (before & 1u) !=
                0u
                )
            {
                continue;
            }


            size_t count =
                gBlockedCount.load(
                    std::memory_order_relaxed
                );


            if (
                count >
                Config::MaxBlockedPawns
                )
            {
                count =
                    Config::MaxBlockedPawns;
            }


            bool found =
                false;


            for (
                size_t index = 0;
                index < count;
                ++index
                )
            {
                if (
                    gBlockedPawns[index].load(
                        std::memory_order_relaxed
                    ) ==
                    pawn
                    )
                {
                    found =
                        true;

                    break;
                }
            }


            const uint32_t after =
                gSequence.load(
                    std::memory_order_acquire
                );


            if (
                before == after &&
                (after & 1u) == 0u
                )
            {
                return found;
            }
        }


        // Fail-safe:
        // snapshot inestable -> render normal.
        return false;
    }


    // ============================================================
    // SNAPSHOT PARA CONSOLA
    // ============================================================

    inline bool TryReadSnapshot(
        Snapshot& snapshot
    )
    {
        snapshot = {};


        for (
            unsigned int attempt = 0;
            attempt <
                Config::ReaderRetries;
            ++attempt
            )
        {
            const uint32_t before =
                gSequence.load(
                    std::memory_order_acquire
                );


            if (
                (before & 1u) !=
                0u
                )
            {
                continue;
            }


            size_t count =
                gBlockedCount.load(
                    std::memory_order_relaxed
                );


            if (
                count >
                Config::MaxBlockedPawns
                )
            {
                count =
                    Config::MaxBlockedPawns;
            }


            for (
                size_t index = 0;
                index < count;
                ++index
                )
            {
                snapshot.pawns[index] =
                    gBlockedPawns[index].load(
                        std::memory_order_relaxed
                    );
            }


            const uint32_t after =
                gSequence.load(
                    std::memory_order_acquire
                );


            if (
                before == after &&
                (after & 1u) == 0u
                )
            {
                snapshot.count =
                    count;

                snapshot.sequence =
                    after;

                return true;
            }
        }


        snapshot = {};

        return false;
    }


    // ============================================================
    // UE2 RENDER BRIDGE
    //
    // FASE 2:
    //
    // READ ONLY.
    //
    // No modifica ningun render state.
    // No modifica materiales.
    // No modifica D3D.
    // ============================================================

    inline bool IsBridgeInstalled()
    {
        return
            gBridgeInstalled.load(
                std::memory_order_acquire
            );
    }


    inline bool WriteRelativeJump(
        BYTE* source,
        const void* destination
    )
    {
        if (
            source == nullptr ||
            destination == nullptr
            )
        {
            return false;
        }


        const uintptr_t from =
            reinterpret_cast<uintptr_t>(
                source
            );


        const uintptr_t to =
            reinterpret_cast<uintptr_t>(
                destination
            );


        const intptr_t delta =
            static_cast<intptr_t>(to) -
            static_cast<intptr_t>(
                from + 5
            );


        if (
            delta <
                std::numeric_limits<int32_t>::min() ||
            delta >
                std::numeric_limits<int32_t>::max()
            )
        {
            return false;
        }


        BYTE patch[5]{};

        patch[0] =
            0xE9;


        const int32_t relative =
            static_cast<int32_t>(
                delta
            );


        std::memcpy(
            patch + 1,
            &relative,
            sizeof(relative)
        );


        DWORD oldProtection =
            0;


        if (!VirtualProtect(
            source,
            sizeof(patch),
            PAGE_EXECUTE_READWRITE,
            &oldProtection
        ))
        {
            return false;
        }


        std::memcpy(
            source,
            patch,
            sizeof(patch)
        );


        FlushInstructionCache(
            GetCurrentProcess(),
            source,
            sizeof(patch)
        );


        DWORD ignored =
            0;


        VirtualProtect(
            source,
            sizeof(patch),
            oldProtection,
            &ignored
        );


        return true;
    }


    // ============================================================
    // FRI -> D3D9
    //
    // Solo publica una direccion candidata.
    //
    // NO instala el hook desde mitad del render.
    // ============================================================

    inline void ObserveRenderInterface(
        uintptr_t renderInterface
    )
    {
        if (renderInterface == 0)
        {
            return;
        }


        uintptr_t renderDevice =
            0;


        uintptr_t device =
            0;


        uintptr_t vtable =
            0;


        uintptr_t dip =
            0;


        __try
        {
            renderDevice =
                *reinterpret_cast<uintptr_t*>(
                    renderInterface +
                    Config::FRenderInterfaceRenderDeviceOffset
                );


            if (renderDevice == 0)
            {
                return;
            }


            device =
                *reinterpret_cast<uintptr_t*>(
                    renderDevice +
                    Config::RenderDeviceD3DDeviceOffset
                );


            if (device == 0)
            {
                return;
            }


            vtable =
                *reinterpret_cast<uintptr_t*>(
                    device
                );


            if (vtable == 0)
            {
                return;
            }


            dip =
                *reinterpret_cast<uintptr_t*>(
                    vtable +
                    Config::DipVTableOffset
                );
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            gDipResolveFaults.fetch_add(
                1,
                std::memory_order_relaxed
            );


            return;
        }


        if (dip == 0)
        {
            return;
        }


        // ========================================================
        // VALIDACION DEL CANDIDATO
        //
        // No aceptamos simplemente cualquier puntero obtenido
        // desde una FRenderInterface.
        //
        // Debe:
        //
        // 1. pertenecer a d3d9.dll;
        // 2. tener el prologo DIP ya observado:
        //
        //      8B FF       mov edi,edi
        //      55          push ebp
        //      8B EC       mov ebp,esp
        // ========================================================

        HMODULE d3d9 =
            GetModuleHandleA(
                "d3d9.dll"
            );


        HMODULE owner =
            nullptr;


        const BOOL ownerResolved =
            GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,

                reinterpret_cast<LPCSTR>(
                    dip
                ),

                &owner
            );


        if (
            d3d9 == nullptr ||
            !ownerResolved ||
            owner != d3d9
            )
        {
            gDipRejectedCandidates.fetch_add(
                1,
                std::memory_order_relaxed
            );


            return;
        }


        constexpr BYTE expectedDipPrologue[5] =
        {
            0x8B,
            0xFF,
            0x55,
            0x8B,
            0xEC
        };


        bool validPrologue =
            false;


        __try
        {
            validPrologue =
                std::memcmp(
                    reinterpret_cast<const void*>(
                        dip
                    ),

                    expectedDipPrologue,

                    sizeof(
                        expectedDipPrologue
                    )
                ) == 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            gDipResolveFaults.fetch_add(
                1,
                std::memory_order_relaxed
            );


            return;
        }


        if (!validPrologue)
        {
            gDipRejectedCandidates.fetch_add(
                1,
                std::memory_order_relaxed
            );


            return;
        }


        gDipDevice.store(
            device,
            std::memory_order_release
        );


        uintptr_t expected =
            0;


        if (
            gDipCandidate.compare_exchange_strong(
                expected,
                dip,
                std::memory_order_acq_rel
            )
            )
        {
            gDipInstallAttempted.store(
                false,
                std::memory_order_release
            );


            return;
        }


        if (
            expected != dip
            )
        {
            gDipCandidateChanges.fetch_add(
                1,
                std::memory_order_relaxed
            );


            // Mientras no exista hook podemos aceptar una nueva
            // candidate, por ejemplo tras una recreacion del device.
            if (
                !gDipInstalled.load(
                    std::memory_order_acquire
                )
                )
            {
                gDipCandidate.store(
                    dip,
                    std::memory_order_release
                );


                gDipInstallAttempted.store(
                    false,
                    std::memory_order_release
                );
            }
        }
    }


    // ============================================================
    // CURRENT TLS CONTEXT
    // ============================================================

    inline bool TryGetCurrentRenderContext(
        RenderContext& context
    )
    {
        context = {};


        // Si estamos dentro de una profundidad que excedio nuestra
        // pila fija, fallamos seguro: no asociamos el DIP.
        if (
            gRenderContextOverflowDepth != 0 ||
            gRenderContextDepth == 0
            )
        {
            return false;
        }


        context =
            gRenderContextStack[
                gRenderContextDepth - 1
            ];


        return
            context.actor != 0;
    }


    // ============================================================
    // BRIDGE CALLBACKS
    //
    // Son llamadas desde el pequeño stub generado en runtime.
    //
    // BridgeEnter incrementa InFlight ANTES de cualquier trabajo.
    // BridgeLeave lo decrementa despues del FRI +0x5C original.
    // ============================================================

    inline void __stdcall BridgeEnter(
        uintptr_t actor,
        uintptr_t renderInterface
    )
    {
        gBridgeInFlight.fetch_add(
            1,
            std::memory_order_acq_rel
        );


        gBridgeHits.fetch_add(
            1,
            std::memory_order_relaxed
        );


        gBridgeLastActor.store(
            actor,
            std::memory_order_relaxed
        );


        // Captura dinamica necesaria solamente durante discovery.
        //
        // Una vez publicado un candidato DIP valido ya no seguimos
        // caminando:
        //
        // FRI -> RenderDevice -> Device -> vtable
        //
        // miles de veces por segundo.
        if (
            gDipCandidate.load(
                std::memory_order_acquire
            ) == 0
            )
        {
            ObserveRenderInterface(
                renderInterface
            );
        }


        bool blocked =
            false;


        if (
            actor != 0 &&
            IsEnabled()
            )
        {
            gBridgeObservedEnabled.fetch_add(
                1,
                std::memory_order_relaxed
            );


            blocked =
                IsBlockedPawn(
                    actor
                );


            if (blocked)
            {
                gBridgeBlockedMatches.fetch_add(
                    1,
                    std::memory_order_relaxed
                );


                gBridgeLastBlockedActor.store(
                    actor,
                    std::memory_order_relaxed
                );
            }
        }


        // --------------------------------------------------------
        // TLS push
        // --------------------------------------------------------

        if (
            gRenderContextOverflowDepth != 0
            )
        {
            ++gRenderContextOverflowDepth;


            return;
        }


        if (
            gRenderContextDepth >=
            Config::RenderContextMaxDepth
            )
        {
            gRenderContextOverflowDepth =
                1;


            gRenderContextOverflows.fetch_add(
                1,
                std::memory_order_relaxed
            );


            return;
        }


        gRenderContextStack[
            gRenderContextDepth++
        ] =
        {
            actor,
            blocked
        };
    }


    inline void __stdcall BridgeLeave()
    {
        if (
            gRenderContextOverflowDepth != 0
            )
        {
            --gRenderContextOverflowDepth;
        }
        else if (
            gRenderContextDepth != 0
            )
        {
            --gRenderContextDepth;
        }


        gBridgeInFlight.fetch_sub(
            1,
            std::memory_order_acq_rel
        );
    }


    // ============================================================
    // BUILD STUB
    //
    // Generamos:
    //
    // pushfd
    // pushad
    //
    // xor eax,eax
    // mov ecx,[ebp+08]       ; FDynamicActor*
    // test ecx,ecx
    // jz actor_ready
    // mov eax,[ecx]          ; AActor*
    //
    // actor_ready:
    // push eax
    // mov eax,BridgeEnter
    // call eax
    //
    // popad
    // popfd
    //
    // push 00
    // call [edx+5C]          ; ORIGINAL
    //
    // pushfd
    // pushad
    // mov eax,BridgeLeave
    // call eax
    // popad
    // popfd
    //
    // jmp Engine+1EB68C
    // ============================================================

    inline bool BuildBridgeStub(
        BYTE* stub,
        BYTE* returnAddress
    )
    {
        if (
            stub == nullptr ||
            returnAddress == nullptr
            )
        {
            return false;
        }


        size_t offset =
            0;


        const auto emit8 =
            [&](BYTE value)
            {
                stub[offset++] =
                    value;
            };


        const auto emit32 =
            [&](uint32_t value)
            {
                std::memcpy(
                    stub + offset,
                    &value,
                    sizeof(value)
                );


                offset +=
                    sizeof(value);
            };


        // pushfd
        emit8(0x9C);

        // pushad
        emit8(0x60);


        // xor eax,eax
        emit8(0x33);
        emit8(0xC0);


        // mov ecx,[ebp+08]
        emit8(0x8B);
        emit8(0x4D);
        emit8(0x08);


        // test ecx,ecx
        emit8(0x85);
        emit8(0xC9);


        // jz +2
        //
        // Salta:
        //
        // mov eax,[ecx]
        emit8(0x74);
        emit8(0x02);


        // mov eax,[ecx]
        emit8(0x8B);
        emit8(0x01);


        // mov edx,[ebp+18]
        //
        // [EBP+18] = FRenderInterface*
        //
        // EDX original se restaura despues mediante popad.
        emit8(0x8B);
        emit8(0x55);
        emit8(0x18);


        // __stdcall:
        // BridgeEnter(actor, renderInterface)
        //
        // argumentos right-to-left.

        // push renderInterface
        emit8(0x52);


        // push actor
        emit8(0x50);


        // mov eax, BridgeEnter
        emit8(0xB8);

        emit32(
            static_cast<uint32_t>(
                reinterpret_cast<uintptr_t>(
                    &BridgeEnter
                )
            )
        );


        // call eax
        emit8(0xFF);
        emit8(0xD0);


        // popad
        emit8(0x61);

        // popfd
        emit8(0x9D);


        // ========================================================
        // instrucciones originales
        // ========================================================

        // push 00
        emit8(0x6A);
        emit8(0x00);


        // call [edx+5C]
        emit8(0xFF);
        emit8(0x52);
        emit8(0x5C);


        // ========================================================
        // Leave
        // ========================================================

        // pushfd
        emit8(0x9C);

        // pushad
        emit8(0x60);


        // mov eax, BridgeLeave
        emit8(0xB8);

        emit32(
            static_cast<uint32_t>(
                reinterpret_cast<uintptr_t>(
                    &BridgeLeave
                )
            )
        );


        // call eax
        emit8(0xFF);
        emit8(0xD0);


        // popad
        emit8(0x61);

        // popfd
        emit8(0x9D);


        // jmp returnAddress
        emit8(0xE9);


        const size_t relativeOffset =
            offset;


        emit32(0);


        if (
            offset >
            Config::BridgeStubSize
            )
        {
            return false;
        }


        const uintptr_t jumpFrom =
            reinterpret_cast<uintptr_t>(
                stub + offset
            );


        const uintptr_t jumpTo =
            reinterpret_cast<uintptr_t>(
                returnAddress
            );


        const intptr_t delta =
            static_cast<intptr_t>(
                jumpTo
            ) -
            static_cast<intptr_t>(
                jumpFrom
            );


        if (
            delta <
                std::numeric_limits<int32_t>::min() ||
            delta >
                std::numeric_limits<int32_t>::max()
            )
        {
            return false;
        }


        const int32_t relative =
            static_cast<int32_t>(
                delta
            );


        std::memcpy(
            stub + relativeOffset,
            &relative,
            sizeof(relative)
        );


        return true;
    }


    // ============================================================
    // RESTORE BRIDGE BYTES
    // ============================================================

    inline bool RestoreBridgeBytes()
    {
        if (gBridgeTarget == nullptr)
        {
            return true;
        }


        DWORD oldProtection =
            0;


        if (!VirtualProtect(
            gBridgeTarget,
            Config::BridgeHookLength,
            PAGE_EXECUTE_READWRITE,
            &oldProtection
        ))
        {
            return false;
        }


        std::memcpy(
            gBridgeTarget,
            gBridgeOriginalBytes,
            Config::BridgeHookLength
        );


        FlushInstructionCache(
            GetCurrentProcess(),
            gBridgeTarget,
            Config::BridgeHookLength
        );


        DWORD ignored =
            0;


        VirtualProtect(
            gBridgeTarget,
            Config::BridgeHookLength,
            oldProtection,
            &ignored
        );


        return true;
    }


    // ============================================================
    // INSTALL
    // ============================================================

    inline bool InstallBridge()
    {
        if (IsBridgeInstalled())
        {
            return true;
        }


        if (
            gBridgeTarget != nullptr ||
            gBridgeStub != nullptr
            )
        {
            return false;
        }


        HMODULE engine =
            GetModuleHandleA(
                "Engine.dll"
            );


        if (engine == nullptr)
        {
            return false;
        }


        BYTE* target =
            reinterpret_cast<BYTE*>(
                engine
            ) +
            Config::BridgeSiteRva;


        constexpr BYTE expected[] =
        {
            0x6A, 0x00,
            0xFF, 0x52, 0x5C,
            0x8B, 0x4D, 0x08,
            0x8B, 0x09,
            0x8B, 0x11,
            0xFF, 0x92,
            0xA0, 0x01, 0x00, 0x00
        };


        if (
            std::memcmp(
                target,
                expected,
                sizeof(expected)
            ) != 0
            )
        {
            gBridgeValidationFailures.fetch_add(
                1,
                std::memory_order_relaxed
            );


            return false;
        }


        BYTE* stub =
            reinterpret_cast<BYTE*>(
                VirtualAlloc(
                    nullptr,
                    Config::BridgeStubSize,
                    MEM_COMMIT | MEM_RESERVE,
                    PAGE_READWRITE
                )
            );


        if (stub == nullptr)
        {
            return false;
        }


        if (!BuildBridgeStub(
            stub,
            target +
                Config::BridgeHookLength
        ))
        {
            VirtualFree(
                stub,
                0,
                MEM_RELEASE
            );


            return false;
        }


        DWORD oldProtection =
            0;


        if (!VirtualProtect(
            stub,
            Config::BridgeStubSize,
            PAGE_EXECUTE_READ,
            &oldProtection
        ))
        {
            VirtualFree(
                stub,
                0,
                MEM_RELEASE
            );


            return false;
        }


        FlushInstructionCache(
            GetCurrentProcess(),
            stub,
            Config::BridgeStubSize
        );


        // Revalidacion inmediatamente antes del patch.
        if (
            std::memcmp(
                target,
                expected,
                sizeof(expected)
            ) != 0
            )
        {
            VirtualFree(
                stub,
                0,
                MEM_RELEASE
            );


            gBridgeValidationFailures.fetch_add(
                1,
                std::memory_order_relaxed
            );


            return false;
        }


        std::memcpy(
            gBridgeOriginalBytes,
            target,
            Config::BridgeHookLength
        );


        gBridgeTarget =
            target;


        gBridgeStub =
            stub;


        if (!WriteRelativeJump(
            target,
            stub
        ))
        {
            gBridgeTarget =
                nullptr;


            gBridgeStub =
                nullptr;


            VirtualFree(
                stub,
                0,
                MEM_RELEASE
            );


            return false;
        }


        gBridgeHits.store(
            0,
            std::memory_order_relaxed
        );


        gBridgeObservedEnabled.store(
            0,
            std::memory_order_relaxed
        );


        gBridgeBlockedMatches.store(
            0,
            std::memory_order_relaxed
        );


        gBridgeInFlight.store(
            0,
            std::memory_order_relaxed
        );


        gBridgeLastActor.store(
            0,
            std::memory_order_relaxed
        );


        gBridgeLastBlockedActor.store(
            0,
            std::memory_order_relaxed
        );


        gBridgeUnhookCompletedAt.store(
            0,
            std::memory_order_release
        );


        gBridgeUninstallRequested.store(
            false,
            std::memory_order_release
        );


        gBridgeInstalled.store(
            true,
            std::memory_order_release
        );


        return true;
    }


    // ============================================================
    // SAFE UNINSTALL REQUEST
    // ============================================================

    inline void RequestBridgeUninstall()
    {
        if (IsBridgeInstalled())
        {
            gBridgeUninstallRequested.store(
                true,
                std::memory_order_release
            );
        }
    }


    // ============================================================
    // SERVICE FROM GAME THREAD
    //
    // Se llama al principio de KFCollision::OnGameFrame().
    //
    // De esa manera retiramos el JMP desde el mismo flujo de
    // PostRender que ya utiliza el proyecto para su lifecycle.
    // ============================================================

    inline void ServiceBridgeLifecycle()
    {
        if (
            !gBridgeUninstallRequested.load(
                std::memory_order_acquire
            )
            )
        {
            return;
        }


        if (!IsBridgeInstalled())
        {
            gBridgeUninstallRequested.store(
                false,
                std::memory_order_release
            );


            return;
        }


        if (!KFGameThread::IsGameThread())
        {
            return;
        }


        if (!RestoreBridgeBytes())
        {
            gBridgeUnhookFailures.fetch_add(
                1,
                std::memory_order_relaxed
            );


            return;
        }


        // Desde este momento el sitio UE2 ya no puede generar
        // nuevas entradas al bridge.
        gBridgeInstalled.store(
            false,
            std::memory_order_release
        );


        gBridgeUninstallRequested.store(
            false,
            std::memory_order_release
        );


        gBridgeUnhookCompletedAt.store(
            GetTickCount64(),
            std::memory_order_release
        );
    }


    // ============================================================
    // SAFE TO UNLOAD
    // ============================================================

    inline bool IsBridgeSafeToUnload()
    {
        if (IsBridgeInstalled())
        {
            return false;
        }


        if (
            gBridgeInFlight.load(
                std::memory_order_acquire
            ) != 0
            )
        {
            return false;
        }


        const ULONGLONG completedAt =
            gBridgeUnhookCompletedAt.load(
                std::memory_order_acquire
            );


        // Nunca instalado / instalacion fallida.
        if (completedAt == 0)
        {
            return
                gBridgeStub ==
                nullptr;
        }


        return
            GetTickCount64() -
                completedAt >=
            Config::BridgeUnloadGraceMs;
    }


    // ============================================================
    // FINAL FREE
    //
    // Solo cuando el sitio ya fue restaurado y termino el periodo
    // de gracia.
    // ============================================================

    inline bool FinalizeBridge()
    {
        if (!IsBridgeSafeToUnload())
        {
            return false;
        }


        BYTE* stub =
            gBridgeStub;


        gBridgeStub =
            nullptr;


        gBridgeTarget =
            nullptr;


        if (stub != nullptr)
        {
            VirtualFree(
                stub,
                0,
                MEM_RELEASE
            );
        }


        return true;
    }


    // ============================================================
    // RED X-RAY STATE
    //
    // Todos los valores modificados se capturan antes del draw
    // y se restauran exactamente despues del DIP original.
    // ============================================================

    struct GlowD3DState
    {
        DWORD zWriteEnable = 0;

        DWORD zFunc = 0;

        DWORD textureFactor = 0;


        DWORD stage4ColorOp = 0;

        DWORD stage4ColorArg1 = 0;

        DWORD stage4ColorArg2 = 0;


        DWORD stage4AlphaOp = 0;

        DWORD stage4AlphaArg1 = 0;


        // Al habilitar Stage4 necesitamos garantizar que la
        // cascada termine inmediatamente despues.
        DWORD stage5ColorOp = 0;
    };


    // ============================================================
    // CAPTURE
    // ============================================================

    inline bool CaptureRedXRayState(
        IDirect3DDevice9* device,
        GlowD3DState& state
    )
    {
        state = {};


        if (device == nullptr)
        {
            return false;
        }


        if (FAILED(
            device->GetRenderState(
                D3DRS_ZWRITEENABLE,
                &state.zWriteEnable
            )
        ))
        {
            return false;
        }


        if (FAILED(
            device->GetRenderState(
                D3DRS_ZFUNC,
                &state.zFunc
            )
        ))
        {
            return false;
        }


        if (FAILED(
            device->GetRenderState(
                D3DRS_TEXTUREFACTOR,
                &state.textureFactor
            )
        ))
        {
            return false;
        }


        if (FAILED(
            device->GetTextureStageState(
                4,
                D3DTSS_COLOROP,
                &state.stage4ColorOp
            )
        ))
        {
            return false;
        }


        if (FAILED(
            device->GetTextureStageState(
                4,
                D3DTSS_COLORARG1,
                &state.stage4ColorArg1
            )
        ))
        {
            return false;
        }


        if (FAILED(
            device->GetTextureStageState(
                4,
                D3DTSS_COLORARG2,
                &state.stage4ColorArg2
            )
        ))
        {
            return false;
        }


        if (FAILED(
            device->GetTextureStageState(
                4,
                D3DTSS_ALPHAOP,
                &state.stage4AlphaOp
            )
        ))
        {
            return false;
        }


        if (FAILED(
            device->GetTextureStageState(
                4,
                D3DTSS_ALPHAARG1,
                &state.stage4AlphaArg1
            )
        ))
        {
            return false;
        }


        if (FAILED(
            device->GetTextureStageState(
                5,
                D3DTSS_COLOROP,
                &state.stage5ColorOp
            )
        ))
        {
            return false;
        }


        return true;
    }


    // ============================================================
    // APPLY
    //
    // Mismo efecto validado en CE:
    //
    // ZWrite = FALSE
    // ZFunc  = ALWAYS
    //
    // TFactor = FFFF3030
    //
    // Stage4:
    //      CURRENT x TFACTOR x 2
    //
    // Stage5:
    //      DISABLE
    // ============================================================

    inline bool ApplyRedXRayState(
        IDirect3DDevice9* device
    )
    {
        if (device == nullptr)
        {
            return false;
        }


        bool success =
            true;


        // Cortamos cualquier estado residual posterior a Stage4.
        if (FAILED(
            device->SetTextureStageState(
                5,
                D3DTSS_COLOROP,
                D3DTOP_DISABLE
            )
        ))
        {
            success =
                false;
        }


        // Rojo intenso manteniendo textura/sombreado.
        if (FAILED(
            device->SetRenderState(
                D3DRS_TEXTUREFACTOR,
                0xFFFF3030u
            )
        ))
        {
            success =
                false;
        }


        // Primero argumentos; COLOROP se activa al final.
        if (FAILED(
            device->SetTextureStageState(
                4,
                D3DTSS_COLORARG1,
                D3DTA_CURRENT
            )
        ))
        {
            success =
                false;
        }


        if (FAILED(
            device->SetTextureStageState(
                4,
                D3DTSS_COLORARG2,
                D3DTA_TFACTOR
            )
        ))
        {
            success =
                false;
        }


        if (FAILED(
            device->SetTextureStageState(
                4,
                D3DTSS_ALPHAARG1,
                D3DTA_CURRENT
            )
        ))
        {
            success =
                false;
        }


        if (FAILED(
            device->SetTextureStageState(
                4,
                D3DTSS_ALPHAOP,
                D3DTOP_SELECTARG1
            )
        ))
        {
            success =
                false;
        }


        if (FAILED(
            device->SetTextureStageState(
                4,
                D3DTSS_COLOROP,
                D3DTOP_MODULATE2X
            )
        ))
        {
            success =
                false;
        }


        // Depth X-Ray.
        if (FAILED(
            device->SetRenderState(
                D3DRS_ZWRITEENABLE,
                FALSE
            )
        ))
        {
            success =
                false;
        }


        if (FAILED(
            device->SetRenderState(
                D3DRS_ZFUNC,
                D3DCMP_ALWAYS
            )
        ))
        {
            success =
                false;
        }


        return success;
    }


    // ============================================================
    // RESTORE
    //
    // Se intentan TODAS las restauraciones incluso si una falla.
    // ============================================================

    inline bool RestoreRedXRayState(
        IDirect3DDevice9* device,
        const GlowD3DState& state
    )
    {
        if (device == nullptr)
        {
            return false;
        }


        bool success =
            true;


        // Depth primero.
        if (FAILED(
            device->SetRenderState(
                D3DRS_ZFUNC,
                state.zFunc
            )
        ))
        {
            success =
                false;
        }


        if (FAILED(
            device->SetRenderState(
                D3DRS_ZWRITEENABLE,
                state.zWriteEnable
            )
        ))
        {
            success =
                false;
        }


        // El Stage4 original estaba deshabilitado para nuestro
        // pipeline validado. Restauramos COLOROP primero para
        // cortar la etapa cuanto antes.
        if (FAILED(
            device->SetTextureStageState(
                4,
                D3DTSS_COLOROP,
                state.stage4ColorOp
            )
        ))
        {
            success =
                false;
        }


        if (FAILED(
            device->SetTextureStageState(
                4,
                D3DTSS_COLORARG1,
                state.stage4ColorArg1
            )
        ))
        {
            success =
                false;
        }


        if (FAILED(
            device->SetTextureStageState(
                4,
                D3DTSS_COLORARG2,
                state.stage4ColorArg2
            )
        ))
        {
            success =
                false;
        }


        if (FAILED(
            device->SetTextureStageState(
                4,
                D3DTSS_ALPHAOP,
                state.stage4AlphaOp
            )
        ))
        {
            success =
                false;
        }


        if (FAILED(
            device->SetTextureStageState(
                4,
                D3DTSS_ALPHAARG1,
                state.stage4AlphaArg1
            )
        ))
        {
            success =
                false;
        }


        if (FAILED(
            device->SetTextureStageState(
                5,
                D3DTSS_COLOROP,
                state.stage5ColorOp
            )
        ))
        {
            success =
                false;
        }


        if (FAILED(
            device->SetRenderState(
                D3DRS_TEXTUREFACTOR,
                state.textureFactor
            )
        ))
        {
            success =
                false;
        }


        return success;
    }


    // ============================================================
    // D3D9 DRAWINDEXEDPRIMITIVE
    //
    // VISIBLE / UNKNOWN:
    //      DIP original sin tocar estados.
    //
    // BLOCKED:
    //      RED X-RAY alrededor del DIP.
    // ============================================================

    inline bool IsDipInstalled()
    {
        return
            gDipInstalled.load(
                std::memory_order_acquire
            );
    }


    inline HRESULT WINAPI HookedDrawIndexedPrimitive(
        IDirect3DDevice9* device,
        D3DPRIMITIVETYPE primitiveType,
        INT baseVertexIndex,
        UINT minVertexIndex,
        UINT numVertices,
        UINT startIndex,
        UINT primitiveCount
    )
    {
        gDipInFlight.fetch_add(
            1,
            std::memory_order_acq_rel
        );


        gDipHits.fetch_add(
            1,
            std::memory_order_relaxed
        );


        gDipDevice.store(
            reinterpret_cast<uintptr_t>(
                device
            ),
            std::memory_order_relaxed
        );


        const DrawIndexedPrimitiveFn original =
            gDipOriginal;


        if (original == nullptr)
        {
            gDipInFlight.fetch_sub(
                1,
                std::memory_order_acq_rel
            );


            return
                D3DERR_INVALIDCALL;
        }


        RenderContext context;


        const bool hasContext =
            TryGetCurrentRenderContext(
                context
            );


        if (hasContext)
        {
            gDipContextHits.fetch_add(
                1,
                std::memory_order_relaxed
            );


            gDipLastActor.store(
                context.actor,
                std::memory_order_relaxed
            );
        }


        const bool shouldGlow =
            hasContext &&
            context.blocked &&
            IsEnabled() &&
            !gGlowFaulted.load(
                std::memory_order_acquire
            );


        if (!shouldGlow)
        {
            const HRESULT result =
                original(
                    device,
                    primitiveType,
                    baseVertexIndex,
                    minVertexIndex,
                    numVertices,
                    startIndex,
                    primitiveCount
                );


            gDipInFlight.fetch_sub(
                1,
                std::memory_order_acq_rel
            );


            return result;
        }


        // ========================================================
        // BLOCKED
        // ========================================================

        gDipBlockedHits.fetch_add(
            1,
            std::memory_order_relaxed
        );


        gDipLastBlockedActor.store(
            context.actor,
            std::memory_order_relaxed
        );


        GlowD3DState oldState;


        if (!CaptureRedXRayState(
            device,
            oldState
        ))
        {
            gGlowCaptureFailures.fetch_add(
                1,
                std::memory_order_relaxed
            );


            const HRESULT result =
                original(
                    device,
                    primitiveType,
                    baseVertexIndex,
                    minVertexIndex,
                    numVertices,
                    startIndex,
                    primitiveCount
                );


            gDipInFlight.fetch_sub(
                1,
                std::memory_order_acq_rel
            );


            return result;
        }


        // Stage4 solo se utiliza si realmente es la primera etapa
        // libre del pipeline actual.
        if (
            oldState.stage4ColorOp !=
            D3DTOP_DISABLE
            )
        {
            gGlowStageBusySkips.fetch_add(
                1,
                std::memory_order_relaxed
            );


            const HRESULT result =
                original(
                    device,
                    primitiveType,
                    baseVertexIndex,
                    minVertexIndex,
                    numVertices,
                    startIndex,
                    primitiveCount
                );


            gDipInFlight.fetch_sub(
                1,
                std::memory_order_acq_rel
            );


            return result;
        }


        // Aplicar el estado completo antes del draw.
        if (!ApplyRedXRayState(
            device
        ))
        {
            gGlowApplyFailures.fetch_add(
                1,
                std::memory_order_relaxed
            );


            // Puede existir una aplicacion parcial.
            //
            // Restauramos ANTES de llamar al DIP original para que
            // este draw termine en estado normal.
            if (!RestoreRedXRayState(
                device,
                oldState
            ))
            {
                gGlowRestoreFailures.fetch_add(
                    1,
                    std::memory_order_relaxed
                );


                gGlowFaulted.store(
                    true,
                    std::memory_order_release
                );
            }


            const HRESULT result =
                original(
                    device,
                    primitiveType,
                    baseVertexIndex,
                    minVertexIndex,
                    numVertices,
                    startIndex,
                    primitiveCount
                );


            gDipInFlight.fetch_sub(
                1,
                std::memory_order_acq_rel
            );


            return result;
        }


        // ========================================================
        // RED X-RAY DRAW
        // ========================================================

        const HRESULT result =
            original(
                device,
                primitiveType,
                baseVertexIndex,
                minVertexIndex,
                numVertices,
                startIndex,
                primitiveCount
            );


        gGlowDraws.fetch_add(
            1,
            std::memory_order_relaxed
        );


        gGlowLastActor.store(
            context.actor,
            std::memory_order_relaxed
        );


        // ========================================================
        // RESTORE EXACTO
        // ========================================================

        if (!RestoreRedXRayState(
            device,
            oldState
        ))
        {
            gGlowRestoreFailures.fetch_add(
                1,
                std::memory_order_relaxed
            );


            gGlowFaulted.store(
                true,
                std::memory_order_release
            );
        }


        gDipInFlight.fetch_sub(
            1,
            std::memory_order_acq_rel
        );


        return result;
    }


    // ============================================================
    // INSTALL DIP
    // ============================================================

    inline bool InstallDipHook()
    {
        if (IsDipInstalled())
        {
            return true;
        }


        if (
            gDipTarget != nullptr ||
            gDipTrampoline != nullptr ||
            gDipOriginal != nullptr
            )
        {
            return false;
        }


        const uintptr_t candidate =
            gDipCandidate.load(
                std::memory_order_acquire
            );


        if (candidate == 0)
        {
            return false;
        }


        BYTE* target =
            reinterpret_cast<BYTE*>(
                candidate
            );


        // Prologo observado y validado durante los POC de CE:
        //
        // mov edi,edi
        // push ebp
        // mov ebp,esp
        constexpr BYTE expected[
            Config::DipHookLength
        ] =
        {
            0x8B,
            0xFF,
            0x55,
            0x8B,
            0xEC
        };


        if (
            std::memcmp(
                target,
                expected,
                sizeof(expected)
            ) != 0
            )
        {
            gDipValidationFailures.fetch_add(
                1,
                std::memory_order_relaxed
            );


            return false;
        }


        BYTE* trampoline =
            reinterpret_cast<BYTE*>(
                VirtualAlloc(
                    nullptr,
                    Config::DipTrampolineSize,
                    MEM_COMMIT | MEM_RESERVE,
                    PAGE_READWRITE
                )
            );


        if (trampoline == nullptr)
        {
            return false;
        }


        std::memcpy(
            trampoline,
            expected,
            Config::DipHookLength
        );


        // trampoline+5:
        //
        // jmp original+5
        if (!WriteRelativeJump(
            trampoline +
                Config::DipHookLength,
            target +
                Config::DipHookLength
        ))
        {
            VirtualFree(
                trampoline,
                0,
                MEM_RELEASE
            );


            return false;
        }


        DWORD oldProtection =
            0;


        if (!VirtualProtect(
            trampoline,
            Config::DipTrampolineSize,
            PAGE_EXECUTE_READ,
            &oldProtection
        ))
        {
            VirtualFree(
                trampoline,
                0,
                MEM_RELEASE
            );


            return false;
        }


        FlushInstructionCache(
            GetCurrentProcess(),
            trampoline,
            Config::DipTrampolineSize
        );


        // Revalidacion antes de tocar d3d9.
        if (
            std::memcmp(
                target,
                expected,
                sizeof(expected)
            ) != 0
            )
        {
            VirtualFree(
                trampoline,
                0,
                MEM_RELEASE
            );


            gDipValidationFailures.fetch_add(
                1,
                std::memory_order_relaxed
            );


            return false;
        }


        std::memcpy(
            gDipOriginalBytes,
            target,
            Config::DipHookLength
        );


        gDipTarget =
            target;


        gDipTrampoline =
            trampoline;


        gDipOriginal =
            reinterpret_cast<DrawIndexedPrimitiveFn>(
                trampoline
            );


        if (!WriteRelativeJump(
            target,
            reinterpret_cast<void*>(
                &HookedDrawIndexedPrimitive
            )
        ))
        {
            gDipTarget =
                nullptr;


            gDipTrampoline =
                nullptr;


            gDipOriginal =
                nullptr;


            VirtualFree(
                trampoline,
                0,
                MEM_RELEASE
            );


            return false;
        }


        gDipHits.store(
            0,
            std::memory_order_relaxed
        );


        gDipContextHits.store(
            0,
            std::memory_order_relaxed
        );


        gDipBlockedHits.store(
            0,
            std::memory_order_relaxed
        );


        gGlowDraws.store(
            0,
            std::memory_order_relaxed
        );


        gGlowCaptureFailures.store(
            0,
            std::memory_order_relaxed
        );


        gGlowApplyFailures.store(
            0,
            std::memory_order_relaxed
        );


        gGlowRestoreFailures.store(
            0,
            std::memory_order_relaxed
        );


        gGlowStageBusySkips.store(
            0,
            std::memory_order_relaxed
        );


        gGlowLastActor.store(
            0,
            std::memory_order_relaxed
        );


        gDipInFlight.store(
            0,
            std::memory_order_relaxed
        );


        gDipLastActor.store(
            0,
            std::memory_order_relaxed
        );


        gDipLastBlockedActor.store(
            0,
            std::memory_order_relaxed
        );


        gDipUnhookCompletedAt.store(
            0,
            std::memory_order_release
        );


        gDipUninstallRequested.store(
            false,
            std::memory_order_release
        );


        gDipInstalled.store(
            true,
            std::memory_order_release
        );


        return true;
    }


    // ============================================================
    // RESTORE DIP
    // ============================================================

    inline bool RestoreDipBytes()
    {
        if (gDipTarget == nullptr)
        {
            return true;
        }


        DWORD oldProtection =
            0;


        if (!VirtualProtect(
            gDipTarget,
            Config::DipHookLength,
            PAGE_EXECUTE_READWRITE,
            &oldProtection
        ))
        {
            return false;
        }


        std::memcpy(
            gDipTarget,
            gDipOriginalBytes,
            Config::DipHookLength
        );


        FlushInstructionCache(
            GetCurrentProcess(),
            gDipTarget,
            Config::DipHookLength
        );


        DWORD ignored =
            0;


        VirtualProtect(
            gDipTarget,
            Config::DipHookLength,
            oldProtection,
            &ignored
        );


        return true;
    }


    // ============================================================
    // REQUEST DIP UNINSTALL
    // ============================================================

    inline void RequestDipUninstall()
    {
        gDipUninstallRequested.store(
            true,
            std::memory_order_release
        );
    }


    // ============================================================
    // DIP LIFECYCLE
    //
    // Llamado desde OnGameFrame.
    // ============================================================

    inline void ServiceDipLifecycle()
    {
        if (
            gDipUninstallRequested.load(
                std::memory_order_acquire
            )
            )
        {
            if (!IsDipInstalled())
            {
                gDipUninstallRequested.store(
                    false,
                    std::memory_order_release
                );


                return;
            }


            if (!KFGameThread::IsGameThread())
            {
                return;
            }


            if (!RestoreDipBytes())
            {
                gDipUnhookFailures.fetch_add(
                    1,
                    std::memory_order_relaxed
                );


                return;
            }


            // Desde este punto d3d9 ya no puede entrar de nuevo
            // mediante nuestro JMP.
            gDipInstalled.store(
                false,
                std::memory_order_release
            );


            gDipUninstallRequested.store(
                false,
                std::memory_order_release
            );


            gDipUnhookCompletedAt.store(
                GetTickCount64(),
                std::memory_order_release
            );


            return;
        }


        if (IsDipInstalled())
        {
            return;
        }


        if (
            gDipCandidate.load(
                std::memory_order_acquire
            ) == 0
            )
        {
            return;
        }


        if (
            gDipInstallAttempted.exchange(
                true,
                std::memory_order_acq_rel
            )
            )
        {
            return;
        }


        InstallDipHook();
    }


    // ============================================================
    // DIP SAFE UNLOAD
    // ============================================================

    inline bool IsDipSafeToUnload()
    {
        if (IsDipInstalled())
        {
            return false;
        }


        if (
            gDipInFlight.load(
                std::memory_order_acquire
            ) != 0
            )
        {
            return false;
        }


        const ULONGLONG completedAt =
            gDipUnhookCompletedAt.load(
                std::memory_order_acquire
            );


        if (completedAt == 0)
        {
            return
                gDipTrampoline ==
                nullptr;
        }


        return
            GetTickCount64() -
                completedAt >=
            Config::DipUnloadGraceMs;
    }


    inline bool FinalizeDipHook()
    {
        if (!IsDipSafeToUnload())
        {
            return false;
        }


        BYTE* trampoline =
            gDipTrampoline;


        gDipTrampoline =
            nullptr;


        gDipTarget =
            nullptr;


        gDipOriginal =
            nullptr;


        if (trampoline != nullptr)
        {
            VirtualFree(
                trampoline,
                0,
                MEM_RELEASE
            );
        }


        return true;
    }


    // ============================================================
    // DIP DEBUG
    // ============================================================

    inline void PrintDipStatus()
    {
        std::cout
            << "\n========================================\n"
            << "             D3D9 DIP HOOK\n"
            << "========================================\n"
            << "[Installed]       "
            << (
                IsDipInstalled()
                    ?
                    "YES"
                    :
                    "NO"
            )
            << '\n'
            << "[Total DIPs]      "
            << gDipHits.load()
            << '\n'
            << "[Context DIPs]    "
            << gDipContextHits.load()
            << '\n'
            << "[Blocked DIPs]    "
            << gDipBlockedHits.load()
            << '\n'
            << "[In flight]       "
            << gDipInFlight.load()
            << '\n'
            << "[Resolve faults]  "
            << gDipResolveFaults.load()
            << '\n'
            << "[Candidate changes] "
            << gDipCandidateChanges.load()
            << '\n'
            << "[Rejected cand.]  "
            << gDipRejectedCandidates.load()
            << '\n'
            << "[Validation fail] "
            << gDipValidationFailures.load()
            << '\n'
            << "[Unhook fail]     "
            << gDipUnhookFailures.load()
            << '\n'
            << "[TLS overflows]   "
            << gRenderContextOverflows.load()
            << '\n'
            << "[Glow draws]      "
            << gGlowDraws.load()
            << '\n'
            << "[Capture fail]    "
            << gGlowCaptureFailures.load()
            << '\n'
            << "[Apply fail]      "
            << gGlowApplyFailures.load()
            << '\n'
            << "[Restore fail]    "
            << gGlowRestoreFailures.load()
            << '\n'
            << "[Glow faulted]    "
            << (
                gGlowFaulted.load()
                    ?
                    "YES"
                    :
                    "NO"
            )
            << '\n'
            << "[Stage4 busy]     "
            << gGlowStageBusySkips.load()
            << '\n';


        std::cout
            << std::hex
            << std::uppercase
            << std::showbase
            << "[Device]          "
            << gDipDevice.load()
            << '\n'
            << "[Candidate DIP]   "
            << gDipCandidate.load()
            << '\n'
            << "[Hook Target]     "
            << reinterpret_cast<uintptr_t>(
                gDipTarget
            )
            << '\n'
            << "[Trampoline]      "
            << reinterpret_cast<uintptr_t>(
                gDipTrampoline
            )
            << '\n'
            << "[Last Actor]      "
            << gDipLastActor.load()
            << '\n'
            << "[Last BLOCKED]    "
            << gDipLastBlockedActor.load()
            << '\n'
            << "[Last Glow]       "
            << gGlowLastActor.load()
            << '\n'
            << std::dec
            << std::nouppercase
            << std::noshowbase
            << "========================================\n\n";
    }


    // ============================================================
    // BRIDGE DEBUG
    // ============================================================

    inline void PrintBridgeStatus()
    {
        std::cout
            << "\n========================================\n"
            << "          UE2 RENDER BRIDGE\n"
            << "========================================\n"
            << "[Installed]       "
            << (
                IsBridgeInstalled()
                    ?
                    "YES"
                    :
                    "NO"
            )
            << '\n'
            << "[Hits]            "
            << gBridgeHits.load()
            << '\n'
            << "[Observed ON]     "
            << gBridgeObservedEnabled.load()
            << '\n'
            << "[Blocked matches] "
            << gBridgeBlockedMatches.load()
            << '\n'
            << "[In flight]       "
            << gBridgeInFlight.load()
            << '\n'
            << "[Validation fail] "
            << gBridgeValidationFailures.load()
            << '\n'
            << "[Unhook fail]     "
            << gBridgeUnhookFailures.load()
            << '\n';


        std::cout
            << std::hex
            << std::uppercase
            << std::showbase;


        std::cout
            << "[Target]          "
            << reinterpret_cast<uintptr_t>(
                gBridgeTarget
            )
            << '\n'
            << "[Stub]            "
            << reinterpret_cast<uintptr_t>(
                gBridgeStub
            )
            << '\n'
            << "[Last Actor]      "
            << gBridgeLastActor.load()
            << '\n'
            << "[Last BLOCKED]    "
            << gBridgeLastBlockedActor.load()
            << '\n';


        std::cout
            << std::dec
            << std::nouppercase
            << std::noshowbase
            << "========================================\n\n";
    }


    // ============================================================
    // DEBUG
    // ============================================================

    inline void PrintStatus()
    {
        Snapshot snapshot;


        const bool stable =
            TryReadSnapshot(
                snapshot
            );


        const ULONGLONG now =
            GetTickCount64();


        const ULONGLONG last =
            gLastPublishAt.load(
                std::memory_order_acquire
            );


        const ULONGLONG age =
            last == 0 ||
            now < last
                ?
                0
                :
                now - last;


        std::cout
            << "\n========================================\n"
            << "             GLOW SNAPSHOT\n"
            << "========================================\n"
            << "[Enabled]        "
            << (
                IsEnabled()
                    ?
                    "YES"
                    :
                    "NO"
            )
            << '\n'
            << "[Stable]         "
            << (
                stable
                    ?
                    "YES"
                    :
                    "NO"
            )
            << '\n'
            << "[Sequence]       "
            << snapshot.sequence
            << '\n'
            << "[Blocked Pawns]  "
            << snapshot.count
            << '\n'
            << "[Publishes]      "
            << gPublishCount.load(
                std::memory_order_acquire
            )
            << '\n'
            << "[Overflows]      "
            << gOverflowCount.load(
                std::memory_order_acquire
            )
            << '\n'
            << "[Snapshot age]   "
            << age
            << " ms\n";


        if (
            stable &&
            snapshot.count > 0
            )
        {
            std::cout
                << std::hex
                << std::uppercase
                << std::showbase;


            for (
                size_t index = 0;
                index < snapshot.count;
                ++index
                )
            {
                std::cout
                    << "  ["
                    << std::dec
                    << index
                    << "] "
                    << std::hex
                    << snapshot.pawns[index]
                    << '\n';
            }


            std::cout
                << std::dec
                << std::nouppercase
                << std::noshowbase;
        }


        std::cout
            << "========================================\n\n";
    }
}