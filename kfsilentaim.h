#pragma once

#include <Windows.h>

#include <cstddef>
#include <cstdint>


#include <atomic>

#include <limits>
#include <cstring>
#include <iostream>
#include <string>

#include "kfmemory.h"
#include "kfoffsets.h"
#include "kffireregistry.h"
#include "kftargetsnapshot.h"
#include "kfaimbot.h"


namespace KFSilentAim
{
#if !defined(_M_IX86)
#error KFSilentAim requires Killing Floor Win32/x86.
#endif


    // ============================================================
    // RUNTIME FINDINGS
    //
    // Confirmed dynamically in this KF build:
    //
    // UObject::CallFunction + 0x1E2
    //     Parameters for the target UFunction are already prepared.
    //
    // KFmod.KFFire.DoTrace:
    //
    //     +0x00 Start : FVector
    //     +0x0C Dir   : FRotator
    //
    // Full reflected PropertySize:
    //
    //     0x84 bytes
    //
    // IMPORTANT:
    //
    // The runtime UObject address of KFFire.DoTrace must NOT be
    // hardcoded. It must be resolved dynamically through UE2
    // reflection/GObjHash on each game session.
    // ============================================================

    namespace Config
    {
        constexpr uintptr_t CallFunctionRva =
            0x000326D0;

        constexpr size_t CallFunctionHookLength =
            5;

        constexpr ULONGLONG PassiveHookUnloadGraceMs =
            150;


        constexpr uintptr_t CallFunctionParamsReadyOffset =
            0x01E2;

        constexpr size_t ParamsReadyHookLength =
            6;

        constexpr size_t ParamsReadyStubSize =
            64;

        constexpr ULONGLONG ParamsReadyUnloadGraceMs =
            150;

        constexpr size_t DoTraceStartOffset =
            0x00;

        constexpr size_t DoTraceDirOffset =
            0x0C;

        constexpr size_t DoTracePropertySize =
            0x84;
    }


