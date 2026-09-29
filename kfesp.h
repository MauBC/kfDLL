#pragma once

#include <Windows.h>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

#include "kfcamera.h"
#include "kfentities.h"


namespace KFESP
{
    // ============================================================
    // CONFIGURACION
    // ============================================================

    namespace Config
    {
        // EntityList completo solamente 4 veces por segundo.
        constexpr ULONGLONG CacheRefreshMs =
            250;


        // Geometria visual.
        constexpr float BoxWidthRatio =
            0.46f;


        constexpr float MinBoxHeight =
            12.0f;


        constexpr float MinBoxWidth =
            8.0f;


        constexpr float VerticalPadding =
            3.0f;
    }


    // ============================================================
    // CACHE
    // ============================================================

    struct CachedEnemy
    {
        uintptr_t pawn = 0;

        std::string className;


        // --------------------------------------------------------
        // HEADSHOT OFFSET LOCAL
        //
        // KFMonster::OnlineHeadshotOffset
        //
        // Se lee en RefreshEnemyCache, no cada frame.
        // --------------------------------------------------------

        float headOffsetX = 0.0f;

        float headOffsetY = 0.0f;

        float headOffsetZ = 0.0f;


        bool hasHeadshotOffset = false;


        // No tenemos todavia un offset MaxHealth confirmado.
        //
        // Guardamos el mayor Health confiable observado para ese
        // Pawn. Normalmente el Pawn entra al cache con vida completa.
        int maxObservedHealth = 0;
    };


    inline std::vector<CachedEnemy>
        gEnemyCache;


    inline ULONGLONG
        gLastCacheRefresh = 0;


    inline std::mutex
        gEnemyCacheMutex;


    // ============================================================
    // ENTRADA FINAL DEL ESP
    // ============================================================

    struct Entry
    {
        uintptr_t pawn = 0;


        std::string className;

        std::string displayName;


        int health = 0;

        int maxObservedHealth = 0;

        bool healthReliable = false;


        float healthRatio = 0.0f;

        float distance = 0.0f;

        float eyeHeight = 0.0f;


        // true:
        //     OnlineHeadshotOffset rotado a world-space.
        //
        // false:
        //     fallback clásico Pawn.Z + EyeHeight.
        bool headFromHeadshotOffset = false;


        KFCamera::Vec3 feetWorld;

        KFCamera::Vec3 headWorld;


        KFCamera::ScreenPoint feetScreen;

        KFCamera::ScreenPoint headScreen;


        bool feetProjected = false;

        bool headProjected = false;

        bool onScreen = false;


        // --------------------------------------------------------
        // Box final en coordenadas del viewport
        // --------------------------------------------------------

        float boxLeft = 0.0f;

        float boxTop = 0.0f;

        float boxRight = 0.0f;

        float boxBottom = 0.0f;

        float boxWidth = 0.0f;

        float boxHeight = 0.0f;
    };


    // ============================================================
    // HELPERS
    // ============================================================

    inline float Clamp01(
        float value
    )
    {
        if (value < 0.0f)
        {
            return 0.0f;
        }


        if (value > 1.0f)
        {
            return 1.0f;
        }


        return value;
    }


    // ============================================================
    // HEADSHOT OFFSET HELPERS
    // ============================================================

    inline bool IsReasonableHeadshotOffset(
        float x,
        float y,
        float z
    )
    {
        if (
            !std::isfinite(x) ||
            !std::isfinite(y) ||
            !std::isfinite(z)
            )
        {
            return false;
        }


        constexpr float MaxComponent =
            512.0f;


        if (
            std::fabs(x) > MaxComponent ||
            std::fabs(y) > MaxComponent ||
            std::fabs(z) > MaxComponent
            )
        {
            return false;
        }


        // Un vector completamente cero normalmente significa
        // que no tenemos una configuracion útil.
        return
            std::fabs(x) +
            std::fabs(y) +
            std::fabs(z) >
            0.01f;
    }


