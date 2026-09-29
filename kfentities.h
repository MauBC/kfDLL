#pragma once

#include <cstdint>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "kfoffsets.h"
#include "kfmemory.h"
#include "kfplayer.h"


namespace KFEntities
{
    // ============================================================
    // CONFIGURACION
    // ============================================================

    namespace Config
    {
        // Protección por si EntityCount estuviera corrupto.
        constexpr int MaxEntitiesToScan = 100000;


        // Vimos un Gorefast con HealthRaw = 17 millones.
        // No queremos imprimir eso como vida válida.
        //
        // Esto NO determina si el Pawn existe.
        // Solo determina si mostramos su Health como confiable.
        constexpr int MaxReasonableHealth = 100000;
    }


    // ============================================================
    // RELACION CON EL JUGADOR
    // ============================================================

    enum class Relation
    {
        Local,
        Ally,
        Enemy,
        Unknown
    };


    inline const char* RelationToString(
        Relation relation
    )
    {
        switch (relation)
        {
        case Relation::Local:
            return "LOCAL";

        case Relation::Ally:
            return "ALLY";

        case Relation::Enemy:
            return "ENEMY";

        default:
            return "UNKNOWN";
        }
    }


    // ============================================================
    // ENTIDAD
    // ============================================================

    struct Entity
    {
        uintptr_t pawn = 0;
        uintptr_t controller = 0;
        uintptr_t pri = 0;
        uintptr_t roster = 0;

        std::string className;

        Relation relation =
            Relation::Unknown;


        int health = 0;

        bool healthReliable = false;


        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;

        // Distancia 3D respecto al jugador local.
        float distanceToLocal = 0.0f;
    };


    // ============================================================
    // RESUMEN
    // ============================================================

    struct Summary
    {
        int total = 0;

        int local = 0;
        int allies = 0;
        int enemies = 0;
        int unknown = 0;
    };


    // ============================================================
    // UCLASS Pawn
    //
    // Partimos de nuestro propio jugador:
    //
    // KFHumanPawn
    //   -> KFPawn
    //   -> xPawn
    //   -> UnrealPawn
    //   -> Pawn
    //   -> Actor
    //   -> Object
    //
    // Buscamos dinámicamente la UClass llamada "Pawn".
    // ============================================================

    inline uintptr_t FindPawnClass(
        uintptr_t localPawn
    )
    {
        if (localPawn == 0)
        {
            return 0;
        }


        uintptr_t currentClass = 0;

        if (!KFMemory::Read(
            localPawn +
            KFOffsets::UObject::Class,
            currentClass
        ))
        {
            return 0;
        }


        for (int depth = 0; depth < 32; ++depth)
        {
            if (currentClass == 0)
            {
                return 0;
            }


            const std::string className =
                KFMemory::GetObjectName(
                    currentClass
                );


            if (className == "Pawn")
            {
                return currentClass;
            }


            uintptr_t parentClass = 0;

            if (!KFMemory::Read(
                currentClass +
                KFOffsets::UClass::SuperClass,
                parentClass
            ))
            {
                return 0;
            }


            // Protección contra ciclos.
            if (parentClass == currentClass)
            {
                return 0;
            }


            currentClass =
                parentClass;
        }


        return 0;
    }


    // ============================================================
    // ¿EL OBJETO HEREDA DE Pawn?
    // ============================================================

    inline bool IsPawn(
        uintptr_t object,
        uintptr_t pawnClass
    )
    {
        if (
            object == 0 ||
            pawnClass == 0
            )
        {
            return false;
        }


        uintptr_t currentClass = 0;

        if (!KFMemory::Read(
            object +
            KFOffsets::UObject::Class,
            currentClass
        ))
        {
            return false;
        }


        for (int depth = 0; depth < 32; ++depth)
        {
            if (currentClass == 0)
            {
                return false;
            }


            if (currentClass == pawnClass)
            {
                return true;
            }


            uintptr_t parentClass = 0;

            if (!KFMemory::Read(
                currentClass +
                KFOffsets::UClass::SuperClass,
                parentClass
            ))
            {
                return false;
            }


            if (parentClass == currentClass)
            {
                return false;
            }


            currentClass =
                parentClass;
        }


        return false;
    }


