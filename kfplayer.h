#pragma once

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

#include "kfoffsets.h"
#include "kfmemory.h"


namespace KFPlayer
{
    // ============================================================
    // CONFIGURACION
    // ============================================================

    constexpr size_t GObjHashBucketCount =
        0x1000;


    // ============================================================
    // CONTEXTO DEL JUGADOR
    // ============================================================

    struct Context
    {
        uintptr_t controller = 0;
        uintptr_t pawn = 0;

        // KFPlayerReplicationInfo*
        uintptr_t playerReplicationInfo = 0;

        uintptr_t weapon = 0;
    };


    // ============================================================
    // SNAPSHOT DEL JUGADOR
    // ============================================================

    struct Snapshot
    {
        Context context;

        int health = 0;

        float armor = 0.0f;
        float cash = 0.0f;

        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        int ammo = 0;
        int magazineCapacity = 0;

        std::string weaponName;

        bool hasHealth = false;
        bool hasArmor = false;
        bool hasCash = false;
        bool hasPosition = false;
        bool hasAmmo = false;
    };


    // ============================================================
    // VALIDAR APlayerController
    //
    // Comprobamos:
    //
    // 1. VTable correcta.
    // 2. Controller +0x360 -> Pawn.
    // 3. Pawn +0x360 -> mismo Controller.
    // 4. PRI del Controller y Pawn coinciden si ambos existen.
    // ============================================================

    inline bool IsLocalControllerCandidate(
        uintptr_t object,
        uintptr_t expectedVTable
    )
    {
        if (object == 0)
        {
            return false;
        }


        // --------------------------------------------------------
        // VTable
        // --------------------------------------------------------

        uintptr_t objectVTable = 0;

        if (!KFMemory::Read(
            object,
            objectVTable
        ))
        {
            return false;
        }


        if (objectVTable != expectedVTable)
        {
            return false;
        }


        // --------------------------------------------------------
        // Controller -> Pawn
        // --------------------------------------------------------

        uintptr_t pawn = 0;

        if (!KFMemory::Read(
            object +
            KFOffsets::Controller::Pawn,
            pawn
        ))
        {
            return false;
        }


        if (pawn == 0)
        {
            return false;
        }


        // --------------------------------------------------------
        // Pawn -> Controller
        // --------------------------------------------------------

        uintptr_t pawnController = 0;

        if (!KFMemory::Read(
            pawn +
            KFOffsets::Pawn::Controller,
            pawnController
        ))
        {
            return false;
        }


        if (pawnController != object)
        {
            return false;
        }


        // --------------------------------------------------------
        // Verificación adicional mediante PRI
        // --------------------------------------------------------

        uintptr_t priFromController = 0;
        uintptr_t priFromPawn = 0;


        KFMemory::Read(
            object +
            KFOffsets::Controller::PlayerReplicationInfo,
            priFromController
        );


        KFMemory::Read(
            pawn +
            KFOffsets::Pawn::PlayerReplicationInfo,
            priFromPawn
        );


        if (
            priFromController != 0 &&
            priFromPawn != 0 &&
            priFromController != priFromPawn
            )
        {
            return false;
        }


        return true;
    }


    // ============================================================
    // ENCONTRAR CONTROLLER LOCAL
    //
    // Core.dll + GObjHash
    //        ↓
    // recorrer 4096 buckets
    //        ↓
    // buscar APlayerController::vftable
    // ============================================================

