#ifndef _NETKILLER_UTIL_H
#define _NETKILLER_UTIL_H

#include <wdm.h>
#include <tdikrnl.h>

#ifndef PATH_MAX
#define PATH_MAX		300
#endif // PATH_MAX

#define NK_DRIVER_NAME "netkiller"

extern PDEVICE_OBJECT g_objNetKiller;
extern PDEVICE_OBJECT g_objDeviceTcp;
extern PDEVICE_OBJECT g_objDeviceUdp;
extern PDEVICE_OBJECT g_objDeviceRawIp;

typedef struct _NK_DEVICE_EXTENSION
{
	PDEVICE_OBJECT		DeviceObject;
	PDEVICE_OBJECT		AttachedDevice;
	PFILE_OBJECT		FileObject;
}NK_DEVICE_EXTENSION, *PNK_DEVICE_EXTENSION;

/**
 * 镜像文件相关的流量限速信息
 */
typedef struct _NK_IMAGEPATH_NETFLOW_LIMIT_INFORMATION
{
	BOOLEAN				DisableSend;
	BOOLEAN				DisableRecv;
	INT64				SpeedSend;		// INT64_MAX: no limit
	INT64				SpeedRecv;
	WCHAR				ImagePath[PATH_MAX];
}NK_IMAGEPATH_NETFLOW_LIMIT_INFORMATION, *PNK_IMAGEPATH_NETFLOW_LIMIT_INFORMATION;

typedef struct _NK_IMAGEPATH_NETFLOW_LIMIT_INFORMATION_NODE
{
	LIST_ENTRY			ListEntry;
	NK_IMAGEPATH_NETFLOW_LIMIT_INFORMATION	LimitInfo;
};

USHORT  NkNtohs(USHORT val);
// bool TxGetImageName(PCHAR *ppImageName);
BOOLEAN InSafeMode();
PVOID	AllocMemory(SIZE_T NumberOfBytes);
VOID	FreeMemory(PVOID Address);
BOOLEAN IsMyDevice(IN PDEVICE_OBJECT DeviceObject);

NTSTATUS CallAttachedDevice(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

VOID DriverUnload(IN PDRIVER_OBJECT DriverObject);

NTSTATUS CreateDevice(IN PDRIVER_OBJECT DriverObject);
NTSTATUS InitTdiDevice(IN PDRIVER_OBJECT DriverObject);

NTSTATUS InitProcessNotify();
NTSTATUS GetProcessImagePath(IN HANDLE Pid, IN OUT PUNICODE_STRING ImagePath);

#endif  // _NETKILLER_UTIL_H