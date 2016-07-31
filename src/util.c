#include <ntifs.h>
#include "util.h"
#include "process.h"

#define NK_MEMORY_TAG		'netk'

#define NK_DEVICE_NAME		L"\\Device\\NetKiller"
#define NK_SYMBOLIC_LINK	L"\\DosDevices\\NetKiller"

#define NK_DEVICE_TCP		L"\\Device\\Tcp"
#define NK_DEVICE_UDP		L"\\Device\\Udp"
#define NK_DEVICE_RAWIP		L"\\Device\\RawIp"

PDEVICE_OBJECT g_objNetKiller;
PDEVICE_OBJECT g_objDeviceTcp;
PDEVICE_OBJECT g_objDeviceUdp;
PDEVICE_OBJECT g_objDeviceRawIp;

USHORT  NkNtohs(USHORT val)
{
	USHORT ret;
	PCHAR tmp = &ret;
	PCHAR tmp1 = &val;

	INT32 test = 0x12345678;
	if (0x12 == *(CHAR *)(&test))
		return val;
	
	tmp[0] = tmp1[1];
	tmp[1] = tmp1[0];
	return ret;
}
BOOLEAN InSafeMode()
{
	//return InitSafeBootMode > 0;
	return FALSE;
}

PVOID	AllocMemory(SIZE_T NumberOfBytes)
{
	PVOID Address = ExAllocatePoolWithTag(NonPagedPool, NumberOfBytes, NK_MEMORY_TAG);
	if (Address)
		RtlZeroMemory(Address, NumberOfBytes);
	return Address;
}

