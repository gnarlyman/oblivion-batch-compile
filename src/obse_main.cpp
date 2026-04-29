// obse_main.cpp — OBSE plugin entry point and CS-window-ready dispatch.

#include <Windows.h>

#include "obse_minimal.h"

#include "batch.h"
#include "config.h"
#include "cs_internals.h"
#include "log.h"

namespace {

constexpr UINT kBatchKickoffMessage = WM_USER + 0xBC;  // 0xBC = "Batch Compile"

OBSEMessagingInterface* g_msgIntfc = nullptr;
PluginHandle            g_pluginHandle = kPluginHandle_Invalid;
obc::BatchConfig        g_config;
WNDPROC                 g_origWndProc = nullptr;
HWND                    g_subclassedHwnd = nullptr;

LRESULT CALLBACK BatchWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == kBatchKickoffMessage) {
        OBC_LOG("WndProc: kickoff received on main thread");
        // Restore original WndProc before running the batch — keeps the
        // hook surgical and prevents double-entry.
        if (g_origWndProc) {
            SetWindowLongPtrA(hwnd, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(g_origWndProc));
            g_origWndProc = nullptr;
            g_subclassedHwnd = nullptr;
        }
        obc::RunBatch(g_config);
        return 0;
    }
    WNDPROC orig = g_origWndProc;
    if (!orig) return DefWindowProcA(hwnd, msg, wParam, lParam);
    return CallWindowProcA(orig, hwnd, msg, wParam, lParam);
}

DWORD WINAPI WaitAndKickoffThread(LPVOID) {
    OBC_LOG("Worker: waiting for CS main window");
    HWND hwnd = nullptr;
    // Poll up to 60 s for the CS to bring up its main window.
    for (int i = 0; i < 600; ++i) {
        hwnd = obc::cs::CSWindow();
        if (hwnd) break;
        Sleep(100);
    }
    if (!hwnd) {
        OBC_LOG("Worker: timeout waiting for CS main window");
        return 1;
    }
    OBC_LOG("Worker: CS main window = %p", static_cast<void*>(hwnd));
    // Allow the CS another moment to finish its startup work (data-handler
    // constructor, INI loads, idle hooks). 2 s is conservative.
    Sleep(2000);

    // Subclass the main WndProc so we can dispatch the batch op on the
    // main thread. SetWindowLongPtrA is safe to call from any thread.
    g_subclassedHwnd = hwnd;
    auto orig = reinterpret_cast<WNDPROC>(
        SetWindowLongPtrA(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(BatchWndProc)));
    g_origWndProc = orig;
    OBC_LOG("Worker: WndProc subclassed (orig=%p)", reinterpret_cast<void*>(orig));

    PostMessageA(hwnd, kBatchKickoffMessage, 0, 0);
    OBC_LOG("Worker: kickoff posted; exiting worker thread");
    return 0;
}

void OBSEMessageHandler(OBSEMessagingInterface::Message* msg) {
    switch (msg->type) {
    case OBSEMessagingInterface::kMessage_PostLoad:
        OBC_LOG("OBSE: PostLoad");
        break;
    case OBSEMessagingInterface::kMessage_PostPostLoad:
        OBC_LOG("OBSE: PostPostLoad");
        if (g_config.LoadFromEnvironment()) {
            OBC_LOG("OBSE: batch mode active; spawning worker thread");
            HANDLE h = CreateThread(nullptr, 0, &WaitAndKickoffThread, nullptr, 0, nullptr);
            if (h) CloseHandle(h);
        } else {
            OBC_LOG("OBSE: CSE_BATCH_ONE not set; plugin staying idle");
        }
        break;
    }
}

}  // namespace

extern "C" {

__declspec(dllexport) bool OBSEPlugin_Query(const OBSEInterface* obse, PluginInfo* info) {
    obc::log::Init();
    OBC_LOG("Query: editor=%d, obseVersion=%u, editorVersion=%u",
            obse->isEditor ? 1 : 0,
            static_cast<unsigned>(obse->obseVersion),
            static_cast<unsigned>(obse->editorVersion));

    info->infoVersion = PluginInfo::kInfoVersion;
    info->name = "oblivion-batch-compile";
    info->version = OBC_VERSION;

    if (!obse->isEditor) {
        OBC_LOG("Query: refusing to load outside editor mode");
        return false;
    }
    if (obse->obseVersion < 21) {
        OBC_LOG("Query: OBSE %u older than required (21)", obse->obseVersion);
        return false;
    }
    return true;
}

__declspec(dllexport) bool OBSEPlugin_Load(const OBSEInterface* obse) {
    OBC_LOG("Load: registering OBSE message handler");
    g_pluginHandle = obse->GetPluginHandle();

    if (!obse->isEditor) return false;

    g_msgIntfc = static_cast<OBSEMessagingInterface*>(obse->QueryInterface(kInterface_Messaging));
    if (!g_msgIntfc) {
        OBC_LOG("Load: messaging interface query failed");
        return false;
    }
    if (!g_msgIntfc->RegisterListener(g_pluginHandle, "OBSE", &OBSEMessageHandler)) {
        OBC_LOG("Load: RegisterListener(OBSE) failed");
        return false;
    }
    return true;
}

}  // extern "C"
