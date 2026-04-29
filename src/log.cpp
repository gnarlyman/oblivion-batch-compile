#include "log.h"

#include <Windows.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

namespace obc::log {

namespace {
std::mutex g_mtx;
FILE* g_fp = nullptr;
bool g_initialized = false;

std::string DllDir() {
    HMODULE hm = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&DllDir), &hm);
    char path[MAX_PATH] = {};
    GetModuleFileNameA(hm, path, MAX_PATH);
    std::string s(path);
    auto pos = s.find_last_of("\\/");
    return (pos == std::string::npos) ? std::string{} : s.substr(0, pos);
}

std::string LogPath() {
    char buf[MAX_PATH] = {};
    DWORD n = GetEnvironmentVariableA("OBC_LOG_PATH", buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) return std::string(buf, n);
    auto dir = DllDir();
    if (dir.empty()) return "oblivion-batch-compile.log";
    return dir + "\\oblivion-batch-compile.log";
}
}  // namespace

void Init() {
    std::lock_guard<std::mutex> lk(g_mtx);
    if (g_initialized) return;
    g_initialized = true;
    auto path = LogPath();
    fopen_s(&g_fp, path.c_str(), "w");
    if (g_fp) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(g_fp,
                "[%04d-%02d-%02d %02d:%02d:%02d] oblivion-batch-compile log opened at %s\n",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, path.c_str());
        fflush(g_fp);
    }
}

void Printf(const char* fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if (n >= static_cast<int>(sizeof(buf) - 2)) n = sizeof(buf) - 2;
    if (n > 0 && buf[n - 1] != '\n') {
        buf[n++] = '\n';
        buf[n] = '\0';
    }
    OutputDebugStringA(buf);
    std::lock_guard<std::mutex> lk(g_mtx);
    if (g_fp) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(g_fp, "[%02d:%02d:%02d.%03d] %s",
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buf);
        fflush(g_fp);
    }
}

void Close() {
    std::lock_guard<std::mutex> lk(g_mtx);
    if (g_fp) {
        fclose(g_fp);
        g_fp = nullptr;
    }
    g_initialized = false;
}

}  // namespace obc::log
