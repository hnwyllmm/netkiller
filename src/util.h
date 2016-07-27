#ifndef _NETKILLER_UTIL_H
#define _NETKILLER_UTIL_H

#include <wdm.h>

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

// bool TxGetImageName(PCHAR *ppImageName);
BOOLEAN InSafeMode();
BOOLEAN IsMyDevice(IN PDEVICE_OBJECT DeviceObject);
NTSTATUS CallAttachedDevice(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp);

VOID DriverUnload(IN PDRIVER_OBJECT DriverObject);

NTSTATUS CreateDevice(IN PDRIVER_OBJECT DriverObject);
NTSTATUS InitTdiDevice(IN PDRIVER_OBJECT DriverObject);

#endif  // _NETKILLER_UTIL_H