#pragma once

#include <cstdint>

namespace KFOffsets
{
    namespace Global
    {
        constexpr uintptr_t Names =
            0x00166674;

        constexpr uintptr_t GObjHash =
            0x00166698;

        constexpr uintptr_t PlayerControllerVTable =
            0x0033DB40;
    }


    namespace UObject
    {
        constexpr uintptr_t HashNext =
            0x08;

        constexpr uintptr_t NameIndex =
            0x24;

        constexpr uintptr_t Class =
            0x28;
    }


    namespace UClass
    {
        constexpr uintptr_t SuperClass =
            0x2C;
    }


    namespace FNameEntry
    {
        constexpr uintptr_t Text =
            0x0C;
    }


    // ============================================================
    // UE2 REFLECTION
    //
    // Confirmados mediante GObjHash/UFunction/UProperty metadata.
    // ============================================================

    namespace UField
    {
        constexpr uintptr_t Next =
            0x30;
    }


    namespace UStruct
    {
        constexpr uintptr_t Children =
            0x40;

        constexpr uintptr_t PropertySize =
            0x44;
    }


    namespace UProperty
    {
        constexpr uintptr_t Offset =
            0x4C;
    }


    // ============================================================
    // KFMonster HEAD / HEADSHOT
    //
    // Offsets obtenidos directamente de UProperty reflection y
    // validados en Clot + Crawler vivos.
    // ============================================================

    namespace KFMonster
    {
        constexpr uintptr_t HeadRadius =
            0x494;

        constexpr uintptr_t HeadHeight =
            0x498;

        constexpr uintptr_t HeadScale =
            0x49C;

        constexpr uintptr_t HeadBone =
            0x768;

        constexpr uintptr_t NeckBone =
            0xF0C;

        constexpr uintptr_t OnlineHeadshotOffset =
            0xFB8;

        constexpr uintptr_t OnlineHeadshotScale =
            0xFC4;

        constexpr uintptr_t HeadHealth =
            0xFC8;
    }

    namespace Actor
    {
        // AActor::GetViewRotation
        // Confirmados experimentalmente en nuestro build.
        constexpr uintptr_t Pitch =
            0x158;

        constexpr uintptr_t Yaw =
            0x15C;

        constexpr uintptr_t Roll =
            0x160;
    }

    namespace Pawn
    {
        constexpr uintptr_t Level =
            0x09C;

        constexpr uintptr_t X =
            0x14C;

        constexpr uintptr_t Y =
            0x150;

        constexpr uintptr_t Z =
            0x154;

        constexpr uintptr_t Controller =
            0x360;

        constexpr uintptr_t Weapon =
            0x43C;

        // Alturas visuales confirmadas experimentalmente.
        constexpr uintptr_t BaseEyeHeight =
            0x448;

        constexpr uintptr_t EyeHeight =
            0x44C;

        constexpr uintptr_t Health =
            0x480;

        constexpr uintptr_t PlayerReplicationInfo =
            0x51C;

        constexpr uintptr_t Armor =
            0x774;
    }


    namespace Controller
    {
        constexpr uintptr_t Pawn =
            0x360;

        // FOV efectivo de la vista.
        // Validado con comando FOV, ADS y Crossbow zoom.
        constexpr uintptr_t FOV =
            0x36C;

        constexpr uintptr_t PlayerReplicationInfo =
            0x490;
    }


    namespace PlayerReplicationInfo
    {
        constexpr uintptr_t Cash =
            0x3B4;

        constexpr uintptr_t TeamRoster =
            0x410;
    }


    namespace Weapon
    {
        constexpr uintptr_t MagazineCapacity =
            0x594;

        constexpr uintptr_t LoadedAmmo =
            0x5A0;
    }


    namespace Level
    {
        constexpr uintptr_t EntityList =
            0x30;

        constexpr uintptr_t EntityCount =
            0x34;

        constexpr uintptr_t EntityCapacity =
            0x38;
    }
}