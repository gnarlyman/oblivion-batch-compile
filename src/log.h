#pragma once

namespace obc::log {

// Initialize: opens the log file at OBC_LOG_PATH (env) or "oblivion-batch-compile.log"
// next to the loaded DLL. Idempotent.
void Init();

// Print a printf-formatted line to log file + OutputDebugString.
void Printf(const char* fmt, ...);

void Close();

}  // namespace obc::log

#define OBC_LOG(fmt, ...) ::obc::log::Printf(fmt, ##__VA_ARGS__)
