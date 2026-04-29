#pragma once

#include "config.h"
#include "result.h"

namespace obc {

// RunBatch performs the entire load → recompile → save → exit cycle on
// the CS main thread. Must be called *from* the main thread (typically
// via a posted WM_USER message handled in our window subclass).
//
// On completion, the result JSON has been written to cfg.resultPath
// (if set) and WM_CLOSE has been posted to the CS main window.
void RunBatch(const BatchConfig& cfg);

}  // namespace obc
