/************************************************************************/
/* irp dispatch routines for network devices                            */
/************************************************************************/

#ifndef _NETKILLER_NET_IRP_H
#define _NETKILLER_NET_IRP_H

NTSTATUS IrpNetCreate(IN PDEVICE_OBJECT DeviceObject, PIRP Irp);

#endif  // _NETKILLER_NET_IRP_H