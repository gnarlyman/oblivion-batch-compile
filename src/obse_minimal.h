#pragma once
//
// obse_minimal.h — Minimal subset of OBSE plugin interface types.
//
// xOBSE's full PluginAPI.h transitively includes ~10 helper headers from the
// OBSE common library (IPrefix, IErrors, IDebugLog, IDataStream, ITypes,
// ISingleton, …). For our use we only need the plugin entry-point ABI and
// the messaging interface, so we redeclare just enough here to be source-
// compatible and avoid the dependency.
//
// Layouts derived from llde/Oblivion-Script-Extender obse/PluginAPI.h.

#include <cstdint>

using UInt8  = std::uint8_t;
using UInt16 = std::uint16_t;
using UInt32 = std::uint32_t;
using SInt32 = std::int32_t;

using PluginHandle = UInt32;
constexpr PluginHandle kPluginHandle_Invalid = 0xFFFFFFFFu;

// Interface IDs (PluginAPI.h: enum { kInterface_Messaging, … }).
constexpr UInt32 kInterface_Serialization = 0;
constexpr UInt32 kInterface_Console       = 1;
constexpr UInt32 kInterface_Messaging     = 2;
constexpr UInt32 kInterface_CommandTable  = 3;
constexpr UInt32 kInterface_StringVar     = 4;
constexpr UInt32 kInterface_ArrayVar      = 5;
constexpr UInt32 kInterface_Script        = 6;

struct PluginInfo {
    enum { kInfoVersion = 2 };
    UInt32      infoVersion;
    const char* name;
    UInt32      version;
};

struct OBSEInterface {
    UInt32 obseVersion;
    UInt32 oblivionVersion;
    UInt32 editorVersion;
    UInt32 isEditor;

    void*       (*QueryInterface)(UInt32 id);
    PluginHandle (*GetPluginHandle)(void);

    // Older PluginAPI.h has additional members past this point (release index,
    // GetReleaseIndex, GetSingleton, etc.). We don't call them; struct size
    // doesn't matter to us because the host (obse) constructed it and passes a
    // pointer — we only read fields up to QueryInterface/GetPluginHandle.
};

struct OBSEMessagingInterface {
    UInt32 version;

    enum {
        kMessage_PostLoad           = 0,
        kMessage_PostPostLoad       = 1,
        kMessage_ExitGame           = 2,
        kMessage_ExitToMainMenu     = 3,
        kMessage_LoadGame           = 4,
        kMessage_SaveGame           = 5,
        kMessage_Precompile         = 6,
        kMessage_PreLoadGame        = 7,
        kMessage_ExitGame_Console   = 8,
        kMessage_DeleteGame         = 9,
        kMessage_InputLoop          = 10,
        kMessage_RuntimeScriptError = 11,
        kMessage_DeletePermanentRefr= 12,
        kMessage_GameLoaded         = 13,
        kMessage_ScriptCompile      = 14,
        kMessage_EventListDestroyed = 15,
        kMessage_PostQueryPlugins   = 16,
    };

    struct Message {
        const char* sender;
        UInt32      type;
        UInt32      dataLen;
        void*       data;
    };

    using EventCallback = void(*)(Message* msg);

    bool (*RegisterListener)(PluginHandle listener, const char* sender, EventCallback handler);
    bool (*Dispatch)(PluginHandle sender, UInt32 messageType, void* data,
                     UInt32 dataLen, const char* receiver);
};
