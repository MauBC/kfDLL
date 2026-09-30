#pragma once

#include <Windows.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace KFGameThread
{
#if !defined(_M_IX86)
#error KFGameThread requires Win32/x86.
#endif

    namespace Config
    {
        constexpr size_t HookLength = 5;


        // El target ya fue restaurado cuando comienza este periodo.
        //
        // Solo dejamos tiempo para que la ultima invocacion de
        // HookedMasterProcessPostRender termine completamente antes
        // de permitir FreeLibraryAndExitThread.
        constexpr ULONGLONG UnloadGraceMs =
            150;


        constexpr char MasterProcessPostRenderExport[] =
            "?MasterProcessPostRender@UInteractionMaster@@QAEXPAVUCanvas@@@Z";
    }

    using FrameCallback = void(*)();

    using MasterProcessPostRenderFn =
        void(__thiscall*)(
            void* interactionMaster,
            void* canvas
        );

    inline std::atomic<DWORD> gGameThreadId{ 0 };
    inline std::atomic_bool gInstalled{ false };
    inline std::atomic_bool gUninstallRequested{ false };
    inline std::atomic<unsigned long long> gFrameCount{ 0 };

    inline std::atomic<unsigned long long>
        gCallbackFaultCount{ 0 };


    inline std::atomic<ULONGLONG>
        gUnhookCompletedAt{ 0 };


    inline std::atomic<FrameCallback>
        gFrameCallback{ nullptr };

    // MasterProcessPostRender normalmente no es reentrante,
    // pero bloqueamos una segunda entrada accidental al callback.
    inline std::atomic_flag
        gCallbackBusy = ATOMIC_FLAG_INIT;

    inline void* gTarget = nullptr;
    inline void* gTrampoline = nullptr;

    inline MasterProcessPostRenderFn
        gOriginal = nullptr;

    inline BYTE
        gOriginalBytes[Config::HookLength]{};

    inline void BindCurrentThread()
    {
        gGameThreadId.store(
            GetCurrentThreadId(),
            std::memory_order_release
        );
    }

    inline void ClearBinding()
    {
        gGameThreadId.store(
            0,
            std::memory_order_release
        );
    }

    inline DWORD GameThreadId()
    {
        return gGameThreadId.load(
            std::memory_order_acquire
        );
    }

    inline bool IsGameThread()
    {
        const DWORD expected =
            GameThreadId();

        return
            expected != 0 &&
            expected == GetCurrentThreadId();
    }

    inline bool IsInstalled()
    {
        return gInstalled.load(
            std::memory_order_acquire
        );
    }

    inline unsigned long long FrameCount()
    {
        return gFrameCount.load(
            std::memory_order_acquire
        );
    }


    inline unsigned long long CallbackFaultCount()
    {
        return gCallbackFaultCount.load(
            std::memory_order_acquire
        );
    }


    inline void SetFrameCallback(
        FrameCallback callback
    )
    {
        gFrameCallback.store(
            callback,
            std::memory_order_release
        );
    }

    inline bool WriteRelativeJump(
        void* source,
        void* destination
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
            static_cast<intptr_t>(from + 5);

        if (
            delta < std::numeric_limits<int32_t>::min() ||
            delta > std::numeric_limits<int32_t>::max()
            )
        {
            return false;
        }

        BYTE patch[5]{};

        patch[0] = 0xE9;

        const int32_t relative =
            static_cast<int32_t>(
                delta
            );

        std::memcpy(
            patch + 1,
            &relative,
            sizeof(relative)
        );

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

    inline bool RestoreOriginalBytes()
    {
        if (gTarget == nullptr)
        {
            return true;
        }

        DWORD oldProtection = 0;

        if (!VirtualProtect(
            gTarget,
            Config::HookLength,
            PAGE_EXECUTE_READWRITE,
            &oldProtection
        ))
        {
            return false;
        }

        std::memcpy(
            gTarget,
            gOriginalBytes,
            Config::HookLength
        );

        FlushInstructionCache(
            GetCurrentProcess(),
            gTarget,
            Config::HookLength
        );

        DWORD ignored = 0;

        VirtualProtect(
            gTarget,
            Config::HookLength,
            oldProtection,
            &ignored
        );

        return true;
    }

    inline bool UninstallFromGameThread();

    inline void __fastcall HookedMasterProcessPostRender(
        void* interactionMaster,
        void*,
        void* canvas
    )
    {
        const MasterProcessPostRenderFn original =
            gOriginal;

        if (original != nullptr)
        {
            original(
                interactionMaster,
                canvas
            );
        }

        BindCurrentThread();

        gFrameCount.fetch_add(
            1,
            std::memory_order_relaxed
        );

        const FrameCallback callback =
            gFrameCallback.load(
                std::memory_order_acquire
            );


        if (
            callback != nullptr &&
            !gCallbackBusy.test_and_set(
                std::memory_order_acquire
            )
            )
        {
            __try
            {
                callback();
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                gCallbackFaultCount.fetch_add(
                    1,
                    std::memory_order_relaxed
                );


                // No repetimos un callback que acaba de generar
                // una excepcion dentro del game thread.
                gFrameCallback.store(
                    nullptr,
                    std::memory_order_release
                );
            }


            gCallbackBusy.clear(
                std::memory_order_release
            );
        }

        if (
            gUninstallRequested.load(
                std::memory_order_acquire
            )
            )
        {
            UninstallFromGameThread();
        }
    }

    inline bool Install()
    {
        if (IsInstalled())
        {
            return true;
        }


        // No intentamos reinstalar sobre un estado parcial.
        if (
            gTarget != nullptr ||
            gTrampoline != nullptr ||
            gOriginal != nullptr
            )
        {
            return false;
        }


        gUninstallRequested.store(
            false,
            std::memory_order_release
        );


        gUnhookCompletedAt.store(
            0,
            std::memory_order_release
        );


        gCallbackFaultCount.store(
            0,
            std::memory_order_relaxed
        );


        gFrameCount.store(
            0,
            std::memory_order_relaxed
        );


        gCallbackBusy.clear(
            std::memory_order_release
        );


        ClearBinding();


        HMODULE engine =
            GetModuleHandleA(
                "Engine.dll"
            );

        if (engine == nullptr)
        {
            return false;
        }

        FARPROC exported =
            GetProcAddress(
                engine,
                Config::MasterProcessPostRenderExport
            );

        if (exported == nullptr)
        {
            return false;
        }

        BYTE* target =
            reinterpret_cast<BYTE*>(
                exported
            );

        constexpr BYTE expected[5] =
        {
            0x55,
            0x8B,
            0xEC,
            0x6A,
            0xFF
        };

        if (
            std::memcmp(
                target,
                expected,
                sizeof(expected)
            ) != 0
            )
        {
            return false;
        }

        std::memcpy(
            gOriginalBytes,
            target,
            Config::HookLength
        );

        BYTE* trampoline =
            reinterpret_cast<BYTE*>(
                VirtualAlloc(
                    nullptr,
                    Config::HookLength + 5,
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
            gOriginalBytes,
            Config::HookLength
        );

        if (!WriteRelativeJump(
            trampoline + Config::HookLength,
            target + Config::HookLength
        ))
        {
            VirtualFree(
                trampoline,
                0,
                MEM_RELEASE
            );

            return false;
        }


        // El trampoline ya esta completo.
        //
        // No necesita permanecer writable durante el runtime.
        DWORD trampolineOldProtection =
            0;


        if (!VirtualProtect(
            trampoline,
            Config::HookLength + 5,
            PAGE_EXECUTE_READ,
            &trampolineOldProtection
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
            Config::HookLength + 5
        );


        // Revalidar el prologo justo antes del parche real.
        //
        // Si algun otro codigo lo modifico mientras construimos el
        // trampoline, no debemos sobreescribir ese cambio.
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

            return false;
        }


        gTarget = target;
        gTrampoline = trampoline;

        gOriginal =
            reinterpret_cast<MasterProcessPostRenderFn>(
                trampoline
            );

        if (!WriteRelativeJump(
            target,
            reinterpret_cast<void*>(
                &HookedMasterProcessPostRender
            )
        ))
        {
            gOriginal = nullptr;
            gTarget = nullptr;
            gTrampoline = nullptr;

            VirtualFree(
                trampoline,
                0,
                MEM_RELEASE
            );

            return false;
        }

        gInstalled.store(
            true,
            std::memory_order_release
        );

        return true;
    }

    inline void RequestUninstall()
    {
        if (IsInstalled())
        {
            gUninstallRequested.store(
                true,
                std::memory_order_release
            );
        }
    }

    inline bool UninstallFromGameThread()
    {
        if (!IsInstalled())
        {
            return true;
        }

        if (!IsGameThread())
        {
            return false;
        }

        if (!RestoreOriginalBytes())
        {
            return false;
        }


        // Desde este momento el export original ya no puede entrar
        // nuevamente a HookedMasterProcessPostRender.
        gUninstallRequested.store(
            false,
            std::memory_order_release
        );


        gFrameCallback.store(
            nullptr,
            std::memory_order_release
        );


        ClearBinding();


        void* trampoline =
            gTrampoline;


        gOriginal = nullptr;
        gTarget = nullptr;
        gTrampoline = nullptr;


        if (trampoline != nullptr)
        {
            VirtualFree(
                trampoline,
                0,
                MEM_RELEASE
            );
        }


        // Publicamos primero el momento del unhook.
        gUnhookCompletedAt.store(
            GetTickCount64(),
            std::memory_order_release
        );


        // installed=false es la ultima transicion estructural.
        gInstalled.store(
            false,
            std::memory_order_release
        );


        return true;
    }

    inline bool IsSafeToUnload()
    {
        if (IsInstalled())
        {
            return false;
        }


        const ULONGLONG completedAt =
            gUnhookCompletedAt.load(
                std::memory_order_acquire
            );


        // Hook nunca instalado / Install fallo:
        // no existe una invocacion de nuestro hook que esperar.
        if (completedAt == 0)
        {
            return true;
        }


        return
            GetTickCount64() -
                completedAt >=
            Config::UnloadGraceMs;
    }


    inline bool WaitForUninstall(
        DWORD timeoutMs
    )
    {
        const ULONGLONG start =
            GetTickCount64();

        while (!IsSafeToUnload())
        {
            if (
                GetTickCount64() - start >=
                timeoutMs
                )
            {
                return false;
            }


            Sleep(10);
        }


        return true;
    }
}