    inline bool BuildHeadshotWorld(
        const CachedEnemy& cached,
        float pawnX,
        float pawnY,
        float pawnZ,
        KFCamera::Vec3& result
    )
    {
        result = {};


        if (
            cached.pawn == 0 ||
            !cached.hasHeadshotOffset
            )
        {
            return false;
        }


        int pawnYaw = 0;


        if (!KFMemory::Read(
            cached.pawn +
            KFOffsets::Actor::Yaw,
            pawnYaw
        ))
        {
            return false;
        }


        // UE2:
        //
        // 65536 rotator units = 360 degrees.
        //
        // Para sin/cos podemos trabajar directamente con
        // 0..65535; no necesitamos convertirlo a signed.
        const unsigned int normalizedYaw =
            static_cast<unsigned int>(
                pawnYaw
            ) &
            0xFFFFu;


        constexpr float TwoPi =
            6.28318530717958647692f;


        constexpr float RotatorToRadians =
            TwoPi /
            65536.0f;


        const float radians =
            static_cast<float>(
                normalizedYaw
            ) *
            RotatorToRadians;


        const float cosYaw =
            std::cos(
                radians
            );


        const float sinYaw =
            std::sin(
                radians
            );


        // UE2 local coordinates:
        //
        // X = forward
        // Y = right
        // Z = up
        const float worldDX =
            cached.headOffsetX *
                cosYaw -
            cached.headOffsetY *
                sinYaw;


        const float worldDY =
            cached.headOffsetX *
                sinYaw +
            cached.headOffsetY *
                cosYaw;


        result =
        {
            pawnX +
                worldDX,

            pawnY +
                worldDY,

            pawnZ +
                cached.headOffsetZ
        };


        return
            std::isfinite(result.x) &&
            std::isfinite(result.y) &&
            std::isfinite(result.z);
    }

    // ZombieClot_STANDARD -> Clot
    // ZombieGorefast_STANDARD -> Gorefast
    inline std::string GetDisplayName(
        const std::string& className
    )
    {
        if (className.empty())
        {
            return "Enemy";
        }


        std::string result =
            className;


        constexpr const char* ZombiePrefix =
            "Zombie";


        if (
            result.rfind(
                ZombiePrefix,
                0
            ) == 0
            )
        {
            result.erase(
                0,
                6
            );
        }


        const size_t suffix =
            result.find('_');


        if (suffix != std::string::npos)
        {
            result.resize(
                suffix
            );
        }


        if (result.empty())
        {
            return className;
        }


        return result;
    }


    inline size_t CacheSize()
    {
        std::lock_guard<std::mutex> lock(
            gEnemyCacheMutex
        );


        return
            gEnemyCache.size();
    }


    inline void ForceRefresh()
    {
        std::lock_guard<std::mutex> lock(
            gEnemyCacheMutex
        );


        gEnemyCache.clear();

        gLastCacheRefresh = 0;
    }


    // ============================================================
    // REFRESH CARO
    //
    // Esta funcion SI recorre EntityList.
    // ============================================================

    inline void RefreshEnemyCache()
    {
        const std::vector<KFEntities::Entity> entities =
            KFEntities::GetLivingEntities();


        std::vector<CachedEnemy> oldCache;


        {
            std::lock_guard<std::mutex> lock(
                gEnemyCacheMutex
            );


            oldCache =
                gEnemyCache;
        }


        // Lookup O(1) promedio para conservar MaxHealth entre
        // refreshes. Evita recorrer oldCache por cada enemigo.
        std::unordered_map<uintptr_t, int>
            oldHealthByPawn;


        oldHealthByPawn.reserve(
            oldCache.size()
        );


        for (
            const CachedEnemy& oldEnemy :
            oldCache
            )
        {
            oldHealthByPawn[
                oldEnemy.pawn
            ] =
                oldEnemy.maxObservedHealth;
        }


        std::vector<CachedEnemy> newCache;


        newCache.reserve(
            entities.size()
        );


        for (
            const KFEntities::Entity& entity :
            entities
            )
        {
            if (
                entity.relation !=
                KFEntities::Relation::Enemy
                )
            {
                continue;
            }


            CachedEnemy enemy;


            enemy.pawn =
                entity.pawn;


            enemy.className =
                entity.className;


            // ====================================================
            // HEADSHOT OFFSET
            //
            // FVector:
            //
            // +00 X
            // +04 Y
            // +08 Z
            //
            // Se cachea porque estos valores son configuración del
            // tipo de Zed y no necesitamos releerlos a 60 FPS.
            // ====================================================

            float headOffsetX = 0.0f;
            float headOffsetY = 0.0f;
            float headOffsetZ = 0.0f;


            const bool headOffsetRead =
                KFMemory::Read(
                    entity.pawn +
                    KFOffsets::KFMonster::OnlineHeadshotOffset +
                    0x00,
                    headOffsetX
                ) &&
                KFMemory::Read(
                    entity.pawn +
                    KFOffsets::KFMonster::OnlineHeadshotOffset +
                    0x04,
                    headOffsetY
                ) &&
                KFMemory::Read(
                    entity.pawn +
                    KFOffsets::KFMonster::OnlineHeadshotOffset +
                    0x08,
                    headOffsetZ
                );


            if (
                headOffsetRead &&
                IsReasonableHeadshotOffset(
                    headOffsetX,
                    headOffsetY,
                    headOffsetZ
                )
                )
            {
                enemy.headOffsetX =
                    headOffsetX;


                enemy.headOffsetY =
                    headOffsetY;


                enemy.headOffsetZ =
                    headOffsetZ;


                enemy.hasHeadshotOffset =
                    true;
            }


            if (entity.healthReliable)
            {
                enemy.maxObservedHealth =
                    entity.health;
            }


            // ----------------------------------------------------
            // Conservar MaxHealth observado entre refreshes.
            // ----------------------------------------------------

            const auto oldHealth =
                oldHealthByPawn.find(
                    enemy.pawn
                );


            if (
                oldHealth !=
                oldHealthByPawn.end()
                )
            {
                enemy.maxObservedHealth =
                    std::max(
                        enemy.maxObservedHealth,
                        oldHealth->second
                    );
            }


            newCache.push_back(
                std::move(enemy)
            );
        }


        {
            std::lock_guard<std::mutex> lock(
                gEnemyCacheMutex
            );


            gEnemyCache =
                std::move(
                    newCache
                );


            gLastCacheRefresh =
                GetTickCount64();
        }
    }


