#pragma once

#include <Windows.h>

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

#include "kfmemory.h"
#include "kfplayer.h"
#include "kfoffsets.h"


namespace KFWeaponProfile
{
#if !defined(_M_IX86)
#error KFWeaponProfile requires Win32/x86.
#endif


    namespace Config
    {
        // Confirmado por reflection previamente.
        constexpr uintptr_t WeaponFireMode =
            0x3C0;

        constexpr size_t FireModeCount =
            2;


        // UBoolProperty::BitMask confirmado previamente.
        constexpr uintptr_t BoolPropertyBitMask =
            0x70;


        constexpr size_t MaxClassDepth =
            32;

        constexpr size_t MaxFieldsPerClass =
            512;

        // UProperty reflection confirmada previamente.
        constexpr uintptr_t PropertyArrayDim =
            0x38;

        constexpr uintptr_t PropertyElementSize =
            0x3C;

    }


    struct FieldInfo
    {
        uintptr_t field = 0;

        uintptr_t ownerClass = 0;

        std::string typeName;

        bool found = false;
    };


    struct PropertyInfo
    {
        FieldInfo field;

        uint32_t offset = 0;

        uint32_t boolMask = 0;

        bool valid = false;
    };


    // ============================================================
    // CLASS HIERARCHY
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


            const std::string name =
                KFMemory::GetObjectName(
                    current
                );


            if (name == wantedName)
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
    // FIND FIELD THROUGH CLASS HIERARCHY
    // ============================================================

    inline FieldInfo FindField(
        uintptr_t classObject,
        const char* wantedName
    )
    {
        FieldInfo result;


        if (
            classObject == 0 ||
            wantedName == nullptr
            )
        {
            return result;
        }


        uintptr_t currentClass =
            classObject;


        for (
            size_t depth = 0;
            depth < Config::MaxClassDepth;
            ++depth
            )
        {
            if (currentClass == 0)
            {
                break;
            }


            uintptr_t field = 0;


            if (KFMemory::Read(
                currentClass +
                    KFOffsets::UStruct::Children,
                field
            ))
            {
                size_t count = 0;


                while (
                    field != 0 &&
                    count <
                        Config::MaxFieldsPerClass
                    )
                {
                    const std::string name =
                        KFMemory::GetObjectName(
                            field
                        );


                    if (name == wantedName)
                    {
                        result.field =
                            field;

                        result.ownerClass =
                            currentClass;

                        result.typeName =
                            KFMemory::GetClassName(
                                field
                            );

                        result.found =
                            true;


                        return result;
                    }


                    uintptr_t next = 0;


                    if (!KFMemory::Read(
                        field +
                            KFOffsets::UField::Next,
                        next
                    ))
                    {
                        break;
                    }


                    if (next == field)
                    {
                        break;
                    }


                    field =
                        next;


                    ++count;
                }
            }


            uintptr_t parentClass = 0;


            if (!KFMemory::Read(
                currentClass +
                    KFOffsets::UClass::SuperClass,
                parentClass
            ))
            {
                break;
            }


            if (
                parentClass == 0 ||
                parentClass == currentClass
                )
            {
                break;
            }


            currentClass =
                parentClass;
        }


        return result;
    }


    // ============================================================
    // FIND PROPERTY
    // ============================================================

    inline PropertyInfo FindProperty(
        uintptr_t classObject,
        const char* wantedName
    )
    {
        PropertyInfo result;


        result.field =
            FindField(
                classObject,
                wantedName
            );


        if (!result.field.found)
        {
            return result;
        }


        if (
            result.field.typeName.find(
                "Property"
            ) ==
            std::string::npos
            )
        {
            return result;
        }


        if (!KFMemory::Read(
            result.field.field +
                KFOffsets::UProperty::Offset,
            result.offset
        ))
        {
            return result;
        }


        if (
            result.field.typeName ==
            "BoolProperty"
            )
        {
            KFMemory::Read(
                result.field.field +
                    Config::BoolPropertyBitMask,
                result.boolMask
            );
        }


        result.valid =
            true;


        return result;
    }


    // ============================================================
    // PROPERTY READERS
    // ============================================================

