#pragma once

#include <Windows.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

#include "kfmemory.h"
#include "kfoffsets.h"


namespace KFFireRegistry
{
#if !defined(_M_IX86)
#error KFFireRegistry requires Win32/x86.
#endif


    namespace Config
    {
        constexpr size_t GObjHashBucketCount =
            0x1000;

        constexpr size_t MaxHashChainLength =
            512;

        constexpr size_t MaxRegisteredFunctions =
            128;

        constexpr size_t MaxClassDepth =
            32;

        constexpr size_t MaxFunctionFields =
            256;


        constexpr uintptr_t UObjectOuter =
            0x1C;

        constexpr uintptr_t PropertyArrayDim =
            0x38;

        constexpr uintptr_t PropertyElementSize =
            0x3C;


        constexpr uint32_t ExpectedStartOffset =
            0x00;

        constexpr uint32_t ExpectedDirOffset =
            0x0C;

        constexpr uint32_t ExpectedVectorSize =
            0x0C;
    }


    struct Descriptor
    {
        uintptr_t function = 0;
        uintptr_t ownerClass = 0;

        uint32_t propertySize = 0;

        uint32_t startOffset = 0;
        uint32_t dirOffset = 0;
    };


    inline std::array<
        Descriptor,
        Config::MaxRegisteredFunctions
    > gDoTraceFunctions{};


    inline std::atomic_size_t
        gDoTraceCount{ 0 };


    inline std::atomic_bool
        gInitialized{ false };


    // ============================================================
    // CLASS INHERITANCE
    // ============================================================

    inline bool ClassInheritsNamed(
        uintptr_t classObject,
        const char* wantedName
    )
    {
        if (
            classObject == 0 ||
            wantedName == nullptr
            )
        {
            return false;
        }


        uintptr_t current =
            classObject;


        for (
            size_t depth = 0;
            depth < Config::MaxClassDepth;
            ++depth
            )
        {
            if (current == 0)
            {
                return false;
            }


            if (
                KFMemory::GetObjectName(
                    current
                ) == wantedName
                )
            {
                return true;
            }


            uintptr_t parent = 0;


            if (!KFMemory::Read(
                current +
                    KFOffsets::UClass::SuperClass,
                parent
            ))
            {
                return false;
            }


            if (
                parent == 0 ||
                parent == current
                )
            {
                return false;
            }


            current =
                parent;
        }


        return false;
    }


    // ============================================================
    // VALIDATE START + DIR
    // ============================================================

    inline bool ValidateDoTraceLayout(
        uintptr_t function,
        Descriptor& descriptor
    )
    {
        descriptor =
            {};


        if (function == 0)
        {
            return false;
        }


        uintptr_t ownerClass = 0;


        if (!KFMemory::Read(
            function +
                Config::UObjectOuter,
            ownerClass
        ))
        {
            return false;
        }


        if (
            ownerClass == 0 ||
            !ClassInheritsNamed(
                ownerClass,
                "WeaponFire"
            )
            )
        {
            return false;
        }


        uint32_t propertySize = 0;


        if (!KFMemory::Read(
            function +
                KFOffsets::UStruct::PropertySize,
            propertySize
        ))
        {
            return false;
        }


        // Como mínimo:
        //
        // FVector Start  0x00 -> 0x0B
        // FRotator Dir   0x0C -> 0x17
        if (propertySize < 0x18)
        {
            return false;
        }


        uintptr_t child = 0;


        if (!KFMemory::Read(
            function +
                KFOffsets::UStruct::Children,
            child
        ))
        {
            return false;
        }


        bool startValid =
            false;

        bool dirValid =
            false;


        for (
            size_t count = 0;
            child != 0 &&
            count < Config::MaxFunctionFields;
            ++count
            )
        {
            const std::string name =
                KFMemory::GetObjectName(
                    child
                );


            const std::string type =
                KFMemory::GetClassName(
                    child
                );


            if (
                name == "Start" ||
                name == "Dir"
                )
            {
                uint32_t offset = 0;
                uint32_t elementSize = 0;
                uint32_t arrayDim = 0;


                const bool offsetOk =
                    KFMemory::Read(
                        child +
                            KFOffsets::UProperty::Offset,
                        offset
                    );


                const bool sizeOk =
                    KFMemory::Read(
                        child +
                            Config::PropertyElementSize,
                        elementSize
                    );


                const bool arrayOk =
                    KFMemory::Read(
                        child +
                            Config::PropertyArrayDim,
                        arrayDim
                    );


                const bool isStruct =
                    type ==
                    "StructProperty";


                if (
                    offsetOk &&
                    sizeOk &&
                    arrayOk &&
                    isStruct &&
                    elementSize ==
                        Config::ExpectedVectorSize &&
                    arrayDim ==
                        1
                    )
                {
                    if (
                        name == "Start" &&
                        offset ==
                            Config::ExpectedStartOffset
                        )
                    {
                        startValid =
                            true;
                    }


                    if (
                        name == "Dir" &&
                        offset ==
                            Config::ExpectedDirOffset
                        )
                    {
                        dirValid =
                            true;
                    }
                }
            }


            uintptr_t next = 0;


            if (!KFMemory::Read(
                child +
                    KFOffsets::UField::Next,
                next
            ))
            {
                break;
            }


            if (next == child)
            {
                break;
            }


            child =
                next;
        }


        if (
            !startValid ||
            !dirValid
            )
        {
            return false;
        }


        descriptor.function =
            function;

        descriptor.ownerClass =
            ownerClass;

        descriptor.propertySize =
            propertySize;

        descriptor.startOffset =
            Config::ExpectedStartOffset;

        descriptor.dirOffset =
            Config::ExpectedDirOffset;


        return true;
    }


