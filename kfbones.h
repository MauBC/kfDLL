#pragma once

#include <Windows.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <string>

#include "kfcamera.h"
#include "kfmemory.h"
#include "kfoffsets.h"


namespace KFBones
{
#if !defined(_M_IX86)
#error KFBones currently requires Win32/x86.
#endif


    // ============================================================
    // CONFIG
    // ============================================================

    namespace Config
    {
        constexpr size_t GObjHashBucketCount =
            0x1000;


        constexpr size_t MaxHashChainLength =
            512;


        constexpr size_t ProcessEventSlot =
            4;


        // Sanity limit:
        // un bone valido no deberia quedar cientos de unidades
        // alejado del origin del Pawn.
        constexpr float MaxReasonableBoneDelta =
            512.0f;
    }


    // ============================================================
    // UE2 TYPES
    //
    // Reflection confirmo:
    //
    // FName = 4 bytes
    //
    // Coords:
    //     Origin  FVector  12
    //     XAxis   FVector  12
    //     YAxis   FVector  12
    //     ZAxis   FVector  12
    //
    // Total = 48 = 0x30
    // ============================================================

    struct Coords
    {
        KFCamera::Vec3 origin;

        KFCamera::Vec3 xAxis;

        KFCamera::Vec3 yAxis;

        KFCamera::Vec3 zAxis;
    };


    static_assert(
        sizeof(Coords) == 0x30,
        "Unexpected UE2 Coords size."
    );


    // ============================================================
    // GetBoneCoords params
    //
    // Reflection confirmo:
    //
    // +0x00 FName BoneName      4
    // +0x04 Coords ReturnValue 48
    //
    // Total = 0x34
    // ============================================================

    struct GetBoneCoordsParams
    {
        uint32_t boneName = 0;

        Coords returnValue{};
    };


    static_assert(
        sizeof(GetBoneCoordsParams) == 0x34,
        "Unexpected GetBoneCoords parameter size."
    );


    // ============================================================
    // CACHE
    //
    // No guardamos una direccion absoluta entre procesos.
    // La UFunction se localiza desde GObjHash en cada carga DLL.
    // ============================================================

    inline uintptr_t
        gGetBoneCoordsFunction = 0;
    // ============================================================
    // GAME THREAD
    //
    // UE2 ProcessEvent / native functions are not safe to invoke
    // from our independent GDI overlay thread.
    //
    // The game thread must be bound from a callback that UE2 itself
    // executes on its own thread before GetBoneCoords can run.
    // ============================================================

    inline std::atomic<DWORD>
        gGameThreadId{ 0 };


    inline void BindGameThread()
    {
        gGameThreadId.store(
            GetCurrentThreadId()
        );
    }


    inline void ClearGameThread()
    {
        gGameThreadId.store(
            0
        );
    }


    inline bool IsGameThread()
    {
        const DWORD expected =
            gGameThreadId.load();


        return
            expected != 0 &&
            GetCurrentThreadId() ==
                expected;
    }




    // ============================================================
    // FIND UFUNCTION
    // ============================================================