    inline bool ReadBoolProperty(
        uintptr_t object,
        uintptr_t classObject,
        const char* name,
        bool& value
    )
    {
        value =
            false;


        const PropertyInfo property =
            FindProperty(
                classObject,
                name
            );


        if (
            !property.valid ||
            property.field.typeName !=
                "BoolProperty" ||
            property.boolMask == 0
            )
        {
            return false;
        }


        uint32_t storage = 0;


        if (!KFMemory::Read(
            object +
                property.offset,
            storage
        ))
        {
            return false;
        }


        value =
            (
                storage &
                property.boolMask
            ) != 0;


        return true;
    }


    inline bool ReadObjectProperty(
        uintptr_t object,
        uintptr_t classObject,
        const char* name,
        uintptr_t& value
    )
    {
        value =
            0;


        const PropertyInfo property =
            FindProperty(
                classObject,
                name
            );


        if (!property.valid)
        {
            return false;
        }


        return
            KFMemory::Read(
                object +
                    property.offset,
                value
            );
    }


    // ============================================================
    // PRINT CLASS CHAIN
    // ============================================================

    inline void PrintClassChain(
        uintptr_t classObject
    )
    {
        std::cout
            << "[Hierarchy]        ";


        uintptr_t current =
            classObject;


        for (
            size_t depth = 0;
            depth < 12;
            ++depth
            )
        {
            if (current == 0)
            {
                break;
            }


            const std::string name =
                KFMemory::GetObjectName(
                    current
                );


            if (depth != 0)
            {
                std::cout
                    << " -> ";
            }


            std::cout
                << name;


            uintptr_t parent = 0;


            if (!KFMemory::Read(
                current +
                    KFOffsets::UClass::SuperClass,
                parent
            ))
            {
                break;
            }


            if (
                parent == 0 ||
                parent == current
                )
            {
                break;
            }


            current =
                parent;
        }


        std::cout
            << '\n';
    }


    // ============================================================
    // PRINT FUNCTION
    // ============================================================

    inline void PrintFunction(
        uintptr_t classObject,
        const char* name
    )
    {
        const FieldInfo function =
            FindField(
                classObject,
                name
            );


        std::cout
            << "["
            << std::left
            << std::setw(18)
            << name
            << "] ";


        if (
            !function.found ||
            function.typeName !=
                "Function"
            )
        {
            std::cout
                << "NOT FOUND\n";

            return;
        }


        std::cout
            << std::hex
            << std::uppercase
            << std::showbase
            << function.field
            << std::dec
            << std::nouppercase
            << std::noshowbase
            << " | owner="
            << KFMemory::GetObjectName(
                function.ownerClass
            )
            << '\n';
    }


    // ============================================================
    // PRINT UFUNCTION PARAM LAYOUT
    // ============================================================