    // ============================================================
    // CLASIFICACION
    // ============================================================

    inline Relation GetRelation(
        uintptr_t pawn,
        uintptr_t localPawn,
        uintptr_t pri,
        uintptr_t roster,
        uintptr_t localRoster,
        uintptr_t controller
    )
    {
        // Nuestro propio Pawn.
        if (pawn == localPawn)
        {
            return Relation::Local;
        }


        // --------------------------------------------------------
        // Jugadores / bots con PRI.
        //
        // En nuestra prueba:
        //
        // todos los KFHumanPawnLight:
        // PRI +0x410 -> mismo xTeamRoster que nosotros.
        // --------------------------------------------------------

        if (pri != 0)
        {
            if (
                localRoster != 0 &&
                roster != 0
                )
            {
                if (roster == localRoster)
                {
                    return Relation::Ally;
                }

                // Tiene PRI pero pertenece a otro roster.
                return Relation::Enemy;
            }


            return Relation::Unknown;
        }


        // --------------------------------------------------------
        // Zeds.
        //
        // En nuestras pruebas:
        //
        // ZombieCrawler_STANDARD
        // ZombieGorefast_STANDARD
        //
        // Controller != NULL
        // PRI        == NULL
        // --------------------------------------------------------

        if (controller != 0)
        {
            return Relation::Enemy;
        }


        return Relation::Unknown;
    }


    // ============================================================
    // LEER UNA ENTIDAD Pawn
    // ============================================================

    inline bool ReadEntity(
        uintptr_t pawn,
        uintptr_t localPawn,
        uintptr_t localRoster,
        Entity& entity
    )
    {
        entity = {};

        if (pawn == 0)
        {
            return false;
        }


        entity.pawn =
            pawn;


        // --------------------------------------------------------
        // Clase
        // --------------------------------------------------------

        entity.className =
            KFMemory::GetClassName(
                pawn
            );


        if (entity.className.empty())
        {
            entity.className =
                "<unnamed>";
        }


        // --------------------------------------------------------
        // Controller
        // --------------------------------------------------------

        KFMemory::Read(
            pawn +
            KFOffsets::Pawn::Controller,
            entity.controller
        );


        // --------------------------------------------------------
        // PlayerReplicationInfo
        // --------------------------------------------------------

        KFMemory::Read(
            pawn +
            KFOffsets::Pawn::PlayerReplicationInfo,
            entity.pri
        );


        // --------------------------------------------------------
        // Team Roster
        // --------------------------------------------------------

        if (entity.pri != 0)
        {
            KFMemory::Read(
                entity.pri +
                KFOffsets::PlayerReplicationInfo::TeamRoster,
                entity.roster
            );
        }


        // --------------------------------------------------------
        // Health
        // --------------------------------------------------------

        KFMemory::Read(
            pawn +
            KFOffsets::Pawn::Health,
            entity.health
        );


        entity.healthReliable =
            entity.health > 0 &&
            entity.health <=
            Config::MaxReasonableHealth;


        // --------------------------------------------------------
        // Posición
        // --------------------------------------------------------

        const bool readX =
            KFMemory::Read(
                pawn +
                KFOffsets::Pawn::X,
                entity.x
            );


        const bool readY =
            KFMemory::Read(
                pawn +
                KFOffsets::Pawn::Y,
                entity.y
            );


        const bool readZ =
            KFMemory::Read(
                pawn +
                KFOffsets::Pawn::Z,
                entity.z
            );


        if (
            !readX ||
            !readY ||
            !readZ
            )
        {
            return false;
        }


        // Evitamos coordenadas corruptas.
        if (
            !std::isfinite(entity.x) ||
            !std::isfinite(entity.y) ||
            !std::isfinite(entity.z)
            )
        {
            return false;
        }


        // --------------------------------------------------------
        // Relación
        // --------------------------------------------------------

        entity.relation =
            GetRelation(
                pawn,
                localPawn,
                entity.pri,
                entity.roster,
                localRoster,
                entity.controller
            );


        return true;
    }


