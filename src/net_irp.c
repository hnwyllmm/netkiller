#include "util.h"

NTSTATUS IrpNetCreate(IN PDEVICE_OBJECT DeviceObject, IN OUT PIRP Irp)
{
	return CallAttachedDevice(DeviceObject, Irp);
}