    // ============================================================
    // REFRESH SI HACE FALTA
    // ============================================================

    inline void EnsureEnemyCache()
    {
        const ULONGLONG now =
            GetTickCount64();


        bool needsRefresh = false;


        {
            std::lock_guard<std::mutex> lock(
                gEnemyCacheMutex
            );


            needsRefresh =
                gEnemyCache.empty() ||
                gLastCacheRefresh == 0 ||
                (
                    now -
                    gLastCacheRefresh >=
                    Config::CacheRefreshMs
                );
        }


        if (needsRefresh)
        {
            RefreshEnemyCache();
        }
    }


    // ============================================================
    // SNAPSHOT LOCAL DEL CACHE
    //
    // El render nunca itera directamente sobre el vector global.
    // ============================================================

    inline std::vector<CachedEnemy>
        CopyEnemyCache()
    {
        std::lock_guard<std::mutex> lock(
            gEnemyCacheMutex
        );


        return
            gEnemyCache;
    }


    // ============================================================
    // ACTUALIZAR VIDA MAXIMA OBSERVADA
    // ============================================================

    inline void UpdateObservedHealth(
        const std::vector<Entry>& entries
    )
    {
        if (entries.empty())
        {
            return;
        }


        std::unordered_map<uintptr_t, int>
            observedHealth;


        observedHealth.reserve(
            entries.size()
        );


        for (
            const Entry& entry :
            entries
            )
        {
            if (
                entry.pawn == 0 ||
                !entry.healthReliable
                )
            {
                continue;
            }


            auto iterator =
                observedHealth.find(
                    entry.pawn
                );


            if (
                iterator ==
                observedHealth.end()
                )
            {
                observedHealth.emplace(
                    entry.pawn,
                    entry.health
                );
            }
            else if (
                entry.health >
                iterator->second
                )
            {
                iterator->second =
                    entry.health;
            }
        }


        if (observedHealth.empty())
        {
            return;
        }


        std::lock_guard<std::mutex> lock(
            gEnemyCacheMutex
        );


        for (
            CachedEnemy& cached :
            gEnemyCache
            )
        {
            const auto iterator =
                observedHealth.find(
                    cached.pawn
                );


            if (
                iterator !=
                    observedHealth.end() &&
                iterator->second >
                    cached.maxObservedHealth
                )
            {
                cached.maxObservedHealth =
                    iterator->second;
            }
        }
    }

    // ============================================================
    // CONSTRUIR ESP FRAME
    //
    // Esta funcion corre ~60 FPS.
    //
    // NO escanea EntityList.
    // ============================================================