    // ============================================================
    // OBTENER PAWNS ACTIVOS
    // ============================================================

    inline std::vector<Entity> GetLivingEntities()
    {
        std::vector<Entity> entities;


        // --------------------------------------------------------
        // Resolver jugador local.
        // --------------------------------------------------------

        KFPlayer::Context localContext;

        if (!KFPlayer::ResolveContext(
            localContext
        ))
        {
            return entities;
        }


        const uintptr_t localPawn =
            localContext.pawn;


        if (localPawn == 0)
        {
            return entities;
        }


        // --------------------------------------------------------
        // Posicion del jugador local.
        //
        // Solo se lee una vez antes de recorrer EntityList.
        // --------------------------------------------------------

        float localX = 0.0f;
        float localY = 0.0f;
        float localZ = 0.0f;


        const bool localXOk =
            KFMemory::Read(
                localPawn +
                KFOffsets::Pawn::X,
                localX
            );


        const bool localYOk =
            KFMemory::Read(
                localPawn +
                KFOffsets::Pawn::Y,
                localY
            );


        const bool localZOk =
            KFMemory::Read(
                localPawn +
                KFOffsets::Pawn::Z,
                localZ
            );


        if (
            !localXOk ||
            !localYOk ||
            !localZOk
            )
        {
            return entities;
        }


        // --------------------------------------------------------
        // Obtener roster local.
        // --------------------------------------------------------

        uintptr_t localRoster = 0;

        if (
            localContext.playerReplicationInfo != 0
            )
        {
            KFMemory::Read(
                localContext.playerReplicationInfo +
                KFOffsets::PlayerReplicationInfo::TeamRoster,
                localRoster
            );
        }


        // --------------------------------------------------------
        // Encontrar UClass Pawn.
        // --------------------------------------------------------

        const uintptr_t pawnClass =
            FindPawnClass(
                localPawn
            );


        if (pawnClass == 0)
        {
            return entities;
        }


        // --------------------------------------------------------
        // Obtener ULevel.
        // --------------------------------------------------------

        uintptr_t level = 0;

        if (!KFMemory::Read(
            localPawn +
            KFOffsets::Pawn::Level,
            level
        ))
        {
            return entities;
        }


        if (level == 0)
        {
            return entities;
        }


        // --------------------------------------------------------
        // EntityList.
        // --------------------------------------------------------

        uintptr_t entityList = 0;
        int entityCount = 0;


        if (!KFMemory::Read(
            level +
            KFOffsets::Level::EntityList,
            entityList
        ))
        {
            return entities;
        }


        if (!KFMemory::Read(
            level +
            KFOffsets::Level::EntityCount,
            entityCount
        ))
        {
            return entities;
        }


        if (
            entityList == 0 ||
            entityCount <= 0 ||
            entityCount >
            Config::MaxEntitiesToScan
            )
        {
            return entities;
        }


        // --------------------------------------------------------
        // Recorrer todos los actores.
        // --------------------------------------------------------

        entities.reserve(64);


        for (
            int index = 0;
            index < entityCount;
            ++index
            )
        {
            uintptr_t actor = 0;


            if (!KFMemory::Read(
                entityList +
                static_cast<uintptr_t>(index) *
                sizeof(uintptr_t),
                actor
            ))
            {
                continue;
            }


            if (actor == 0)
            {
                continue;
            }


            // Solo nos interesan Pawn o derivados.
            if (!IsPawn(
                actor,
                pawnClass
            ))
            {
                continue;
            }


            Entity entity;


            if (!ReadEntity(
                actor,
                localPawn,
                localRoster,
                entity
            ))
            {
                continue;
            }


            // ----------------------------------------------------
            // Distancia 3D respecto al jugador local.
            // ----------------------------------------------------

            const float dx =
                entity.x - localX;

            const float dy =
                entity.y - localY;

            const float dz =
                entity.z - localZ;


            entity.distanceToLocal =
                std::sqrt(
                    dx * dx +
                    dy * dy +
                    dz * dz
                );


            // ----------------------------------------------------
            // ¿Está vivo?
            //
            // Para Health confiable:
            //   health > 0.
            //
            // Para casos raros como el Gorefast del testmap,
            // conservamos el Pawn si tiene Controller aunque el
            // Health raw sea absurdo.
            // ----------------------------------------------------

            // Pawn con vida normal positiva -> aceptado.
            //
            // Pawn con Health raro -> solo lo conservamos si tiene Controller.
            // Esto contempla el Gorefast raro de nuestro testmap.
            if (
                !entity.healthReliable &&
                entity.controller == 0
                )
            {
                continue;
            }


            entities.push_back(
                entity
            );
        }


        return entities;
    }


