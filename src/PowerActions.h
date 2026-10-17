#pragma once

// Power menu actions (hold Power for 5 seconds). They are carried out by the
// main loop, outside any activity, so the open screen (e.g. a book) closes
// normally first and saves its state.
namespace poweractions {
void requestShutdown();
void requestRestart();
}  // namespace poweractions