    inline uintptr_t FindLocalController()
    {
        const uintptr_t coreBase =
            KFMemory::CoreBase();

        const uintptr_t engineBase =
            KFMemory::EngineBase();


        if (
            coreBase == 0 ||
            engineBase == 0
            )
        {
            return 0;
        }


        const uintptr_t hashBase =
            coreBase +
            KFOffsets::Global::GObjHash;


        const uintptr_t expectedVTable =
            engineBase +
            KFOffsets::Global::PlayerControllerVTable;


        for (
            size_t bucket = 0;
            bucket < GObjHashBucketCount;
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


            // Evita quedarnos atrapados en una cadena corrupta.
            size_t chainLength = 0;


            while (
                object != 0 &&
                chainLength < 512
                )
            {
                ++chainLength;


                if (
                    IsLocalControllerCandidate(
                        object,
                        expectedVTable
                    )
                    )
                {
                    return object;
                }


                uintptr_t nextObject = 0;


                if (!KFMemory::Read(
                    object +
                    KFOffsets::UObject::HashNext,
                    nextObject
                ))
                {
                    break;
                }


                // Protección contra ciclo.
                if (nextObject == object)
                {
                    break;
                }


                object =
                    nextObject;
            }
        }


        return 0;
    }


    // ============================================================
    // RESOLVER CONTEXTO
    // ============================================================

    inline bool ResolveContext(
        Context& context
    )
    {
        context = {};


        // --------------------------------------------------------
        // Controller
        // --------------------------------------------------------

        context.controller =
            FindLocalController();


        if (context.controller == 0)
        {
            return false;
        }


        // --------------------------------------------------------
        // Pawn
        // --------------------------------------------------------

        if (!KFMemory::Read(
            context.controller +
            KFOffsets::Controller::Pawn,
            context.pawn
        ))
        {
            return false;
        }


        if (context.pawn == 0)
        {
            return false;
        }


        // --------------------------------------------------------
        // PlayerReplicationInfo
        // --------------------------------------------------------

        uintptr_t priFromController = 0;
        uintptr_t priFromPawn = 0;


        KFMemory::Read(
            context.controller +
            KFOffsets::Controller::PlayerReplicationInfo,
            priFromController
        );


        KFMemory::Read(
            context.pawn +
            KFOffsets::Pawn::PlayerReplicationInfo,
            priFromPawn
        );


        if (priFromController != 0)
        {
            context.playerReplicationInfo =
                priFromController;
        }
        else
        {
            context.playerReplicationInfo =
                priFromPawn;
        }


        // --------------------------------------------------------
        // Weapon
        // --------------------------------------------------------

        KFMemory::Read(
            context.pawn +
            KFOffsets::Pawn::Weapon,
            context.weapon
        );


        return true;
    }


    // ============================================================
    // LEER SNAPSHOT
    // ============================================================

    inline bool ReadSnapshot(
        Snapshot& snapshot
    )
    {
        snapshot = {};


        if (!ResolveContext(
            snapshot.context
        ))
        {
            return false;
        }


        const uintptr_t pawn =
            snapshot.context.pawn;


        // --------------------------------------------------------
        // Health
        // --------------------------------------------------------

        snapshot.hasHealth =
            KFMemory::Read(
                pawn +
                KFOffsets::Pawn::Health,
                snapshot.health
            );


        // --------------------------------------------------------
        // Armor
        // --------------------------------------------------------

        snapshot.hasArmor =
            KFMemory::Read(
                pawn +
                KFOffsets::Pawn::Armor,
                snapshot.armor
            );


        // --------------------------------------------------------
        // Posición
        // --------------------------------------------------------

        const bool hasX =
            KFMemory::Read(
                pawn +
                KFOffsets::Pawn::X,
                snapshot.x
            );


        const bool hasY =
            KFMemory::Read(
                pawn +
                KFOffsets::Pawn::Y,
                snapshot.y
            );


        const bool hasZ =
            KFMemory::Read(
                pawn +
                KFOffsets::Pawn::Z,
                snapshot.z
            );


        snapshot.hasPosition =
            hasX &&
            hasY &&
            hasZ;


        // --------------------------------------------------------
        // Cash
        // --------------------------------------------------------

        if (
            snapshot.context.playerReplicationInfo != 0
            )
        {
            snapshot.hasCash =
                KFMemory::Read(
                    snapshot.context.playerReplicationInfo +
                    KFOffsets::PlayerReplicationInfo::Cash,
                    snapshot.cash
                );
        }


        // --------------------------------------------------------
        // Weapon
        // --------------------------------------------------------

        if (snapshot.context.weapon != 0)
        {
            snapshot.weaponName =
                KFMemory::GetClassName(
                    snapshot.context.weapon
                );


            const bool hasMagazineCapacity =
                KFMemory::Read(
                    snapshot.context.weapon +
                    KFOffsets::Weapon::MagazineCapacity,
                    snapshot.magazineCapacity
                );


            const bool hasLoadedAmmo =
                KFMemory::Read(
                    snapshot.context.weapon +
                    KFOffsets::Weapon::LoadedAmmo,
                    snapshot.ammo
                );


            snapshot.hasAmmo =
                hasMagazineCapacity &&
                hasLoadedAmmo;
        }


        return true;
    }