VOID FreeMemory(PVOID Address)
{
	if (Address)
		ExFreePoolWithTag(Address, NK_MEMORY_TAG);
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

VOID UninitInitProcessNotify()
{
	PsSetCreateProcessNotifyRoutine(CreateProcessNotifyRoutine, TRUE);
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

	UninitInitProcessNotify();
	UninitTdiDevice(DriverObject);
	UninitDevice();
}

NTSTATUS InitProcessNotify()
{
	return PsSetCreateProcessNotifyRoutine(CreateProcessNotifyRoutine, FALSE);
}

NTSTATUS ZwQueryInformationProcess(
	__in      HANDLE           ProcessHandle,
	__in      PROCESSINFOCLASS ProcessInformationClass,
	__out_bcount(ProcessInformationLength)     PVOID            ProcessInformation,
	__in      ULONG            ProcessInformationLength,
	__out_opt PULONG           ReturnLength
	);

#if 0
NTSTATUS GetProcessImagePath(IN HANDLE Pid, IN OUT PUNICODE_STRING ImagePath)
{
	NTSTATUS					status;
	KIRQL						Irql;
	ULONG						ReturnLen;
	PUNICODE_STRING				ProcessInfo;

	PAGED_CODE();
	Irql = KeGetCurrentIrql();
	if (Irql != PASSIVE_LEVEL)
		return STATUS_UNSUCCESSFUL;

	status = ZwQueryInformationProcess(
		Pid,
		ProcessImageFileName,
		NULL,
		0,
		&ReturnLen
		);
	if (status != STATUS_INFO_LENGTH_MISMATCH)
		return status;

	if (ReturnLen - sizeof(UNICODE_STRING) > ImagePath->MaximumLength)
		return STATUS_INFO_LENGTH_MISMATCH;

	ProcessInfo = AllocMemory(ReturnLen);
	if (!ProcessInfo)
		return STATUS_NO_MEMORY;

	status = ZwQueryInformationProcess(
		Pid,
		ProcessImageFileName,
		ProcessInfo,
		ReturnLen,
		&ReturnLen
		);
	if (NT_SUCCESS(status))
	{
		RtlCopyMemory(ImagePath->Buffer, ProcessInfo->Buffer, ProcessInfo->Length);
		ImagePath->Length = ProcessInfo->Length;
	}

	FreeMemory(ProcessInfo);
	
	return status;
}
#endif

NTSTATUS GetProcessImagePath(IN HANDLE Pid, IN OUT PUNICODE_STRING ImagePath)
{
	NTSTATUS					status;
	KIRQL						Irql;
	PEPROCESS					Eprocess = NULL;
	HANDLE						ProcessHandle = NULL;
	OBJECT_ATTRIBUTES			FileAttr;
	PUNICODE_STRING						ProcessInformation = NULL;
	SIZE_T						ProcessInformationSize = 4096;
	SIZE_T						ProcessInfoSizeReturn = 0;
	HANDLE						FileHandle = NULL;
	IO_STATUS_BLOCK				IoStatus;
	PVOID						FileObject = NULL;
	POBJECT_NAME_INFORMATION	ObjectNameInfo = NULL;

	ImagePath->Length = 0;

	Irql = KeGetCurrentIrql();
	if (Irql != PASSIVE_LEVEL)
		return STATUS_UNSUCCESSFUL;

	__try
	{
		status = PsLookupProcessByProcessId(Pid, &Eprocess);
		if (!NT_SUCCESS(status))
			return status;

		DbgPrint("PsLookupProcessByProcessId\n");

		status = ObOpenObjectByPointer(
			Eprocess,
			OBJ_KERNEL_HANDLE,
			NULL,
			STANDARD_RIGHTS_ALL | SPECIFIC_RIGHTS_ALL,
			NULL,
			KernelMode,
			&ProcessHandle
			);
		if (!NT_SUCCESS(status))
			return status;

		DbgPrint("ObOpenObjectByPointer\n");
		ProcessInformation = AllocMemory(ProcessInformationSize);
		if (!ProcessInformation)
			return STATUS_NO_MEMORY;

		status = ZwQueryInformationProcess(
			ProcessHandle,
			//ProcessWx86Information|ProcessExceptionPort, // ProcessImageFileName可能也可以
			ProcessImageFileName,
			ProcessInformation,
			ProcessInformationSize,
			&ProcessInfoSizeReturn
			);
		if (!NT_SUCCESS(status))
			return status;

		DbgPrint("ZwQueryInformationProcess\n");

		/*
		if (ProcessInformation->Length > ImagePath->MaximumLength)
			return STATUS_INFO_LENGTH_MISMATCH;

		RtlCopyMemory(ImagePath->Buffer, ProcessInformation->Buffer, ProcessInformation->Length);
		ImagePath->Length = ProcessInformation->Length;
		FreeMemory(ProcessInformation);
		ProcessInformation = NULL;
		*/
		
		InitializeObjectAttributes(
			&FileAttr,
			(PUNICODE_STRING)ProcessInformation,
			OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
			NULL,
			NULL
			);

		status = ZwOpenFile(
			&FileHandle, 
			GENERIC_READ, 
			&FileAttr, 
			&IoStatus,
			FILE_SHARE_READ, 
			FILE_SYNCHRONOUS_IO_NONALERT
			);
		if (!NT_SUCCESS(status))
			return status;

		DbgPrint("ZwOpenFile\n");
		status = ObReferenceObjectByHandle(
			FileHandle,
			0,
			*IoFileObjectType,
			KernelMode,
			&FileObject,
			NULL
			);
		if (!NT_SUCCESS(status))
			return status;

		DbgPrint("ObReferenceObjectByHandle\n");
		status = IoQueryFileDosDeviceName(FileObject, &ObjectNameInfo);
		if (!NT_SUCCESS(status))
			return status;
		
		DbgPrint("IoQueryFileDosDeviceName\n");
		if (ObjectNameInfo->Name.Length <= ImagePath->MaximumLength)
			RtlCopyMemory(ImagePath->Buffer, ObjectNameInfo->Name.Buffer, ObjectNameInfo->Name.Length);
		else
			status = STATUS_BUFFER_TOO_SMALL;
		ExFreePool(ObjectNameInfo);
		ObjectNameInfo = NULL;
		

		return status;
	}
	__finally
	{
		if (FileObject)
			ObfDereferenceObject(FileObject);

		if (FileHandle)
			ZwClose(FileHandle);

		if (ProcessInformation)
			FreeMemory(ProcessInformation);

		if (ProcessHandle)
			ZwClose(ProcessHandle);

		if (Eprocess)
			ObfDereferenceObject(Eprocess);
	}
}

