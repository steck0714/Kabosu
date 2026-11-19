#include "ppt2ai_client.h"

static Service g_ppt2aiSrv;

bool ppt2aiIsRunning(void) {
    // Checks the process (pm:dmnt) because smGetService blocks until the service is registered.
    u64 pid = 0;
    return R_SUCCEEDED(pmdmntGetProcessId(&pid, PPT2AI_PROGRAM_ID)) && pid != 0;
}

Result ppt2aiInitialize(void) {
    if (serviceIsActive(&g_ppt2aiSrv))
        return 0;
    return smGetService(&g_ppt2aiSrv, PPT2AI_SERVICE_NAME);
}

void ppt2aiExit(void) {
    serviceClose(&g_ppt2aiSrv);
}

Result ppt2aiGetVersion(u32 *out) {
    return serviceDispatchOut(&g_ppt2aiSrv, Ppt2AiCmd_GetVersion, *out);
}

Result ppt2aiGetStatus(Ppt2AiStatus *out) {
    return serviceDispatchOut(&g_ppt2aiSrv, Ppt2AiCmd_GetStatus, *out);
}

Result ppt2aiGetConfig(Ppt2AiConfig *out) {
    return serviceDispatchOut(&g_ppt2aiSrv, Ppt2AiCmd_GetConfig, *out);
}

Result ppt2aiSetConfig(const Ppt2AiConfig *config) {
    return serviceDispatchIn(&g_ppt2aiSrv, Ppt2AiCmd_SetConfig, *config);
}

Result ppt2aiSetManualInput(const Ppt2AiManualInput *input) {
    return serviceDispatchIn(&g_ppt2aiSrv, Ppt2AiCmd_SetManualInput, *input);
}

Result ppt2aiGetBoard(u32 player, Ppt2AiBoard *out) {
    return serviceDispatchIn(&g_ppt2aiSrv, Ppt2AiCmd_GetBoard, player,
        .buffer_attrs = { SfBufferAttr_HipcMapAlias | SfBufferAttr_Out },
        .buffers = { { out, sizeof(*out) } });
}

Result ppt2aiReconnectController(void) {
    return serviceDispatch(&g_ppt2aiSrv, Ppt2AiCmd_ReconnectController);
}

Result ppt2aiSetAttachPaused(bool paused) {
    u8 in = paused;
    return serviceDispatchIn(&g_ppt2aiSrv, Ppt2AiCmd_SetAttachPaused, in);
}

Result ppt2aiSetOverlayVisible(bool visible) {
    u8 in = visible;
    return serviceDispatchIn(&g_ppt2aiSrv, Ppt2AiCmd_SetOverlayVisible, in);
}

Result ppt2aiExitSysmodule(void) {
    return serviceDispatch(&g_ppt2aiSrv, Ppt2AiCmd_Exit);
}

Result ppt2aiSwapControllers(u8 slot_a, u8 slot_b) {
    u8 in[2] = {slot_a, slot_b};
    return serviceDispatchIn(&g_ppt2aiSrv, Ppt2AiCmd_SwapControllers, in);
}
