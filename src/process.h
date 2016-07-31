#ifndef _NETKILLER_PROCESS_H
#define _NETKILLER_PROCESS_H

#include "util.h"

/**
 * PsSetCreateProcessNotifyRoutine 注册的回调函数
 */
VOID CreateProcessNotifyRoutine(
		IN HANDLE ParentId,
		IN HANDLE ProcessId,
		IN BOOLEAN Create
		);

#endif	// _NETKILLER_PROCESS_H