    // ============================================================
    // IMPRIMIR PLAYER INFO
    // ============================================================

    inline void PrintPlayerInfo()
    {
        Snapshot player;


        if (!ReadSnapshot(player))
        {
            std::cout
                << "\n[PLAYER] No se pudo resolver "
                << "el jugador local.\n";

            return;
        }


        uintptr_t controllerVTable = 0;


        KFMemory::Read(
            player.context.controller,
            controllerVTable
        );


        // ========================================================
        // HEADER
        // ========================================================

        std::cout
            << "\n========================================\n"
            << "              PLAYER INFO\n"
            << "========================================\n";


        // ========================================================
        // DIRECCIONES
        // ========================================================

        std::cout
            << std::hex
            << std::uppercase
            << std::showbase;


        std::cout
            << "[Core.dll]             "
            << KFMemory::CoreBase()
            << '\n';


        std::cout
            << "[GObjHash]             "
            << KFMemory::CoreBase() +
            KFOffsets::Global::GObjHash
            << '\n';


        std::cout
            << "[Engine.dll]           "
            << KFMemory::EngineBase()
            << '\n';


        std::cout
            << "[Controller]           "
            << player.context.controller
            << '\n';


        std::cout
            << "[Controller VTable]    "
            << controllerVTable
            << '\n';


        std::cout
            << "[APawn]                "
            << player.context.pawn
            << '\n';


        std::cout
            << "[PlayerReplicationInfo] "
            << player.context.playerReplicationInfo
            << '\n';


        std::cout
            << "[Weapon]               "
            << player.context.weapon
            << '\n';


        // Regresar a decimal.
        std::cout
            << std::dec
            << std::noshowbase
            << std::nouppercase;


        // ========================================================
        // PLAYER
        // ========================================================

        std::cout
            << "\n--------------- PLAYER -----------------\n";


        if (player.hasHealth)
        {
            std::cout
                << "[Health]               "
                << player.health
                << '\n';
        }


        if (player.hasArmor)
        {
            std::cout
                << "[Armor]                "
                << std::fixed
                << std::setprecision(1)
                << player.armor
                << '\n';
        }


        if (player.hasCash)
        {
            std::cout
                << "[Cash]                 "
                << std::fixed
                << std::setprecision(0)
                << player.cash
                << '\n';
        }


        // ========================================================
        // POSITION
        // ========================================================

        if (player.hasPosition)
        {
            std::cout
                << "\n-------------- POSITION ----------------\n";


            std::cout
                << std::fixed
                << std::setprecision(3);


            std::cout
                << "[X]                    "
                << player.x
                << '\n';


            std::cout
                << "[Y]                    "
                << player.y
                << '\n';


            std::cout
                << "[Z]                    "
                << player.z
                << '\n';
        }


        // ========================================================
        // WEAPON
        // ========================================================

        if (player.context.weapon != 0)
        {
            std::cout
                << "\n--------------- WEAPON -----------------\n";


            if (!player.weaponName.empty())
            {
                std::cout
                    << "[Class]                 "
                    << player.weaponName
                    << '\n';
            }


            if (player.hasAmmo)
            {
                std::cout
                    << "[Loaded Ammo]           "
                    << player.ammo
                    << '\n';


                std::cout
                    << "[Magazine Capacity]     "
                    << player.magazineCapacity
                    << '\n';
            }
        }


        std::cout
            << "========================================\n\n";
    }
}