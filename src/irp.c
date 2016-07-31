#include "util.h"
#include "net_irp.h"

NTSTATUS IrpPassThough(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	if (!IsMyDevice(DeviceObject))
	{
		Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_INVALID_PARAMETER;
	}

	if (DeviceObject != g_objNetKiller)
	{
		return CallAttachedDevice(DeviceObject, Irp);
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS IrpDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	if (!IsMyDevice(DeviceObject))
	{
		Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_INVALID_PARAMETER;
	}

	if (DeviceObject != g_objNetKiller)
	{
		return CallAttachedDevice(DeviceObject, Irp);
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}
NTSTATUS IrpInternalDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	if (!IsMyDevice(DeviceObject))
	{
		Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_INVALID_PARAMETER;
	}

	if (DeviceObject != g_objNetKiller)
	{
		return IrpNetInternalDeviceControl(DeviceObject, Irp);
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS IrpCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	DbgPrint(NK_DRIVER_NAME ": irp create\n");
	if (!IsMyDevice(DeviceObject))
	{
		Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_INVALID_PARAMETER;
	}

	if (DeviceObject != g_objNetKiller)
	{
		return IrpNetCreate(DeviceObject, Irp);
	}
	
	if (KeGetCurrentIrql() > PASSIVE_LEVEL)
	{
		Irp->IoStatus.Status = STATUS_ACCESS_DENIED;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_ACCESS_DENIED;
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS IrpCleanup(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	if (!IsMyDevice(DeviceObject))
	{
		Irp->IoStatus.Status = STATUS_INVALID_PARAMETER;
		Irp->IoStatus.Information = 0;

		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		return STATUS_INVALID_PARAMETER;
	}

	if (DeviceObject != g_objNetKiller)
	{
		return CallAttachedDevice(DeviceObject, Irp);	
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}