    inline void PrintFunctionLayout(
        uintptr_t classObject,
        const char* name
    )
    {
        const FieldInfo function =
            FindField(
                classObject,
                name
            );


        if (
            !function.found ||
            function.typeName != "Function"
            )
        {
            return;
        }


        uint32_t propertySize = 0;


        KFMemory::Read(
            function.field +
                KFOffsets::UStruct::PropertySize,
            propertySize
        );


        std::cout
            << "\n    [" << name << " layout]"
            << " PropertySize=0x"
            << std::hex
            << std::uppercase
            << propertySize
            << std::dec
            << std::nouppercase
            << '\n';


        uintptr_t child = 0;


        if (!KFMemory::Read(
            function.field +
                KFOffsets::UStruct::Children,
            child
        ))
        {
            return;
        }


        size_t count = 0;


        while (
            child != 0 &&
            count < Config::MaxFieldsPerClass
            )
        {
            const std::string childName =
                KFMemory::GetObjectName(
                    child
                );


            const std::string childType =
                KFMemory::GetClassName(
                    child
                );


            if (
                childType.find("Property") !=
                std::string::npos
                )
            {
                uint32_t offset = 0;
                uint32_t arrayDim = 0;
                uint32_t elementSize = 0;


                KFMemory::Read(
                    child +
                        KFOffsets::UProperty::Offset,
                    offset
                );


                KFMemory::Read(
                    child +
                        Config::PropertyArrayDim,
                    arrayDim
                );


                KFMemory::Read(
                    child +
                        Config::PropertyElementSize,
                    elementSize
                );


                std::cout
                    << "      "
                    << std::left
                    << std::setw(20)
                    << childName
                    << " "
                    << std::setw(18)
                    << childType
                    << " offset=0x"
                    << std::hex
                    << std::uppercase
                    << offset
                    << " size=0x"
                    << elementSize
                    << std::dec
                    << std::nouppercase
                    << " array="
                    << arrayDim
                    << '\n';
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


            ++count;
        }
    }

    // ============================================================
    // PRINT PROPERTY
    // ============================================================

    inline void PrintProperty(
        uintptr_t object,
        uintptr_t classObject,
        const char* name
    )
    {
        const PropertyInfo property =
            FindProperty(
                classObject,
                name
            );


        std::cout
            << "["
            << std::left
            << std::setw(18)
            << name
            << "] ";


        if (!property.valid)
        {
            std::cout
                << "NOT FOUND\n";

            return;
        }


        std::cout
            << property.field.typeName
            << " @ +"
            << std::hex
            << std::uppercase
            << property.offset
            << std::dec
            << std::nouppercase;


        if (
            property.field.typeName ==
            "FloatProperty"
            )
        {
            float value = 0.0f;


            if (KFMemory::Read(
                object +
                    property.offset,
                value
            ))
            {
                std::cout
                    << " = "
                    << value;
            }
        }
        else if (
            property.field.typeName ==
            "IntProperty"
            )
        {
            int32_t value = 0;


            if (KFMemory::Read(
                object +
                    property.offset,
                value
            ))
            {
                std::cout
                    << " = "
                    << value;
            }
        }
        else if (
            property.field.typeName ==
                "ByteProperty"
            )
        {
            uint8_t value = 0;


            if (KFMemory::Read(
                object +
                    property.offset,
                value
            ))
            {
                std::cout
                    << " = "
                    << static_cast<unsigned int>(
                        value
                    );
            }
        }
        else if (
            property.field.typeName ==
                "BoolProperty"
            )
        {
            uint32_t storage = 0;


            if (KFMemory::Read(
                object +
                    property.offset,
                storage
            ))
            {
                const bool value =
                    (
                        storage &
                        property.boolMask
                    ) != 0;


                std::cout
                    << " mask=0x"
                    << std::hex
                    << std::uppercase
                    << property.boolMask
                    << std::dec
                    << std::nouppercase
                    << " = "
                    << (
                        value
                            ? "TRUE"
                            : "FALSE"
                    );
            }
        }
        else if (
            property.field.typeName ==
                "ObjectProperty" ||
            property.field.typeName ==
                "ClassProperty"
            )
        {
            uintptr_t value = 0;


            if (KFMemory::Read(
                object +
                    property.offset,
                value
            ))
            {
                std::cout
                    << " = "
                    << std::hex
                    << std::uppercase
                    << std::showbase
                    << value
                    << std::dec
                    << std::nouppercase
                    << std::noshowbase;


                if (value != 0)
                {
                    std::cout
                        << " ("
                        << KFMemory::GetObjectName(
                            value
                        )
                        << ")";
                }
            }
        }


        std::cout
            << '\n';
    }


    // ============================================================
    // CLASSIFY
    // ============================================================

    inline const char* ClassifyFireMode(
        uintptr_t fireMode,
        uintptr_t fireClass
    )
    {
        bool instantHit = false;


        const bool instantKnown =
            ReadBoolProperty(
                fireMode,
                fireClass,
                "bInstantHit",
                instantHit
            );


        uintptr_t projectileClass = 0;


        ReadObjectProperty(
            fireMode,
            fireClass,
            "ProjectileClass",
            projectileClass
        );


        if (
            projectileClass != 0 ||
            ClassInheritsNamed(
                fireClass,
                "ProjectileFire"
            )
            )
        {
            return
                "PROJECTILE";
        }


        if (
            instantKnown &&
            instantHit
            )
        {
            return
                "HITSCAN";
        }


        if (
            ClassInheritsNamed(
                fireClass,
                "InstantFire"
            ) ||
            ClassInheritsNamed(
                fireClass,
                "KFFire"
            )
            )
        {
            return
                "HITSCAN CANDIDATE";
        }


        return
            "SPECIAL / UNKNOWN";
    }


    // ============================================================
    // PRINT FIRE MODE
    // ============================================================

    inline void PrintFireMode(
        uintptr_t fireMode,
        size_t index
    )
    {
        std::cout
            << "\n----------------------------------------\n"
            << " FIRE MODE ["
            << index
            << "]\n"
            << "----------------------------------------\n";


        if (fireMode == 0)
        {
            std::cout
                << "[Object]           NULL\n";

            return;
        }


        const uintptr_t fireClass =
            KFMemory::GetObjectClass(
                fireMode
            );


        std::cout
            << std::hex
            << std::uppercase
            << std::showbase
            << "[Object]           "
            << fireMode
            << '\n'
            << "[Class object]     "
            << fireClass
            << '\n'
            << std::dec
            << std::nouppercase
            << std::noshowbase
            << "[Class]            "
            << KFMemory::GetClassName(
                fireMode
            )
            << '\n';


        PrintClassChain(
            fireClass
        );


        std::cout
            << "[Classification]   "
            << ClassifyFireMode(
                fireMode,
                fireClass
            )
            << "\n\n";


        PrintFunction(
            fireClass,
            "DoTrace"
        );

        PrintFunction(
            fireClass,
            "SpawnProjectile"
        );


        PrintFunctionLayout(
            fireClass,
            "DoTrace"
        );


        PrintFunctionLayout(
            fireClass,
            "SpawnProjectile"
        );


        std::cout
            << '\n';


        PrintProperty(
            fireMode,
            fireClass,
            "bInstantHit"
        );

        PrintProperty(
            fireMode,
            fireClass,
            "ProjectileClass"
        );

        PrintProperty(
            fireMode,
            fireClass,
            "FireRate"
        );

        PrintProperty(
            fireMode,
            fireClass,
            "AmmoPerFire"
        );

        PrintProperty(
            fireMode,
            fireClass,
            "aimerror"
        );

        PrintProperty(
            fireMode,
            fireClass,
            "Spread"
        );

        PrintProperty(
            fireMode,
            fireClass,
            "SpreadStyle"
        );


        std::cout
            << "\n--- RECOIL / SHAKE ---\n";


        PrintProperty(
            fireMode,
            fireClass,
            "ShakeRotMag"
        );

        PrintProperty(
            fireMode,
            fireClass,
            "ShakeRotRate"
        );

        PrintProperty(
            fireMode,
            fireClass,
            "ShakeRotTime"
        );

        PrintProperty(
            fireMode,
            fireClass,
            "ShakeOffsetMag"
        );

        PrintProperty(
            fireMode,
            fireClass,
            "ShakeOffsetRate"
        );

        PrintProperty(
            fireMode,
            fireClass,
            "ShakeOffsetTime"
        );
    }


    // ============================================================
    // PRINT CURRENT WEAPON
    // ============================================================

    inline void PrintCurrentWeaponProfile()
    {
        KFPlayer::Context context;


        if (!KFPlayer::ResolveContext(
            context
        ))
        {
            std::cout
                << "\n[WEAPON PROFILE] Player context unavailable.\n";

            return;
        }


        if (context.weapon == 0)
        {
            std::cout
                << "\n[WEAPON PROFILE] No active weapon.\n";

            return;
        }


        std::cout
            << "\n========================================\n"
            << "             WEAPON PROFILE\n"
            << "========================================\n"
            << std::hex
            << std::uppercase
            << std::showbase
            << "[Weapon]           "
            << context.weapon
            << '\n'
            << std::dec
            << std::nouppercase
            << std::noshowbase
            << "[Weapon Class]     "
            << KFMemory::GetClassName(
                context.weapon
            )
            << '\n';


        for (
            size_t index = 0;
            index < Config::FireModeCount;
            ++index
            )
        {
            uintptr_t fireMode = 0;


            KFMemory::Read(
                context.weapon +
                    Config::WeaponFireMode +
                    index *
                        sizeof(uintptr_t),
                fireMode
            );


            PrintFireMode(
                fireMode,
                index
            );
        }


        std::cout
            << "========================================\n\n";
    }
}