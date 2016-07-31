/************************************************************************/
/* irp dispatch routines for network devices                            */
/************************************************************************/

#ifndef _NETKILLER_NET_IRP_H
#define _NETKILLER_NET_IRP_H

NTSTATUS IrpNetCreate(IN PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS IrpNetInternalDeviceControl(IN PDEVICE_OBJECT DeviceObject, IN OUT PIRP Irp);


#endif  // _NETKILLER_NET_IRP_H