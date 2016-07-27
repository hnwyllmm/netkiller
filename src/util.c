#include "util.h"

#define NK_DEVICE_NAME		L"\\Device\\NetKiller"
#define NK_SYMBOLIC_LINK	L"\\DosDevices\\NetKiller"

#define NK_DEVICE_TCP		L"\\Device\\Tcp"
#define NK_DEVICE_UDP		L"\\Device\\Udp"
#define NK_DEVICE_RAWIP		L"\\Device\\RawIp"

PDEVICE_OBJECT g_objNetKiller;
PDEVICE_OBJECT g_objDeviceTcp;
PDEVICE_OBJECT g_objDeviceUdp;
PDEVICE_OBJECT g_objDeviceRawIp;

BOOLEAN InSafeMode()
{
	//return InitSafeBootMode > 0;
	return FALSE;
}

BOOLEAN IsMyDevice(IN PDEVICE_OBJECT DeviceObject)
{
	return DeviceObject == g_objNetKiller ||
		   DeviceObject == g_objDeviceTcp ||
		   DeviceObject == g_objDeviceUdp ||
		   DeviceObject == g_objDeviceRawIp;
}

NTSTATUS CallAttachedDevice(IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp)
{
	PNK_DEVICE_EXTENSION DeviceExt = DeviceObject->DeviceExtension;
	if (!DeviceExt || !DeviceExt->AttachedDevice)
	{
		Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_UNSUCCESSFUL;
	}
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(DeviceExt->AttachedDevice, Irp);
}

///////////////////////////////////////////////////////////////////////////////
// internal routines

NTSTATUS CreateDeviceAndSymbolicLink(IN PDRIVER_OBJECT DriverObject, 
									 IN PCWSTR DeviceName, 
									 IN PCWSTR SymbolicLink, 
									 OUT PDEVICE_OBJECT *DeviceObject
									 )
{
	NTSTATUS status;
	UNICODE_STRING DeviceNameString;
	UNICODE_STRING SymbolicLinkString;
	PNK_DEVICE_EXTENSION DeviceExt;

	RtlInitUnicodeString(&DeviceNameString, DeviceName);
	status = IoCreateDevice(DriverObject, 
		sizeof(NK_DEVICE_EXTENSION), 
		&DeviceNameString, 
		FILE_DEVICE_UNKNOWN, 
		0,
		FALSE, 
		DeviceObject);
	if (!NT_SUCCESS(status))
	{
		DbgPrint(NK_DRIVER_NAME ": create device failure: %ws\n", DeviceName);
		return status;
	}

	DeviceExt = (PNK_DEVICE_EXTENSION)(*DeviceObject)->DeviceExtension;
	RtlZeroMemory(DeviceExt, sizeof(*DeviceExt));

	RtlInitUnicodeString(&SymbolicLinkString, SymbolicLink);
	status = IoCreateSymbolicLink(&SymbolicLinkString, &DeviceNameString);
	if (!NT_SUCCESS(status))
	{
		DbgPrint(NK_DRIVER_NAME ": create symbolic link failure: %ws\n", SymbolicLink);
		IoDeleteDevice(*DeviceObject);
		*DeviceObject = NULL;
		return status;
	}
	return status;
}

NTSTATUS AttachDevice(IN PDRIVER_OBJECT DriverObject,
					  IN PCWSTR TargetDevice,
					  OUT PDEVICE_OBJECT *DeviceObject
					  )
{
	NTSTATUS				status = STATUS_SUCCESS;
	UNICODE_STRING			TargetDeviceString;
	PNK_DEVICE_EXTENSION	DeviceExt;

	if (!DriverObject || !TargetDevice || !DeviceObject)
		return STATUS_INVALID_PARAMETER;

	RtlInitUnicodeString(&TargetDeviceString, TargetDevice);

	status = IoCreateDevice(DriverObject, 
		sizeof(NK_DEVICE_EXTENSION),
		NULL,
		FILE_DEVICE_UNKNOWN,
		0,
		FALSE,
		DeviceObject);
	if (!NT_SUCCESS(status))
	{
		DbgPrint(NK_DRIVER_NAME ": create device failure %ws\n", TargetDevice);
		return status;
	}

	(*DeviceObject)->Flags |= DO_DIRECT_IO;
//	(*DeviceObject)->Flags &= ~DO_DEVICE_INITIALIZING;
//	(*DeviceObject)->Flags |= DeviceNextStack->Flags & (DO_DIRECT_IO | DO_BUFFERED_IO);

	DeviceExt = (PNK_DEVICE_EXTENSION)(*DeviceObject)->DeviceExtension;
	RtlZeroMemory(DeviceExt, sizeof(*DeviceExt));

	DeviceExt->DeviceObject = *DeviceObject;
	status = IoAttachDevice(
		*DeviceObject,
		&TargetDeviceString,
		&DeviceExt->AttachedDevice
		);
	if (!NT_SUCCESS(status))
	{
		IoDeleteDevice(*DeviceObject);
		*DeviceObject = NULL;
		DbgPrint(NK_DRIVER_NAME ": attach device failure: %ws\n", TargetDevice);
	}

	return status;
}