    inline uintptr_t FindFunction(
        const char* wantedName
    )
    {
        if (
            wantedName == nullptr ||
            *wantedName == '\0'
            )
        {
            return 0;
        }


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
            bucket <
                Config::GObjHashBucketCount;
            ++bucket
            )
        {
            uintptr_t object = 0;


            if (!KFMemory::Read(
                hashBase +
                bucket *
                sizeof(uintptr_t),
                object
            ))
            {
                continue;
            }


            size_t chainLength = 0;


            while (
                object != 0 &&
                chainLength <
                    Config::MaxHashChainLength
                )
            {
                const std::string name =
                    KFMemory::GetObjectName(
                        object
                    );


                if (name == wantedName)
                {
                    const std::string className =
                        KFMemory::GetClassName(
                            object
                        );


                    if (className == "Function")
                    {
                        return object;
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


    // ============================================================
    // GET FUNCTION
    // ============================================================

    inline uintptr_t GetGetBoneCoordsFunction()
    {
        if (gGetBoneCoordsFunction != 0)
        {
            return gGetBoneCoordsFunction;
        }


        gGetBoneCoordsFunction =
            FindFunction(
                "GetBoneCoords"
            );


        return gGetBoneCoordsFunction;
    }


    // ============================================================
    // HEAD FNAME
    // ============================================================

    inline bool ReadHeadBoneName(
        uintptr_t pawn,
        uint32_t& boneName
    )
    {
        boneName = 0;


        if (pawn == 0)
        {
            return false;
        }


        if (!KFMemory::Read(
            pawn +
            KFOffsets::KFMonster::HeadBone,
            boneName
        ))
        {
            return false;
        }


        if (boneName == 0)
        {
            return false;
        }


        return true;
    }


    // ============================================================
    // PROCESS EVENT
    //
    // CE runtime tests confirmed:
    //
    // Pawn VTable[4] =
    //     Actor/Pawn ProcessEvent override.
    //
    // Signature:
    //
    // thiscall ProcessEvent(
    //     UObject* this,
    //     UFunction* function,
    //     void* params,
    //     void* result
    // )
    // ============================================================

    using ProcessEventFn =
        void(__thiscall*)(
            void*,
            void*,
            void*,
            void*
        );


    inline ProcessEventFn GetProcessEvent(
        uintptr_t object
    )
    {
        if (object == 0)
        {
            return nullptr;
        }


        uintptr_t vtable = 0;


        if (!KFMemory::Read(
            object,
            vtable
        ))
        {
            return nullptr;
        }


        if (vtable == 0)
        {
            return nullptr;
        }


        uintptr_t functionAddress = 0;


        if (!KFMemory::Read(
            vtable +
            Config::ProcessEventSlot *
            sizeof(uintptr_t),
            functionAddress
        ))
        {
            return nullptr;
        }


        if (functionAddress == 0)
        {
            return nullptr;
        }


        return reinterpret_cast<ProcessEventFn>(
            functionAddress
        );
    }


    // ============================================================
    // GET BONE COORDS
    // ============================================================

    inline bool GetBoneCoords(
        uintptr_t pawn,
        uint32_t boneName,
        Coords& out
    )
    {
        out = {};


        // IMPORTANT:
        //
        // Never execute UE2 ProcessEvent from the independent
        // overlay/helper thread. The engine allocator and script
        // runtime are not thread-safe for this usage.
        if (!IsGameThread())
        {
            return false;
        }


        if (
            pawn == 0 ||
            boneName == 0
            )
        {
            return false;
        }


        const uintptr_t function =
            GetGetBoneCoordsFunction();


        if (function == 0)
        {
            return false;
        }


        const ProcessEventFn processEvent =
            GetProcessEvent(
                pawn
            );


        if (processEvent == nullptr)
        {
            return false;
        }


        GetBoneCoordsParams params;


        params.boneName =
            boneName;


        __try
        {
            processEvent(
                reinterpret_cast<void*>(
                    pawn
                ),
                reinterpret_cast<void*>(
                    function
                ),
                &params,
                nullptr
            );
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }


        const KFCamera::Vec3& origin =
            params.returnValue.origin;


        if (
            !std::isfinite(origin.x) ||
            !std::isfinite(origin.y) ||
            !std::isfinite(origin.z)
            )
        {
            return false;
        }


        out =
            params.returnValue;


        return true;
    }


    // ============================================================
    // HEAD POSITION
    // ============================================================

    inline bool GetHeadPosition(
        uintptr_t pawn,
        KFCamera::Vec3& headWorld
    )
    {
        headWorld = {};


        uint32_t headBone = 0;


        if (!ReadHeadBoneName(
            pawn,
            headBone
        ))
        {
            return false;
        }


        Coords coords;


        if (!GetBoneCoords(
            pawn,
            headBone,
            coords
        ))
        {
            return false;
        }


        float pawnX = 0.0f;
        float pawnY = 0.0f;
        float pawnZ = 0.0f;


        if (
            !KFMemory::Read(
                pawn +
                KFOffsets::Pawn::X,
                pawnX
            ) ||
            !KFMemory::Read(
                pawn +
                KFOffsets::Pawn::Y,
                pawnY
            ) ||
            !KFMemory::Read(
                pawn +
                KFOffsets::Pawn::Z,
                pawnZ
            )
            )
        {
            return false;
        }


        const float dx =
            coords.origin.x -
            pawnX;


        const float dy =
            coords.origin.y -
            pawnY;


        const float dz =
            coords.origin.z -
            pawnZ;


        const float maxDelta =
            Config::MaxReasonableBoneDelta;


        if (
            dx * dx +
            dy * dy +
            dz * dz >
            maxDelta *
            maxDelta
            )
        {
            return false;
        }


        headWorld =
            coords.origin;


        return true;
    }
}