    // ============================================================
    // INITIALIZE
    // ============================================================

    inline bool Initialize()
    {
        if (
            gInitialized.load(
                std::memory_order_acquire
            )
            )
        {
            return
                gDoTraceCount.load(
                    std::memory_order_acquire
                ) != 0;
        }


        const uintptr_t coreBase =
            KFMemory::CoreBase();


        if (coreBase == 0)
        {
            return false;
        }


        const uintptr_t hashBase =
            coreBase +
            KFOffsets::Global::GObjHash;


        size_t registered =
            0;


        for (
            size_t bucket = 0;
            bucket < Config::GObjHashBucketCount;
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


            size_t chainLength =
                0;


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


                if (
                    name == "DoTrace" &&
                    KFMemory::GetClassName(
                        object
                    ) == "Function"
                    )
                {
                    Descriptor descriptor;


                    if (
                        ValidateDoTraceLayout(
                            object,
                            descriptor
                        )
                        )
                    {
                        bool duplicate =
                            false;


                        for (
                            size_t i = 0;
                            i < registered;
                            ++i
                            )
                        {
                            if (
                                gDoTraceFunctions[i].function ==
                                object
                                )
                            {
                                duplicate =
                                    true;

                                break;
                            }
                        }


                        if (
                            !duplicate &&
                            registered <
                                Config::MaxRegisteredFunctions
                            )
                        {
                            gDoTraceFunctions[
                                registered
                            ] =
                                descriptor;


                            ++registered;
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


        gDoTraceCount.store(
            registered,
            std::memory_order_release
        );


        gInitialized.store(
            true,
            std::memory_order_release
        );


        return
            registered != 0;
    }


    // ============================================================
    // LOOKUP
    // ============================================================

    inline bool TryGetDoTrace(
        uintptr_t function,
        Descriptor& descriptor
    )
    {
        descriptor =
            {};


        if (function == 0)
        {
            return false;
        }


        const size_t count =
            gDoTraceCount.load(
                std::memory_order_acquire
            );


        for (
            size_t i = 0;
            i < count;
            ++i
            )
        {
            if (
                gDoTraceFunctions[i].function ==
                function
                )
            {
                descriptor =
                    gDoTraceFunctions[i];

                return true;
            }
        }


        return false;
    }


    inline bool IsDoTraceFunction(
        uintptr_t function
    )
    {
        Descriptor descriptor;


        return
            TryGetDoTrace(
                function,
                descriptor
            );
    }


    // ============================================================
    // DEBUG
    // ============================================================

    inline void PrintStatus()
    {
        const size_t count =
            gDoTraceCount.load(
                std::memory_order_acquire
            );


        std::cout
            << "\n========================================\n"
            << "          FIRE FUNCTION REGISTRY\n"
            << "========================================\n"
            << "[Initialized]      "
            << (
                gInitialized.load()
                    ? "YES"
                    : "NO"
            )
            << '\n'
            << "[DoTrace count]    "
            << count
            << '\n';


        for (
            size_t i = 0;
            i < count;
            ++i
            )
        {
            const Descriptor& item =
                gDoTraceFunctions[i];


            std::cout
                << "  ["
                << i
                << "] "
                << KFMemory::GetObjectName(
                    item.ownerClass
                )
                << ".DoTrace"
                << " | size=0x"
                << std::hex
                << std::uppercase
                << item.propertySize
                << " | fn="
                << std::showbase
                << item.function
                << std::dec
                << std::nouppercase
                << std::noshowbase
                << '\n';
        }


        std::cout
            << "========================================\n\n";
    }
}