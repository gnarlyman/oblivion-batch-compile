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

// Interface IDs. Order matches xOBSE obse/PluginAPI.h `enum { kInterface_Console, … }`.
constexpr UInt32 kInterface_Console       = 0;
constexpr UInt32 kInterface_Serialization = 1;
constexpr UInt32 kInterface_StringVar     = 2;
constexpr UInt32 kInterface_IO            = 3;
constexpr UInt32 kInterface_Messaging     = 4;
constexpr UInt32 kInterface_ArrayVar      = 5;
constexpr UInt32 kInterface_CommandTable  = 6;
constexpr UInt32 kInterface_Script        = 7;

struct PluginInfo {
    enum { kInfoVersion = 2 };
    UInt32      infoVersion;
    const char* name;
    UInt32      version;
};

// Layout matches xOBSE obse/PluginAPI.h::OBSEInterface. We don't actually call
// the command-table or directory functions, but the struct slots in front of
// QueryInterface MUST exist so the offset to QueryInterface is correct.
struct OBSEInterface {
    UInt32 obseVersion;
    UInt32 oblivionVersion;
    UInt32 editorVersion;
    UInt32 isEditor;
    bool   (*RegisterCommand)(void* info);       // CommandInfo*; opaque to us
    void   (*SetOpcodeBase)(UInt32 opcode);
    void*  (*QueryInterface)(UInt32 id);
    PluginHandle (*GetPluginHandle)(void);
    bool   (*RegisterTypedCommand)(void* info, int retnType);
    const char* (*GetOblivionDirectory)();
    bool   (*GetPluginLoaded)(const char* pluginName);
    UInt32 (*GetPluginVersion)(const char* pluginName);
};

struct OBSEMessagingInterface {
    UInt32 version;

    // Order matches xOBSE obse/PluginAPI.h. PostPostLoad was added after the
    // earlier ExitGame / SaveGame / Precompile / PreLoadGame messages.
    enum {
        kMessage_PostLoad           = 0,
        kMessage_ExitGame           = 1,
        kMessage_ExitToMainMenu     = 2,
        kMessage_LoadGame           = 3,
        kMessage_SaveGame           = 4,
        kMessage_Precompile         = 5,  // EDITOR: script editor save trigger
        kMessage_PreLoadGame        = 6,
        kMessage_ExitGame_Console   = 7,
        kMessage_PostLoadGame       = 8,
        kMessage_PostPostLoad       = 9,
        kMessage_RuntimeScriptError = 10,
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
