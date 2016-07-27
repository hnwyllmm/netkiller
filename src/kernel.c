#include "util.h"
#include "irp.h"

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	NTSTATUS result = STATUS_SUCCESS;
	int i;

	DbgPrint(NK_DRIVER_NAME ": driver entry\n");

	if (InSafeMode())
		return STATUS_UNSUCCESSFUL;

	DriverObject->DriverUnload = DriverUnload;

	// set irp dispatch routines
	for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		DriverObject->MajorFunction[i] = IrpPassThough;
	}
	
	DriverObject->MajorFunction[IRP_MJ_CREATE]					= IrpCreate;
	DriverObject->MajorFunction[IRP_MJ_CLEANUP]					= IrpCleanup;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL]			= IrpDeviceControl;
	DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = IrpInternalDeviceControl;

	// create device and symbolic link
	result = CreateDevice(DriverObject);
	if (!NT_SUCCESS(result))
		goto drv_exit;

	// create network devices
	result = InitTdiDevice(DriverObject);
	if (!NT_SUCCESS(result))
		goto drv_exit;

drv_exit:
	if (!NT_SUCCESS(result))
		DriverUnload(DriverObject);

	DbgPrint(NK_DRIVER_NAME ": driver entry exit\n");
	return result;
}