    inline bool BuildEntriesFast(
        const KFCamera::Viewport& viewport,
        std::vector<Entry>& entries,
        KFCamera::Snapshot* cameraOut = nullptr
    )
    {
        entries.clear();


        KFCamera::Snapshot camera;


        if (!KFCamera::ReadSnapshot(
            camera
        ))
        {
            return false;
        }


        if (cameraOut != nullptr)
        {
            *cameraOut =
                camera;
        }


        EnsureEnemyCache();


        const std::vector<CachedEnemy> enemies =
            CopyEnemyCache();


        entries.reserve(
            enemies.size()
        );


        for (
            const CachedEnemy& cached :
            enemies
            )
        {
            if (cached.pawn == 0)
            {
                continue;
            }


            Entry entry;


            entry.pawn =
                cached.pawn;


            entry.className =
                cached.className;


            entry.displayName =
                GetDisplayName(
                    cached.className
                );


            entry.maxObservedHealth =
                cached.maxObservedHealth;


            // ====================================================
            // POSICION
            // ====================================================

            float x = 0.0f;

            float y = 0.0f;

            float z = 0.0f;


            const bool xOk =
                KFMemory::Read(
                    cached.pawn +
                    KFOffsets::Pawn::X,
                    x
                );


            const bool yOk =
                KFMemory::Read(
                    cached.pawn +
                    KFOffsets::Pawn::Y,
                    y
                );


            const bool zOk =
                KFMemory::Read(
                    cached.pawn +
                    KFOffsets::Pawn::Z,
                    z
                );


            if (
                !xOk ||
                !yOk ||
                !zOk
                )
            {
                continue;
            }


            if (
                !std::isfinite(x) ||
                !std::isfinite(y) ||
                !std::isfinite(z)
                )
            {
                continue;
            }


            // ====================================================
            // HEALTH
            // ====================================================

            const bool healthRead =
                KFMemory::Read(
                    cached.pawn +
                    KFOffsets::Pawn::Health,
                    entry.health
                );


            // Si realmente vale cero, normalmente ya murio.
            if (
                healthRead &&
                entry.health == 0
                )
            {
                continue;
            }


            entry.healthReliable =
                healthRead &&
                entry.health > 0 &&
                entry.health <=
                    KFEntities::Config::MaxReasonableHealth;


            if (
                entry.healthReliable &&
                entry.health >
                entry.maxObservedHealth
                )
            {
                entry.maxObservedHealth =
                    entry.health;
            }


            // ====================================================
            // EYE HEIGHT
            // ====================================================

            bool eyeHeightOk =
                KFMemory::Read(
                    cached.pawn +
                    KFOffsets::Pawn::EyeHeight,
                    entry.eyeHeight
                );


            // Fallback a BaseEyeHeight.
            if (
                !eyeHeightOk ||
                !std::isfinite(
                    entry.eyeHeight
                ) ||
                entry.eyeHeight <= 0.0f ||
                entry.eyeHeight > 256.0f
                )
            {
                entry.eyeHeight = 0.0f;


                eyeHeightOk =
                    KFMemory::Read(
                        cached.pawn +
                        KFOffsets::Pawn::BaseEyeHeight,
                        entry.eyeHeight
                    );
            }


            if (
                !eyeHeightOk ||
                !std::isfinite(
                    entry.eyeHeight
                )
                )
            {
                entry.eyeHeight =
                    0.0f;
            }


            // ====================================================
            // DISTANCIA
            // ====================================================

            const float dx =
                x -
                camera.pawnLocation.x;


            const float dy =
                y -
                camera.pawnLocation.y;


            const float dz =
                z -
                camera.pawnLocation.z;


            entry.distance =
                std::sqrt(
                    dx * dx +
                    dy * dy +
                    dz * dz
                );


            // ====================================================
            // FEET / HEAD
            // ====================================================

            entry.feetWorld =
            {
                x,
                y,
                z
            };


            // ----------------------------------------------------
            // FALLBACK SEGURO
            // ----------------------------------------------------

            entry.headWorld =
            {
                x,
                y,
                z +
                entry.eyeHeight
            };


            // ----------------------------------------------------
            // KF HEADSHOT CENTER
            //
            // Validado empiricamente contra GetBoneCoords("head"):
            //
            // Fleshpound ~6.8 uu de error
            // Scrake     ~5.5 uu de error
            //
            // No usamos OnlineHeadshotScale aqui. El offset por sí
            // solo ya representa correctamente la posición central.
            // ----------------------------------------------------

            KFCamera::Vec3
                headshotWorld;


            if (BuildHeadshotWorld(
                cached,
                x,
                y,
                z,
                headshotWorld
            ))
            {
                entry.headWorld =
                    headshotWorld;


                entry.headFromHeadshotOffset =
                    true;
            }


            // ====================================================
            // WORLD TO SCREEN
            // ====================================================

            entry.feetProjected =
                KFCamera::WorldToScreen(
                    entry.feetWorld,
                    camera,
                    viewport,
                    entry.feetScreen
                );


            entry.headProjected =
                KFCamera::WorldToScreen(
                    entry.headWorld,
                    camera,
                    viewport,
                    entry.headScreen
                );


            if (
                !entry.feetProjected ||
                !entry.headProjected
                )
            {
                entries.push_back(
                    entry
                );

                continue;
            }


            // ====================================================
            // BOX GEOMETRY
            // ====================================================

            float top =
                std::min(
                    entry.headScreen.y,
                    entry.feetScreen.y
                ) -
                Config::VerticalPadding;


            float bottom =
                std::max(
                    entry.headScreen.y,
                    entry.feetScreen.y
                ) +
                Config::VerticalPadding;


            float boxHeight =
                bottom -
                top;


            if (
                boxHeight <
                Config::MinBoxHeight
                )
            {
                const float centerY =
                    (
                        top +
                        bottom
                    ) *
                    0.5f;


                boxHeight =
                    Config::MinBoxHeight;


                top =
                    centerY -
                    boxHeight * 0.5f;


                bottom =
                    centerY +
                    boxHeight * 0.5f;
            }


            const float boxWidth =
                std::max(
                    Config::MinBoxWidth,
                    boxHeight *
                    Config::BoxWidthRatio
                );


            const float centerX =
                (
                    entry.headScreen.x +
                    entry.feetScreen.x
                ) *
                0.5f;


            entry.boxLeft =
                centerX -
                boxWidth * 0.5f;


            entry.boxRight =
                centerX +
                boxWidth * 0.5f;


            entry.boxTop =
                top;


            entry.boxBottom =
                bottom;


            entry.boxWidth =
                boxWidth;


            entry.boxHeight =
                boxHeight;


            // El box puede estar parcialmente fuera de pantalla.
            entry.onScreen =
                entry.boxRight >= 0.0f &&
                entry.boxLeft <=
                    static_cast<float>(
                        viewport.width
                    ) &&
                entry.boxBottom >= 0.0f &&
                entry.boxTop <=
                    static_cast<float>(
                        viewport.height
                    );


            // ====================================================
            // HEALTH RATIO
            // ====================================================

            if (
                entry.healthReliable &&
                entry.maxObservedHealth > 0
                )
            {
                entry.healthRatio =
                    Clamp01(
                        static_cast<float>(
                            entry.health
                        ) /
                        static_cast<float>(
                            entry.maxObservedHealth
                        )
                    );
            }


            entries.push_back(
                entry
            );
        }


        UpdateObservedHealth(
            entries
        );


        return true;
    }