    struct FVectorRaw
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };


    struct FRotatorRaw
    {
        int32_t pitch = 0;
        int32_t yaw = 0;
        int32_t roll = 0;
    };


    // Solo representa el prefijo de parametros que necesitamos.
    //
    // No intentamos modelar por ahora los 0x84 bytes completos
    // de KFFire.DoTrace.
    struct DoTraceParamsPrefix
    {
        FVectorRaw start;
        FRotatorRaw dir;
    };


    static_assert(
        sizeof(FVectorRaw) == 0x0C,
        "Unexpected FVectorRaw size."
    );


    static_assert(
        sizeof(FRotatorRaw) == 0x0C,
        "Unexpected FRotatorRaw size."
    );


    static_assert(
        offsetof(DoTraceParamsPrefix, start) ==
            Config::DoTraceStartOffset,
        "Unexpected DoTrace Start offset."
    );


    static_assert(
        offsetof(DoTraceParamsPrefix, dir) ==
            Config::DoTraceDirOffset,
        "Unexpected DoTrace Dir offset."
    );

    // ============================================================
    // RESOLVE KFmod.KFFire.DoTrace
    //
    // No runtime UObject address is hardcoded.
    // ============================================================

    inline uintptr_t ResolveDoTraceFunction()
    {
        constexpr size_t BucketCount =
            0x1000;

        constexpr size_t MaxChainLength =
            512;

        constexpr uintptr_t OuterOffset =
            0x1C;

        constexpr uintptr_t PropertySizeOffset =
            0x44;


        const uintptr_t coreBase =
            KFMemory::CoreBase();


        if (coreBase == 0)
        {
            return 0;
        }


        const uintptr_t hashBase =
            coreBase +
            KFOffsets::Global::GObjHash;


        for (
            size_t bucket = 0;
            bucket < BucketCount;
            ++bucket
            )
        {
            uintptr_t object = 0;


            if (!KFMemory::Read(
                hashBase +
                bucket * sizeof(uintptr_t),
                object
            ))
            {
                continue;
            }


            size_t chainLength = 0;


            while (
                object != 0 &&
                chainLength <
                    MaxChainLength
                )
            {
                if (
                    KFMemory::GetObjectName(
                        object
                    ) == "DoTrace" &&
                    KFMemory::GetClassName(
                        object
                    ) == "Function"
                    )
                {
                    uintptr_t outer = 0;


                    if (
                        KFMemory::Read(
                            object +
                            OuterOffset,
                            outer
                        ) &&
                        outer != 0 &&
                        KFMemory::GetObjectName(
                            outer
                        ) == "KFFire"
                        )
                    {
                        uint32_t propertySize = 0;


                        if (
                            KFMemory::Read(
                                object +
                                PropertySizeOffset,
                                propertySize
                            ) &&
                            propertySize ==
                                Config::DoTracePropertySize
                            )
                        {
                            return object;
                        }
                    }
                }


                uintptr_t next = 0;


                if (!KFMemory::Read(
                    object +
                    KFOffsets::UObject::HashNext,
                    next
                ))
                {
                    break;
                }


                if (
                    next == 0 ||
                    next == object
                    )
                {
                    break;
                }


                object =
                    next;


                ++chainLength;
            }
        }


        return 0;
    }


    inline void PrintDiscoveryStatus()
    {
        const uintptr_t doTrace =
            ResolveDoTraceFunction();


        std::cout
            << "[SILENT AIM] KFFire.DoTrace: ";


        if (doTrace == 0)
        {
            std::cout
                << "NOT FOUND\n";

            return;
        }


        std::cout
            << std::hex
            << std::uppercase
            << std::showbase
            << doTrace
            << std::dec
            << std::nouppercase
            << std::noshowbase
            << '\n';
    }

    // ============================================================
    // CALLFUNCTION HOOK SITE
    //
    // Core.dll + 0x326D0
    //
    // 55             push ebp
    // 8D 6C 24 94    lea ebp,[esp-6C]
    //
    // Total: 5 bytes exactos.
    // ============================================================

    inline uintptr_t GetCallFunctionAddress()
    {
        const uintptr_t coreBase =
            KFMemory::CoreBase();

        if (coreBase == 0)
        {
            return 0;
        }

        return
            coreBase +
            Config::CallFunctionRva;
    }


    inline bool ValidateCallFunctionSite()
    {
        const uintptr_t address =
            GetCallFunctionAddress();

        if (address == 0)
        {
            return false;
        }

        constexpr BYTE expected[
            Config::CallFunctionHookLength
        ] =
        {
            0x55,
            0x8D,
            0x6C,
            0x24,
            0x94
        };

        BYTE actual[
            Config::CallFunctionHookLength
        ]{};

        for (
            size_t i = 0;
            i < Config::CallFunctionHookLength;
            ++i
            )
        {
            if (!KFMemory::Read(
                address + i,
                actual[i]
            ))
            {
                return false;
            }
        }

        return
            std::memcmp(
                actual,
                expected,
                sizeof(expected)
            ) == 0;
    }


    inline void PrintHookSiteStatus()
    {
        const uintptr_t address =
            GetCallFunctionAddress();

        const bool valid =
            ValidateCallFunctionSite();

        std::cout
            << "[SILENT AIM] CallFunction site: "
            << (
                valid
                    ? "OK"
                    : "INVALID"
            );

        if (address != 0)
        {
            std::cout
                << " @ "
                << std::hex
                << std::uppercase
                << std::showbase
                << address
                << std::dec
                << std::nouppercase
                << std::noshowbase;
        }

        std::cout << '\n';
    }

    // ============================================================
    // PASSIVE CALLFUNCTION HOOK
    //
    // IMPORTANT:
    //
    // This hook currently ONLY observes calls.
    // It does NOT modify Start, Dir or any game state.
    //
    // Runtime ABI confirmed:
    //
    // ECX      = UObject / fire mode instance
    // [ESP+04] = FFrame*
    // [ESP+08] = Result
    // [ESP+0C] = UFunction*
    // ============================================================

    using CallFunctionFn =
        void(__thiscall*)(
            void* object,
            void* frame,
            void* result,
            void* function
        );


    inline std::atomic_bool
        gPassiveHookInstalled{ false };


    inline std::atomic<unsigned long long>
        gCallFunctionHits{ 0 };


    inline std::atomic<unsigned long long>
        gDoTraceHits{ 0 };


    inline std::atomic<unsigned long long>
        gPassiveHookInFlight{ 0 };


    inline std::atomic_bool
        gPassiveHookUninstallRequested{ false };


    inline std::atomic<ULONGLONG>
        gPassiveHookUnhookCompletedAt{ 0 };


    inline std::atomic<unsigned long long>
        gPassiveHookUnhookFailures{ 0 };


    inline uintptr_t
        gDoTraceFunction{ 0 };


    inline BYTE*
        gCallFunctionTarget{ nullptr };


    inline BYTE*
        gCallFunctionTrampoline{ nullptr };


    inline CallFunctionFn
        gCallFunctionOriginal{ nullptr };


    inline BYTE
        gCallFunctionOriginalBytes[
            Config::CallFunctionHookLength
        ]{};


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


    inline void __fastcall HookedCallFunction(
        void* object,
        void* /* edx */,
        void* frame,
        void* result,
        void* function
    )
    {
        gPassiveHookInFlight.fetch_add(
            1,
            std::memory_order_acq_rel
        );


        gCallFunctionHits.fetch_add(
            1,
            std::memory_order_relaxed
        );


        if (
            reinterpret_cast<uintptr_t>(
                function
            ) ==
            gDoTraceFunction
            )
        {
            gDoTraceHits.fetch_add(
                1,
                std::memory_order_relaxed
            );
        }


        const CallFunctionFn original =
            gCallFunctionOriginal;


        if (original != nullptr)
        {
            original(
                object,
                frame,
                result,
                function
            );
        }


        gPassiveHookInFlight.fetch_sub(
            1,
            std::memory_order_acq_rel
        );
    }


    inline bool InstallPassiveHook()
    {
        // NOTE:
        // This function is intentionally NOT called yet.
        //
        // First we only compile and validate the implementation.

        if (
            gPassiveHookInstalled.load(
                std::memory_order_acquire
            )
            )
        {
            return true;
        }


        if (!ValidateCallFunctionSite())
        {
            return false;
        }


        const uintptr_t doTrace =
            ResolveDoTraceFunction();


        if (doTrace == 0)
        {
            return false;
        }


        BYTE* target =
            reinterpret_cast<BYTE*>(
                GetCallFunctionAddress()
            );


        if (target == nullptr)
        {
            return false;
        }


        constexpr size_t TrampolineSize =
            Config::CallFunctionHookLength +
            5;


        BYTE* trampoline =
            reinterpret_cast<BYTE*>(
                VirtualAlloc(
                    nullptr,
                    TrampolineSize,
                    MEM_COMMIT |
                    MEM_RESERVE,
                    PAGE_READWRITE
                )
            );


        if (trampoline == nullptr)
        {
            return false;
        }


        std::memcpy(
            gCallFunctionOriginalBytes,
            target,
            Config::CallFunctionHookLength
        );


        std::memcpy(
            trampoline,
            gCallFunctionOriginalBytes,
            Config::CallFunctionHookLength
        );


        if (!WriteRelativeJump(
            trampoline +
                Config::CallFunctionHookLength,
            target +
                Config::CallFunctionHookLength
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
            TrampolineSize,
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
            TrampolineSize
        );


        // Revalidate immediately before patching.
        if (!ValidateCallFunctionSite())
        {
            VirtualFree(
                trampoline,
                0,
                MEM_RELEASE
            );

            return false;
        }


        gDoTraceFunction =
            doTrace;


        gCallFunctionTarget =
            target;


        gCallFunctionTrampoline =
            trampoline;


        gCallFunctionOriginal =
            reinterpret_cast<CallFunctionFn>(
                trampoline
            );


        gCallFunctionHits.store(
            0,
            std::memory_order_relaxed
        );


        gDoTraceHits.store(
            0,
            std::memory_order_relaxed
        );


        gPassiveHookInFlight.store(
            0,
            std::memory_order_relaxed
        );


        gPassiveHookUninstallRequested.store(
            false,
            std::memory_order_release
        );


        gPassiveHookUnhookCompletedAt.store(
            0,
            std::memory_order_release
        );


        gPassiveHookUnhookFailures.store(
            0,
            std::memory_order_relaxed
        );


        if (!WriteRelativeJump(
            target,
            reinterpret_cast<void*>(
                &HookedCallFunction
            )
        ))
        {
            gCallFunctionOriginal =
                nullptr;

            gCallFunctionTarget =
                nullptr;

            gCallFunctionTrampoline =
                nullptr;

            gDoTraceFunction =
                0;


            VirtualFree(
                trampoline,
                0,
                MEM_RELEASE
            );


            return false;
        }


        gPassiveHookInstalled.store(
            true,
            std::memory_order_release
        );


        return true;
    }


    inline void PrintPassiveHookStatus()
    {
        std::cout
            << "\n========================================\n"
            << "       SILENT AIM PASSIVE HOOK\n"
            << "========================================\n"
            << "[Installed]       "
            << (
                gPassiveHookInstalled.load(
                    std::memory_order_acquire
                )
                    ? "YES"
                    : "NO"
            )
            << '\n'
            << "[CallFunction]     "
            << gCallFunctionHits.load(
                std::memory_order_relaxed
            )
            << '\n'
            << "[DoTrace hits]     "
            << gDoTraceHits.load(
                std::memory_order_relaxed
            )
            << '\n'
            << "========================================\n";
    }

    // ============================================================
    // PASSIVE HOOK SAFE UNLOAD
    // ============================================================

    inline bool IsPassiveHookInstalled()
    {
        return
            gPassiveHookInstalled.load(
                std::memory_order_acquire
            );
    }


    inline bool RestorePassiveHookBytes()
    {
        if (gCallFunctionTarget == nullptr)
        {
            return true;
        }


        DWORD oldProtection =
            0;


        if (!VirtualProtect(
            gCallFunctionTarget,
            Config::CallFunctionHookLength,
            PAGE_EXECUTE_READWRITE,
            &oldProtection
        ))
        {
            return false;
        }


        std::memcpy(
            gCallFunctionTarget,
            gCallFunctionOriginalBytes,
            Config::CallFunctionHookLength
        );


        FlushInstructionCache(
            GetCurrentProcess(),
            gCallFunctionTarget,
            Config::CallFunctionHookLength
        );


        DWORD ignored =
            0;


        VirtualProtect(
            gCallFunctionTarget,
            Config::CallFunctionHookLength,
            oldProtection,
            &ignored
        );


        return true;
    }


    inline void RequestPassiveHookUninstall()
    {
        if (IsPassiveHookInstalled())
        {
            gPassiveHookUninstallRequested.store(
                true,
                std::memory_order_release
            );
        }
    }


    inline bool PassiveHookUninstallRequested()
    {
        return
            gPassiveHookUninstallRequested.load(
                std::memory_order_acquire
            );
    }


    inline bool UninstallPassiveHookNow()
    {
        if (!IsPassiveHookInstalled())
        {
            return true;
        }


        if (!RestorePassiveHookBytes())
        {
            gPassiveHookUnhookFailures.fetch_add(
                1,
                std::memory_order_relaxed
            );

            return false;
        }


        // No new entries can arrive through our JMP after this.
        gPassiveHookInstalled.store(
            false,
            std::memory_order_release
        );


        gPassiveHookUninstallRequested.store(
            false,
            std::memory_order_release
        );


        gPassiveHookUnhookCompletedAt.store(
            GetTickCount64(),
            std::memory_order_release
        );


        return true;
    }


    inline bool IsPassiveHookSafeToUnload()
    {
        if (IsPassiveHookInstalled())
        {
            return false;
        }


        if (
            gPassiveHookInFlight.load(
                std::memory_order_acquire
            ) != 0
            )
        {
            return false;
        }


        const ULONGLONG completedAt =
            gPassiveHookUnhookCompletedAt.load(
                std::memory_order_acquire
            );


        // Never installed.
        if (completedAt == 0)
        {
            return
                gCallFunctionTrampoline ==
                nullptr;
        }


        return
            GetTickCount64() -
                completedAt >=
            Config::PassiveHookUnloadGraceMs;
    }


    inline bool FinalizePassiveHook()
    {
        if (!IsPassiveHookSafeToUnload())
        {
            return false;
        }


        BYTE* trampoline =
            gCallFunctionTrampoline;


        gCallFunctionTrampoline =
            nullptr;


        gCallFunctionTarget =
            nullptr;


        gCallFunctionOriginal =
            nullptr;


        gDoTraceFunction =
            0;


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
    // PARAMS READY SITE
    //
    // CE confirmed:
    //
    // CallFunction + 0x1E2
    //
    // 8B 56 0C    mov edx,[esi+0C]
    // 42          inc edx
    // 8B C2       mov eax,edx
    //
    // 6 complete bytes.
    //
    // At this point:
    //
    // [EBP+74] = prepared parameter buffer
    // [EBP+7C] = UFunction*
    // ============================================================

    inline uintptr_t GetParamsReadyAddress()
    {
        const uintptr_t callFunction =
            GetCallFunctionAddress();

        if (callFunction == 0)
        {
            return 0;
        }

        return
            callFunction +
            Config::CallFunctionParamsReadyOffset;
    }


    inline bool ValidateParamsReadySite()
    {
        const uintptr_t address =
            GetParamsReadyAddress();

        if (address == 0)
        {
            return false;
        }


        constexpr BYTE expected[6] =
        {
            0x8B, 0x56, 0x0C,
            0x42,
            0x8B, 0xC2
        };


        BYTE actual[6]{};


        for (
            size_t i = 0;
            i < sizeof(actual);
            ++i
            )
        {
            if (!KFMemory::Read(
                address + i,
                actual[i]
            ))
            {
                return false;
            }
        }


        return
            std::memcmp(
                actual,
                expected,
                sizeof(expected)
            ) == 0;
    }


    inline void PrintParamsReadyStatus()
    {
        const uintptr_t address =
            GetParamsReadyAddress();

        const bool valid =
            ValidateParamsReadySite();


        std::cout
            << "[SILENT AIM] Params-ready site: "
            << (
                valid
                    ? "OK"
                    : "INVALID"
            );


        if (address != 0)
        {
            std::cout
                << " @ "
                << std::hex
                << std::uppercase
                << std::showbase
                << address
                << std::dec
                << std::nouppercase
                << std::noshowbase;
        }


        std::cout << '\n';
    }

    // ============================================================
    // PARAMS-READY PASSIVE CAPTURE
    //
    // READ ONLY.
    //
    // At CallFunction + 0x1E2:
    //
    // [EBP+74] = prepared parameter buffer
    // [EBP+7C] = UFunction*
    //
    // For KFmod.KFFire.DoTrace:
    //
    // params + 0x00 = Start
    // params + 0x0C = Dir
    // ============================================================

    inline std::atomic_bool
        gParamsReadyHookInstalled{ false };


    inline std::atomic<unsigned long long>
        gParamsReadyHits{ 0 };


    inline std::atomic<unsigned long long>
        gParamsReadyInFlight{ 0 };


    inline std::atomic_bool
        gParamsReadyUninstallRequested{ false };


    inline std::atomic<ULONGLONG>
        gParamsReadyUnhookCompletedAt{ 0 };


    inline std::atomic<unsigned long long>
        gParamsReadyUnhookFailures{ 0 };


    inline std::atomic<uintptr_t>
        gLastParamsAddress{ 0 };


    inline std::atomic<uint32_t>
        gLastStartXBits{ 0 };

    inline std::atomic<uint32_t>
        gLastStartYBits{ 0 };

    inline std::atomic<uint32_t>
        gLastStartZBits{ 0 };


    inline std::atomic<int32_t>
        gLastDirPitch{ 0 };

    inline std::atomic<int32_t>
        gLastDirYaw{ 0 };

    inline std::atomic<int32_t>
        gLastDirRoll{ 0 };

    inline std::atomic_bool
        gLastTargetEligible{ false };


    inline std::atomic_bool
        gLastCalculatedDirValid{ false };


    inline std::atomic<uintptr_t>
        gLastTargetPawn{ 0 };


    inline std::atomic<ULONGLONG>
        gLastTargetAgeMs{ 0 };


    inline std::atomic<float>
        gLastTargetHeadX{ 0.0f };

    inline std::atomic<float>
        gLastTargetHeadY{ 0.0f };

    inline std::atomic<float>
        gLastTargetHeadZ{ 0.0f };


    inline std::atomic<int32_t>
        gLastCalculatedPitch{ 0 };

    inline std::atomic<int32_t>
        gLastCalculatedYaw{ 0 };

    // ============================================================
    // SILENT AIM FEATURE STATE
    // ============================================================

    inline std::atomic_bool
        gSilentAimEnabled{ false };


    inline std::atomic<unsigned long long>
        gSilentAimRedirects{ 0 };


    inline std::atomic<unsigned long long>
        gSilentAimWriteFailures{ 0 };


    inline std::atomic<unsigned long long>
        gSilentAimSkipped{ 0 };




    inline BYTE*
        gParamsReadyTarget{ nullptr };


    inline BYTE*
        gParamsReadyStub{ nullptr };


    inline BYTE
        gParamsReadyOriginalBytes[
            Config::ParamsReadyHookLength
        ]{};


    inline float BitsToFloat(
        uint32_t bits
    )
    {
        float value = 0.0f;

        std::memcpy(
            &value,
            &bits,
            sizeof(value)
        );

        return value;
    }


    // Called by the generated x86 stub.
    //
    // IMPORTANT:
    // Keep this function simple. No floating point calculations.
        struct ParamsReadyInFlightGuard
    {
        ParamsReadyInFlightGuard()
        {
            gParamsReadyInFlight.fetch_add(
                1,
                std::memory_order_acq_rel
            );
        }


        ~ParamsReadyInFlightGuard()
        {
            gParamsReadyInFlight.fetch_sub(
                1,
                std::memory_order_acq_rel
            );
        }
    };

    // ============================================================
    // SILENT AIM CONTROL
    // ============================================================

    inline bool IsSilentAimEnabled()
    {
        return
            gSilentAimEnabled.load(
                std::memory_order_acquire
            );
    }


    inline void SetSilentAimEnabled(
        bool enabled
    )
    {
        gSilentAimEnabled.store(
            enabled,
            std::memory_order_release
        );
    }


    inline bool ToggleSilentAim()
    {
        const bool enabled =
            !IsSilentAimEnabled();


        SetSilentAimEnabled(
            enabled
        );


        return enabled;
    }

inline void __cdecl ObserveParamsReady(
        uintptr_t frameBase
    )
    {
        ParamsReadyInFlightGuard inFlightGuard;
        if (
            frameBase == 0 ||
            gDoTraceFunction == 0
            )
        {
            return;
        }


        uintptr_t function = 0;


        if (!KFMemory::Read(
            frameBase + 0x7C,
            function
        ))
        {
            return;
        }


        if (!KFFireRegistry::IsDoTraceFunction(function))
        {
            return;
        }


        uintptr_t params = 0;


        if (!KFMemory::Read(
            frameBase + 0x74,
            params
        ))
        {
            return;
        }


        if (params == 0)
        {
            return;
        }


        uint32_t xBits = 0;
        uint32_t yBits = 0;
        uint32_t zBits = 0;

        int32_t pitch = 0;
        int32_t yaw = 0;
        int32_t roll = 0;


        if (
            !KFMemory::Read(
                params + 0x00,
                xBits
            ) ||
            !KFMemory::Read(
                params + 0x04,
                yBits
            ) ||
            !KFMemory::Read(
                params + 0x08,
                zBits
            ) ||
            !KFMemory::Read(
                params + 0x0C,
                pitch
            ) ||
            !KFMemory::Read(
                params + 0x10,
                yaw
            ) ||
            !KFMemory::Read(
                params + 0x14,
                roll
            )
            )
        {
            return;
        }


        gLastParamsAddress.store(
            params,
            std::memory_order_relaxed
        );


        gLastStartXBits.store(
            xBits,
            std::memory_order_relaxed
        );

        gLastStartYBits.store(
            yBits,
            std::memory_order_relaxed
        );

        gLastStartZBits.store(
            zBits,
            std::memory_order_relaxed
        );


        gLastDirPitch.store(
            pitch,
            std::memory_order_relaxed
        );

        gLastDirYaw.store(
            yaw,
            std::memory_order_relaxed
        );

        gLastDirRoll.store(
            roll,
            std::memory_order_relaxed
        );


        // ========================================================
        // SILENT AIM MATH - READ ONLY
        //
        // No escribe en params.
        // No modifica la camara.
        // ========================================================

        KFTargetSnapshot::Snapshot targetSnapshot;


        const bool targetEligible =
            KFTargetSnapshot::TryReadStrictVisible(
                targetSnapshot
            );


        gLastTargetEligible.store(
            targetEligible,
            std::memory_order_relaxed
        );


        gLastCalculatedDirValid.store(
            false,
            std::memory_order_relaxed
        );


        if (targetEligible)
        {
            gLastTargetPawn.store(
                targetSnapshot.pawn,
                std::memory_order_relaxed
            );


            gLastTargetAgeMs.store(
                targetSnapshot.ageMs,
                std::memory_order_relaxed
            );


            gLastTargetHeadX.store(
                targetSnapshot.headWorld.x,
                std::memory_order_relaxed
            );

            gLastTargetHeadY.store(
                targetSnapshot.headWorld.y,
                std::memory_order_relaxed
            );

            gLastTargetHeadZ.store(
                targetSnapshot.headWorld.z,
                std::memory_order_relaxed
            );


            // Reutilizamos exactamente la matematica del aimbot,
            // pero el origen es DoTrace.Start.
            KFCamera::Snapshot shotCamera{};


            shotCamera.eyeLocation =
            {
                BitsToFloat(xBits),
                BitsToFloat(yBits),
                BitsToFloat(zBits)
            };


            const KFAimbot::DesiredRotation desired =
                KFAimbot::CalculateDesiredRotationToPoint(
                    shotCamera,
                    targetSnapshot.headWorld
                );


            if (desired.valid)
            {
                gLastCalculatedPitch.store(
                    desired.pitch,
                    std::memory_order_relaxed
                );


                gLastCalculatedYaw.store(
                    desired.yaw,
                    std::memory_order_relaxed
                );


                gLastCalculatedDirValid.store(
                    true,
                    std::memory_order_release
                );


                // ================================================
                // SILENT AIM ACTIVE PATH
                //
                // Solo cambiamos el parametro local Dir de
                // KFFire.DoTrace.
                //
                // NO escribimos Pitch/Yaw del Controller.
                // NO movemos la camara.
                // ================================================

                if (IsSilentAimEnabled())
                {
                    const FRotatorRaw redirectedDir
                    {
                        desired.pitch,
                        desired.yaw,
                        0
                    };


                    if (KFMemory::Write(
                        params +
                            Config::DoTraceDirOffset,
                        redirectedDir
                    ))
                    {
                        gSilentAimRedirects.fetch_add(
                            1,
                            std::memory_order_relaxed
                        );
                    }
                    else
                    {
                        gSilentAimWriteFailures.fetch_add(
                            1,
                            std::memory_order_relaxed
                        );
                    }
                }
            }
        }
        else
        {
            gLastTargetPawn.store(
                0,
                std::memory_order_relaxed
            );


            gLastTargetAgeMs.store(
                0,
                std::memory_order_relaxed
            );


            if (IsSilentAimEnabled())
            {
                gSilentAimSkipped.fetch_add(
                    1,
                    std::memory_order_relaxed
                );
            }
        }


        gParamsReadyHits.fetch_add(
            1,
            std::memory_order_relaxed
        );
    }


    inline bool WriteParamsReadyJump(
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


        BYTE patch[
            Config::ParamsReadyHookLength
        ]{};


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


        // Sixth byte because the validated instruction boundary
        // at +1E2 is exactly six bytes.
        patch[5] =
            0x90;


        DWORD oldProtection = 0;


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


        DWORD ignored = 0;


        VirtualProtect(
            source,
            sizeof(patch),
            oldProtection,
            &ignored
        );


        return true;
    }


    inline bool BuildParamsReadyStub(
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


        size_t offset = 0;


        auto emit8 =
            [&](BYTE value)
            {
                stub[offset++] =
                    value;
            };


        auto emit32 =
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


        // Preserve flags + GP registers.
        emit8(0x9C); // pushfd
        emit8(0x60); // pushad


        // push ebp
        //
        // EBP is the CallFunction frame base that CE validated.
        emit8(0x55);


        // mov eax, ObserveParamsReady
        emit8(0xB8);

        emit32(
            static_cast<uint32_t>(
                reinterpret_cast<uintptr_t>(
                    &ObserveParamsReady
                )
            )
        );


        // call eax
        emit8(0xFF);
        emit8(0xD0);


        // add esp,4
        emit8(0x83);
        emit8(0xC4);
        emit8(0x04);


        emit8(0x61); // popad
        emit8(0x9D); // popfd


        if (
            offset +
            Config::ParamsReadyHookLength +
            5 >
            Config::ParamsReadyStubSize
            )
        {
            return false;
        }


        // Replay the six bytes displaced from Core.dll.
        std::memcpy(
            stub + offset,
            gParamsReadyOriginalBytes,
            Config::ParamsReadyHookLength
        );


        offset +=
            Config::ParamsReadyHookLength;


        // jmp CallFunction+1E8
        return
            WriteRelativeJump(
                stub + offset,
                returnAddress
            );
    }


    inline bool InstallParamsReadyHook()
    {
        // Intentionally NOT connected to startup yet.

        if (
            gParamsReadyHookInstalled.load(
                std::memory_order_acquire
            )
            )
        {
            return true;
        }


        if (!ValidateParamsReadySite())
        {
            return false;
        }


        if (gDoTraceFunction == 0)
        {
            gDoTraceFunction =
                ResolveDoTraceFunction();
        }


        if (gDoTraceFunction == 0)
        {
            return false;
        }


        BYTE* target =
            reinterpret_cast<BYTE*>(
                GetParamsReadyAddress()
            );


        if (target == nullptr)
        {
            return false;
        }


        std::memcpy(
            gParamsReadyOriginalBytes,
            target,
            Config::ParamsReadyHookLength
        );


        BYTE* stub =
            reinterpret_cast<BYTE*>(
                VirtualAlloc(
                    nullptr,
                    Config::ParamsReadyStubSize,
                    MEM_COMMIT |
                    MEM_RESERVE,
                    PAGE_READWRITE
                )
            );


        if (stub == nullptr)
        {
            return false;
        }


        if (!BuildParamsReadyStub(
            stub,
            target +
                Config::ParamsReadyHookLength
        ))
        {
            VirtualFree(
                stub,
                0,
                MEM_RELEASE
            );

            return false;
        }


        DWORD oldProtection = 0;


        if (!VirtualProtect(
            stub,
            Config::ParamsReadyStubSize,
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
            Config::ParamsReadyStubSize
        );


        if (!ValidateParamsReadySite())
        {
            VirtualFree(
                stub,
                0,
                MEM_RELEASE
            );

            return false;
        }


        gParamsReadyTarget =
            target;


        gParamsReadyStub =
            stub;


        gParamsReadyHits.store(
            0,
            std::memory_order_relaxed
        );


        gParamsReadyInFlight.store(
            0,
            std::memory_order_relaxed
        );


        gParamsReadyUninstallRequested.store(
            false,
            std::memory_order_release
        );


        gParamsReadyUnhookCompletedAt.store(
            0,
            std::memory_order_release
        );


        gParamsReadyUnhookFailures.store(
            0,
            std::memory_order_relaxed
        );


        if (!WriteParamsReadyJump(
            target,
            stub
        ))
        {
            gParamsReadyTarget =
                nullptr;

            gParamsReadyStub =
                nullptr;


            VirtualFree(
                stub,
                0,
                MEM_RELEASE
            );


            return false;
        }


        gParamsReadyHookInstalled.store(
            true,
            std::memory_order_release
        );


        return true;
    }


    inline void PrintParamsCaptureStatus()
    {
        const unsigned long long hits =
            gParamsReadyHits.load(
                std::memory_order_relaxed
            );


        std::cout
            << "\n========================================\n"
            << "       SILENT AIM PARAM CAPTURE\n"
            << "========================================\n"
            << "[Installed]       "
            << (
                gParamsReadyHookInstalled.load(
                    std::memory_order_acquire
                )
                    ? "YES"
                    : "NO"
            )
            << '\n'
            << "[DoTrace params]   "
            << hits
            << '\n';


        if (hits != 0)
        {
            std::cout
                << std::fixed
                << std::setprecision(3)
                << "[Start]            "
                << BitsToFloat(
                    gLastStartXBits.load()
                )
                << ", "
                << BitsToFloat(
                    gLastStartYBits.load()
                )
                << ", "
                << BitsToFloat(
                    gLastStartZBits.load()
                )
                << '\n'
                << std::defaultfloat
                << "[Dir]              "
                << gLastDirPitch.load()
                << ", "
                << gLastDirYaw.load()
                << ", "
                << gLastDirRoll.load()
                << '\n';
        }


        std::cout
            << "========================================\n";
    }

    // ============================================================
    // PARAMS-READY SAFE UNLOAD
    // ============================================================

    inline bool IsParamsReadyHookInstalled()
    {
        return
            gParamsReadyHookInstalled.load(
                std::memory_order_acquire
            );
    }


    inline bool RestoreParamsReadyHookBytes()
    {
        if (gParamsReadyTarget == nullptr)
        {
            return true;
        }


        DWORD oldProtection = 0;


        if (!VirtualProtect(
            gParamsReadyTarget,
            Config::ParamsReadyHookLength,
            PAGE_EXECUTE_READWRITE,
            &oldProtection
        ))
        {
            return false;
        }


        std::memcpy(
            gParamsReadyTarget,
            gParamsReadyOriginalBytes,
            Config::ParamsReadyHookLength
        );


        FlushInstructionCache(
            GetCurrentProcess(),
            gParamsReadyTarget,
            Config::ParamsReadyHookLength
        );


        DWORD ignored = 0;


        VirtualProtect(
            gParamsReadyTarget,
            Config::ParamsReadyHookLength,
            oldProtection,
            &ignored
        );


        return true;
    }


    inline void RequestParamsReadyHookUninstall()
    {
        if (IsParamsReadyHookInstalled())
        {
            gParamsReadyUninstallRequested.store(
                true,
                std::memory_order_release
            );
        }
    }


    inline bool ParamsReadyHookUninstallRequested()
    {
        return
            gParamsReadyUninstallRequested.load(
                std::memory_order_acquire
            );
    }


    inline bool UninstallParamsReadyHookNow()
    {
        if (!IsParamsReadyHookInstalled())
        {
            return true;
        }


        if (!RestoreParamsReadyHookBytes())
        {
            gParamsReadyUnhookFailures.fetch_add(
                1,
                std::memory_order_relaxed
            );

            return false;
        }


        gParamsReadyHookInstalled.store(
            false,
            std::memory_order_release
        );


        gParamsReadyUninstallRequested.store(
            false,
            std::memory_order_release
        );


        gParamsReadyUnhookCompletedAt.store(
            GetTickCount64(),
            std::memory_order_release
        );


        return true;
    }


    inline bool IsParamsReadyHookSafeToUnload()
    {
        if (IsParamsReadyHookInstalled())
        {
            return false;
        }


        if (
            gParamsReadyInFlight.load(
                std::memory_order_acquire
            ) != 0
            )
        {
            return false;
        }


        const ULONGLONG completedAt =
            gParamsReadyUnhookCompletedAt.load(
                std::memory_order_acquire
            );


        if (completedAt == 0)
        {
            return
                gParamsReadyStub ==
                nullptr;
        }


        return
            GetTickCount64() -
                completedAt >=
            Config::ParamsReadyUnloadGraceMs;
    }


    inline bool FinalizeParamsReadyHook()
    {
        if (!IsParamsReadyHookSafeToUnload())
        {
            return false;
        }


        BYTE* stub =
            gParamsReadyStub;


        gParamsReadyStub =
            nullptr;


        gParamsReadyTarget =
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
    // AIM MATH DEBUG
    // ============================================================

    inline void PrintAimMathStatus()
    {
        const bool eligible =
            gLastTargetEligible.load(
                std::memory_order_acquire
            );


        const bool calculated =
            gLastCalculatedDirValid.load(
                std::memory_order_acquire
            );


        std::cout
            << "\n========================================\n"
            << "         SILENT AIM MATH DEBUG\n"
            << "========================================\n"
            << "[Silent Aim]       "
            << (
                IsSilentAimEnabled()
                    ? "ON"
                    : "OFF"
            )
            << '\n'
            << "[Redirects]        "
            << gSilentAimRedirects.load(
                std::memory_order_relaxed
            )
            << '\n'
            << "[Write failures]   "
            << gSilentAimWriteFailures.load(
                std::memory_order_relaxed
            )
            << '\n'
            << "[Skipped]          "
            << gSilentAimSkipped.load(
                std::memory_order_relaxed
            )
            << '\n'
            << "[Eligible]         "
            << (
                eligible
                    ? "YES"
                    : "NO"
            )
            << '\n'
            << "[Calculated]       "
            << (
                calculated
                    ? "YES"
                    : "NO"
            )
            << '\n';


        if (eligible)
        {
            std::cout
                << std::hex
                << std::uppercase
                << std::showbase
                << "[Target Pawn]      "
                << gLastTargetPawn.load()
                << '\n'
                << std::dec
                << std::nouppercase
                << std::noshowbase
                << "[Snapshot age]     "
                << gLastTargetAgeMs.load()
                << " ms\n"
                << std::fixed
                << std::setprecision(3)
                << "[Target Head]      "
                << gLastTargetHeadX.load()
                << ", "
                << gLastTargetHeadY.load()
                << ", "
                << gLastTargetHeadZ.load()
                << '\n'
                << std::defaultfloat;
        }


        std::cout
            << "[Current Dir]      "
            << gLastDirPitch.load()
            << ", "
            << gLastDirYaw.load()
            << ", "
            << gLastDirRoll.load()
            << '\n';


        if (calculated)
        {
            std::cout
                << "[Calculated Dir]   "
                << gLastCalculatedPitch.load()
                << ", "
                << gLastCalculatedYaw.load()
                << ", 0\n";
        }


        std::cout
            << "========================================\n";
    }
}