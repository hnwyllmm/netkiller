#ifndef _TAO_IRP_H
#define _TAO_IRP_H

NTSTATUS IrpPassThough(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS IrpDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS IrpInternalDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS IrpCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS IrpCleanup(PDEVICE_OBJECT DeviceObject, PIRP Irp);

#endif // _TAO_IRP_H