VOID UninitTdiDevice(PDRIVER_OBJECT DriverObject)
{
	PNK_DEVICE_EXTENSION DeviceExt;

	DbgPrint(NK_DRIVER_NAME ": uninit tdi device\n");

	if (g_objDeviceTcp)
	{
		DeviceExt = (PNK_DEVICE_EXTENSION)g_objDeviceTcp->DeviceExtension;
		if (DeviceExt)
		{
			if (DeviceExt->AttachedDevice)
				IoDetachDevice(DeviceExt->AttachedDevice);

			if (DeviceExt->FileObject)
				ObDereferenceObject(DeviceExt->FileObject);
		}
		IoDeleteDevice(g_objDeviceTcp);
		g_objDeviceTcp = NULL;
	}

	if (g_objDeviceUdp)
	{
		DeviceExt = (PNK_DEVICE_EXTENSION)g_objDeviceUdp->DeviceExtension;
		if (DeviceExt)
		{
			if (DeviceExt->AttachedDevice)
				IoDetachDevice(DeviceExt->AttachedDevice);

			if (DeviceExt->FileObject)
				ObDereferenceObject(DeviceExt->FileObject);
		}
		IoDeleteDevice(g_objDeviceUdp);
		g_objDeviceUdp = NULL;
	}

	if (g_objDeviceRawIp)
	{
		DeviceExt = (PNK_DEVICE_EXTENSION)g_objDeviceRawIp->DeviceExtension;
		if (DeviceExt)
		{
			if (DeviceExt->AttachedDevice)
				IoDetachDevice(DeviceExt->AttachedDevice);

			if (DeviceExt->FileObject)
				ObDereferenceObject(DeviceExt->FileObject);
		}
		IoDeleteDevice(g_objDeviceRawIp);
		g_objDeviceRawIp = NULL;
	}
}

VOID UninitDevice()
{
	UNICODE_STRING SymbolicLink;

	DbgPrint(NK_DRIVER_NAME ": uninit device\n");

	if (g_objNetKiller)
	{
		RtlInitUnicodeString(&SymbolicLink, NK_SYMBOLIC_LINK);
		IoDeleteSymbolicLink(&SymbolicLink);
		IoDeleteDevice(g_objNetKiller);
	}
}

///////////////////////////////////////////////////////////////////////////////
NTSTATUS CreateDevice(PDRIVER_OBJECT DriverObject)
{
	DbgPrint(NK_DRIVER_NAME ": create device and symbolic link\n");
	return CreateDeviceAndSymbolicLink(DriverObject, 
		NK_DEVICE_NAME,
		NK_SYMBOLIC_LINK,
		&g_objNetKiller
		);
}

NTSTATUS InitTdiDevice(PDRIVER_OBJECT DriverObject)
{
	NTSTATUS status = STATUS_SUCCESS;

	DbgPrint(NK_DRIVER_NAME ": init tdi device\n");

	status = AttachDevice(DriverObject, 
		NK_DEVICE_TCP,
		&g_objDeviceTcp);
	if (!NT_SUCCESS(status))
		return status;
/*
	status = AttachDevice(DriverObject, 
		NK_DEVICE_UDP,
		&g_objDeviceUdp);
	if (!NT_SUCCESS(status))
		return status;

	status = AttachDevice(DriverObject, 
		NK_DEVICE_RAWIP,
		&g_objDeviceRawIp);
	if (!NT_SUCCESS(status))
		return status;
*/
	DbgPrint(NK_DRIVER_NAME ": init tdi device success\n");
	return status;
}

VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
	DbgPrint(NK_DRIVER_NAME ": driver unload\n");

	UninitTdiDevice(DriverObject);
	UninitDevice();
}