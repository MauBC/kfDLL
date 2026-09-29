#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>

#include "kfoffsets.h"


namespace KFMemory
{
    // ============================================================
    // LECTURA SEGURA
    // ============================================================

    template <typename T>
    inline bool Read(
        uintptr_t address,
        T& value
    )
    {
        if (address == 0)
        {
            return false;
        }


        __try
        {
            value =
                *reinterpret_cast<T*>(
                    address
                    );

            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }


    // ============================================================
    // MODULOS
    // ============================================================

    inline uintptr_t CoreBase()
    {
        return reinterpret_cast<uintptr_t>(
            GetModuleHandleA(
                "Core.dll"
            )
            );
    }


    inline uintptr_t EngineBase()
    {
        return reinterpret_cast<uintptr_t>(
            GetModuleHandleA(
                "Engine.dll"
            )
            );
    }


    // ============================================================
    // FNAMES
    //
    // Core.dll + 0x166674
    //     ↓
    // Names.Data
    //
    // Names.Data[index]
    //     ↓
    // FNameEntry*
    //
    // FNameEntry + 0x0C
    //     ↓
    // texto Unicode
    // ============================================================

    inline uintptr_t GetNamesData()
    {
        const uintptr_t coreBase =
            CoreBase();


        if (coreBase == 0)
        {
            return 0;
        }


        uintptr_t namesData = 0;


        if (!Read(
            coreBase +
            KFOffsets::Global::Names,
            namesData
        ))
        {
            return 0;
        }


        return namesData;
    }


    inline uintptr_t GetFNameEntry(
        uint32_t nameIndex
    )
    {
        const uintptr_t namesData =
            GetNamesData();


        if (namesData == 0)
        {
            return 0;
        }


        uintptr_t entry = 0;


        if (!Read(
            namesData +
            static_cast<uintptr_t>(
                nameIndex
                ) * sizeof(uintptr_t),
            entry
        ))
        {
            return 0;
        }


        return entry;
    }


    // ============================================================
    // LEER STRING UNICODE
    // ============================================================

    inline std::wstring ReadWideString(
        uintptr_t address,
        size_t maxCharacters = 128
    )
    {
        std::wstring result;


        if (address == 0)
        {
            return result;
        }


        result.reserve(
            maxCharacters
        );


        for (
            size_t i = 0;
            i < maxCharacters;
            ++i
            )
        {
            wchar_t character = L'\0';


            if (!Read(
                address +
                i * sizeof(wchar_t),
                character
            ))
            {
                break;
            }


            if (character == L'\0')
            {
                break;
            }


            result.push_back(
                character
            );
        }


        return result;
    }


    // ============================================================
    // WCHAR -> UTF-8
    // ============================================================

    inline std::string WideToUtf8(
        const std::wstring& text
    )
    {
        if (text.empty())
        {
            return {};
        }


        const int requiredBytes =
            WideCharToMultiByte(
                CP_UTF8,
                0,
                text.data(),
                static_cast<int>(
                    text.size()
                    ),
                nullptr,
                0,
                nullptr,
                nullptr
            );


        if (requiredBytes <= 0)
        {
            return {};
        }


        std::string result(
            requiredBytes,
            '\0'
        );


        WideCharToMultiByte(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(
                text.size()
                ),
            result.data(),
            requiredBytes,
            nullptr,
            nullptr
        );


        return result;
    }


    // ============================================================
    // NOMBRE DESDE FName INDEX
    // ============================================================

    inline std::string GetNameFromIndex(
        uint32_t nameIndex
    )
    {
        const uintptr_t entry =
            GetFNameEntry(
                nameIndex
            );


        if (entry == 0)
        {
            return {};
        }


        const std::wstring wideName =
            ReadWideString(
                entry +
                KFOffsets::FNameEntry::Text
            );


        return WideToUtf8(
            wideName
        );
    }


    // ============================================================
    // NOMBRE DEL UObject
    //
    // UObject + 0x24 -> FName index
    // ============================================================

    inline std::string GetObjectName(
        uintptr_t object
    )
    {
        if (object == 0)
        {
            return {};
        }


        uint32_t nameIndex = 0;


        if (!Read(
            object +
            KFOffsets::UObject::NameIndex,
            nameIndex
        ))
        {
            return {};
        }


        return GetNameFromIndex(
            nameIndex
        );
    }


    // ============================================================
    // UCLASS DEL OBJETO
    //
    // UObject + 0x28 -> UClass*
    // ============================================================

    inline uintptr_t GetObjectClass(
        uintptr_t object
    )
    {
        if (object == 0)
        {
            return 0;
        }


        uintptr_t objectClass = 0;


        if (!Read(
            object +
            KFOffsets::UObject::Class,
            objectClass
        ))
        {
            return 0;
        }


        return objectClass;
    }


    // ============================================================
    // NOMBRE DE LA CLASE
    // ============================================================

    inline std::string GetClassName(
        uintptr_t object
    )
    {
        const uintptr_t objectClass =
            GetObjectClass(
                object
            );


        if (objectClass == 0)
        {
            return {};
        }


        return GetObjectName(
            objectClass
        );
    }
}