    // ============================================================
    // RESUMEN
    // ============================================================

    inline Summary BuildSummary(
        const std::vector<Entity>& entities
    )
    {
        Summary summary;


        for (const Entity& entity : entities)
        {
            ++summary.total;


            switch (entity.relation)
            {
            case Relation::Local:
                ++summary.local;
                break;


            case Relation::Ally:
                ++summary.allies;
                break;


            case Relation::Enemy:
                ++summary.enemies;
                break;


            default:
                ++summary.unknown;
                break;
            }
        }


        return summary;
    }


    // ============================================================
    // IMPRIMIR ENTIDADES
    // ============================================================

    inline void PrintLivingEntities()
    {
        const std::vector<Entity> entities =
            GetLivingEntities();


        const Summary summary =
            BuildSummary(
                entities
            );


        std::cout
            << "\n============================================================\n"
            << "                    LIVING ENTITIES\n"
            << "============================================================\n";


        if (entities.empty())
        {
            std::cout
                << "[!] No se encontraron Pawns activos.\n"
                << "============================================================\n\n";

            return;
        }


        int index = 0;


        for (const Entity& entity : entities)
        {
            ++index;


            std::cout
                << '\n'
                << '['
                << std::setw(3)
                << std::setfill('0')
                << index
                << "] "
                << entity.className
                << '\n';


            std::cout
                << std::setfill(' ');


            std::cout
                << "      Relation : "
                << RelationToString(
                    entity.relation
                )
                << '\n';


            // ----------------------------------------------------
            // Dirección Pawn.
            // ----------------------------------------------------

            std::cout
                << std::hex
                << std::uppercase
                << std::showbase;


            std::cout
                << "      Pawn     : "
                << entity.pawn
                << '\n';


            std::cout
                << std::dec
                << std::noshowbase
                << std::nouppercase;


            // ----------------------------------------------------
            // Health.
            // ----------------------------------------------------

            if (entity.healthReliable)
            {
                std::cout
                    << "      Health   : "
                    << entity.health
                    << '\n';
            }
            else
            {
                std::cout
                    << "      Health   : N/A"
                    << " (raw="
                    << entity.health
                    << ")\n";
            }


            // ----------------------------------------------------
            // Posición.
            // ----------------------------------------------------

            std::cout
                << std::fixed
                << std::setprecision(3);


            std::cout
                << "      X        : "
                << entity.x
                << '\n';


            std::cout
                << "      Y        : "
                << entity.y
                << '\n';


            std::cout
                << "      Z/Height : "
                << entity.z
                << '\n';


            std::cout
                << "      Distance : "
                << std::fixed
                << std::setprecision(2)
                << entity.distanceToLocal
                << '\n';
        }


        // ========================================================
        // SUMMARY
        // ========================================================

        std::cout
            << "\n============================================================\n"
            << "                       SUMMARY\n"
            << "============================================================\n";


        std::cout
            << "Local player : "
            << summary.local
            << '\n';


        std::cout
            << "Allies       : "
            << summary.allies
            << '\n';


        std::cout
            << "Enemies      : "
            << summary.enemies
            << '\n';


        if (summary.unknown > 0)
        {
            std::cout
                << "Unknown      : "
                << summary.unknown
                << '\n';
        }


        std::cout
            << "Total        : "
            << summary.total
            << '\n';


        std::cout
            << "============================================================\n\n";
    }
}