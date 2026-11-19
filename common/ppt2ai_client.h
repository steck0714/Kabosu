// PPT2NX-AI IPC client
#pragma once
#include "ppt2ai_ipc.h"

#ifdef __cplusplus
extern "C" {
#endif

Result ppt2aiInitialize(void);
void ppt2aiExit(void);
// needs pmdmntInitialize
bool ppt2aiIsRunning(void);

Result ppt2aiGetVersion(u32 *out);
Result ppt2aiGetStatus(Ppt2AiStatus *out);
Result ppt2aiGetConfig(Ppt2AiConfig *out);
Result ppt2aiSetConfig(const Ppt2AiConfig *config);
Result ppt2aiSetManualInput(const Ppt2AiManualInput *input);
Result ppt2aiGetBoard(u32 player, Ppt2AiBoard *out);
Result ppt2aiReconnectController(void);
Result ppt2aiSetAttachPaused(bool paused);
Result ppt2aiSetOverlayVisible(bool visible);
// player slots, 0 = 1P
Result ppt2aiSwapControllers(u8 slot_a, u8 slot_b);
Result ppt2aiExitSysmodule(void);

#ifdef __cplusplus
}
#endif
