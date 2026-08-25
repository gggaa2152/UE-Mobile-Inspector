#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Dobby Hook lightweight C interface
int DobbyHook(void *function_address, void *replace_call, void **origin_call);
int DobbyDestroy(void *function_address);

#ifdef __cplusplus
}
#endif
