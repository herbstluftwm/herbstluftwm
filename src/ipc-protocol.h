#ifndef __HERBST_IPC_PROTOCOL_H_
#define __HERBST_IPC_PROTOCOL_H_

#define HERBST_IPC_CLASS "HERBST_IPC_CLASS"
//#define HERBST_IPC_READY "HERBST_IPC_READY"
//#define HERBST_IPC_ATOM  "_HERBST_IPC"
#define HERBST_IPC_ARGS_ATOM "_HERBST_IPC_ARGS"
//! the (utf8 text) atom containing the output channel
#define HERBST_IPC_OUTPUT_ATOM "_HERBST_IPC_OUTPUT"
//! the (utf8 text) atom containing the error channel
#define HERBST_IPC_ERROR_ATOM "_HERBST_IPC_ERROR"
#define HERBST_IPC_STATUS_ATOM "_HERBST_IPC_EXIT_STATUS"

/** if this (integer) atom is present and non-empty on the hook window,
 *  then the ipc reply has an error channel.
 */
#define HERBST_IPC_HAS_ERROR "_HERBST_IPC_HAS_ERROR"

#define HERBST_HOOK_CLASS "HERBST_HOOK_CLASS"
#define HERBST_HOOK_WIN_ID_ATOM "__HERBST_HOOK_WIN_ID"
#define HERBST_HOOK_PROPERTY_FORMAT "__HERBST_HOOK_ARGUMENTS_%d"
// maximum number of hooks to buffer
#define HERBST_HOOK_PROPERTY_COUNT 10

// function exit codes
enum {
    HERBST_EXIT_SUCCESS = 0,
    HERBST_UNKNOWN_ERROR,
    HERBST_COMMAND_NOT_FOUND,
    HERBST_INVALID_ARGUMENT,
    HERBST_SETTING_NOT_FOUND,
    HERBST_TAG_IN_USE,
    HERBST_FORBIDDEN,
    HERBST_NO_PARAMETER_EXPECTED,
    HERBST_ENV_UNSET,
    HERBST_NEED_MORE_ARGS,
};

#endif

