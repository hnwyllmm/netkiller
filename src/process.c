#include "process.h"

VOID CreateProcessNotifyRoutine(
		IN HANDLE ParentId,
		IN HANDLE ProcessId,
		IN BOOLEAN Create
		)
{
	NTSTATUS status;
	WCHAR Buffer[PATH_MAX] = {0};
	UNICODE_STRING ImagePath;
	RtlInitEmptyUnicodeString(&ImagePath, Buffer, sizeof(Buffer));
	status = GetProcessImagePath(ProcessId, &ImagePath);
	if (!NT_SUCCESS(status))
	{
		DbgPrint(NK_DRIVER_NAME ": get image path error %x\n", status);
	}
	DbgPrint(NK_DRIVER_NAME ": process %s, pid %d: %ws\n",
		Create ? "created" : "exit", ProcessId, Buffer);
}