    // ============================================================
    // DEBUG
    // ============================================================

    inline void PrintProjectionDebug()
    {
        KFCamera::Viewport viewport;


        if (!KFCamera::ReadViewport(
            viewport
        ))
        {
            std::cout
                << "\n[W2S] No se pudo leer viewport.\n";

            return;
        }


        ForceRefresh();


        std::vector<Entry> entries;

        KFCamera::Snapshot camera;


        if (!BuildEntriesFast(
            viewport,
            entries,
            &camera
        ))
        {
            std::cout
                << "\n[W2S] No se pudo leer camara.\n";

            return;
        }


        std::cout
            << "\n============================================================\n"
            << "                  WORLD TO SCREEN DEBUG\n"
            << "============================================================\n"
            << "[Viewport] "
            << viewport.width
            << " x "
            << viewport.height
            << '\n'
            << "[Center]   "
            << viewport.width / 2
            << ", "
            << viewport.height / 2
            << '\n'
            << "[FOV]      "
            << std::fixed
            << std::setprecision(3)
            << camera.fov
            << '\n'
            << "[Cache]    "
            << CacheSize()
            << " enemies\n"
            << "============================================================\n";


        for (
            const Entry& entry :
            entries
            )
        {
            std::cout
                << "\n["
                << entry.displayName
                << "]\n";


            std::cout
                << std::fixed
                << std::setprecision(2);


            std::cout
                << "  Distance : "
                << entry.distance
                << '\n';


            std::cout
                << "  EyeHeight: "
                << entry.eyeHeight
                << '\n';


            std::cout
                << "  HeadSrc  : "
                << (
                    entry.headFromHeadshotOffset
                        ?
                        "HEADSHOT_OFFSET"
                        :
                        "EYE_HEIGHT"
                    )
                << '\n';


            if (
                !entry.headProjected ||
                !entry.feetProjected
                )
            {
                std::cout
                    << "  Projection: BEHIND CAMERA\n";

                continue;
            }


            std::cout
                << "  Head     : "
                << entry.headScreen.x
                << ", "
                << entry.headScreen.y
                << '\n';


            std::cout
                << "  Feet     : "
                << entry.feetScreen.x
                << ", "
                << entry.feetScreen.y
                << '\n';


            std::cout
                << "  Box      : "
                << entry.boxWidth
                << " x "
                << entry.boxHeight
                << '\n';


            std::cout
                << "  OnScreen : "
                << (
                    entry.onScreen ?
                    "YES" :
                    "NO"
                )
                << '\n';
        }


        std::cout
            << "\n============================================================\n\n";
    }
}