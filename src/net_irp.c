#include <ntddk.h>
#include "util.h"

///////////////////////////////////////////////////////////////////////////////

#define NK_PACKAGE_DIRECTION_SEND	1
#define NK_PACKAGE_DIRECTION_RECV	0

typedef struct _NK_IRP_COMPLETE_CONTEXT
{
	KEVENT		Event;
	NTSTATUS	Status;
}NK_IRP_COMPLETE_CONTEXT, *PNK_IRP_COMPLETE_CONTEXT;

typedef struct _NK_TDI_RECV_COMPLETE_CONTEXT
{
	UCHAR					Direction;
	BOOLEAN					NeedCallBack;		// 是否需要调用回调函数
	HANDLE					Pid;
	PIO_COMPLETION_ROUTINE	CompleteRoutine;	// IO_STACK_LOCATION.CompletionRoutine
	ULONG					Control;			// IO_STACK_LOCATION.Control
	PVOID					Context;			// IO_STACK_LOCATION.Context
	PFILE_OBJECT			FileObject;			// IO_STACK_LOCATION.FileObject
}NK_TDI_RECV_COMPLETE_CONTEXT, *PNK_TDI_RECV_COMPLETE_CONTEXT;

///////////////////////////////////////////////////////////////////////////////
/**
 * TDI MJ_CREATE的完成函数
 */
NTSTATUS TransportAddrCompleteRoutine(
			  IN PDEVICE_OBJECT DeviceObject, 
			  IN PIRP Irp, 
			  IN PVOID Context
			  )
{
	PNK_IRP_COMPLETE_CONTEXT IrpContext = (PNK_IRP_COMPLETE_CONTEXT)Context;
	IrpContext->Status = Irp->IoStatus.Status;
	KeSetEvent(&IrpContext->Event, 0, FALSE);
	if (Irp->PendingReturned)
		IoMarkIrpPending(Irp);
	return STATUS_SUCCESS;
}

/**
* TDI 通用的IRP完成函数
*/
NTSTATUS IrpCompleteRoutine(
			IN PDEVICE_OBJECT DeviceObject,
			IN PIRP Irp,
			IN PVOID Context
			)
{
	if (Context)
		KeSetEvent((PKEVENT)Context, 0, FALSE);
	return STATUS_MORE_PROCESSING_REQUIRED;
}

/**
* TDI 在发送或者接收函数完成时调用的回调函数
*/
NTSTATUS TdiSendReceiveCompleteRoutine(
		   IN PDEVICE_OBJECT DeviceObject,
		   IN PIRP Irp,
		   PVOID Context
		   )
{
	NTSTATUS		status = STATUS_SUCCESS;
	PNK_TDI_RECV_COMPLETE_CONTEXT TdiContext = (PNK_TDI_RECV_COMPLETE_CONTEXT)Context;

	if (NT_SUCCESS(Irp->IoStatus.Status))
	{
		if (NK_PACKAGE_DIRECTION_SEND == TdiContext->Direction)
		{
			DbgPrint(NK_DRIVER_NAME ": send bytes: %d\n", Irp->IoStatus.Information);
		}
		else
		{
			DbgPrint(NK_DRIVER_NAME ": recv bytes: %d\n", Irp->IoStatus.Information);
		}
	}

	if (TdiContext->NeedCallBack &&
		TdiContext->CompleteRoutine &&
		(
			(NT_SUCCESS(Irp->IoStatus.Status) && (TdiContext->Control & 0x40)) ||
			(!NT_SUCCESS(Irp->IoStatus.Status) && (TdiContext->Control & 0x80)) ||
			(Irp->Cancel && (TdiContext->Control & 0x20))
		)
	   )
	{
		status = (*TdiContext->CompleteRoutine)(DeviceObject, Irp, TdiContext->Context);
	}

	if (status != STATUS_MORE_PROCESSING_REQUIRED && Irp->PendingReturned)
		IoMarkIrpPending(Irp);

	if (TdiContext)
		FreeMemory(TdiContext);

	return status != STATUS_MORE_PROCESSING_REQUIRED ? STATUS_SUCCESS : STATUS_MORE_PROCESSING_REQUIRED;
}

/**
* TDI 从TDI地址信息中提取IP地址和端口号
*/
VOID GetAddressInfoFromTdiAddressInfo(
			IN PTDI_ADDRESS_INFO AddrInfo, 
			OUT PULONG Addr, 
			OUT PULONG Port
			)
{
	PTDI_ADDRESS_IP IpAddr = (PTDI_ADDRESS_IP)AddrInfo->Address.Address[0].Address;
	*Addr = IpAddr->in_addr;
	*Port = NkNtohs(IpAddr->sin_port);
}

/**
* TDI 向TDI设备发送消息获取IP地址和端口号
*/
NTSTATUS GetAddressInfo(
			PDEVICE_OBJECT DeviceObject, 
			PFILE_OBJECT FileObject, 
			PULONG Addr, 
			PULONG Port
			)
{
	SIZE_T						IrpMemorySize = 4096;
	PVOID						MdlMemory;
	KEVENT						Event;
	PMDL						Mdl;
	PIRP						Irp;
	PIO_STACK_LOCATION			IrpSp;
	NTSTATUS					status = STATUS_SUCCESS;
	
	__try 
	{
		KeInitializeEvent(&Event, NotificationEvent, FALSE);

		MdlMemory = AllocMemory(IrpMemorySize);
		if (!MdlMemory)
		{
			status = STATUS_NO_MEMORY;
			goto out_exit;
		}

		Mdl = IoAllocateMdl(MdlMemory, IrpMemorySize, FALSE, FALSE, NULL);
		if (!Mdl)
		{
			status = STATUS_NO_MEMORY;
			goto out_exit;
		}

		MmBuildMdlForNonPagedPool(Mdl);
		Irp = IoBuildDeviceIoControlRequest(TDI_CONNECT, DeviceObject, NULL, 0, NULL, 0, TRUE, NULL, NULL);
		if (!Irp)
		{
			status = STATUS_NO_MEMORY;
			goto out_exit;
		}

		Irp->MdlAddress = Mdl;
		TdiBuildQueryInformation(
			Irp, 
			DeviceObject, 
			FileObject, 
			IrpCompleteRoutine, 
			&Event, 
			TDI_QUERY_ADDRESS_INFO, 
			Mdl
			);

		status = IoCallDriver(DeviceObject, Irp);
		if (STATUS_PENDING == status)
			KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);

		if (NT_SUCCESS(status))
		{
			GetAddressInfoFromTdiAddressInfo((PTDI_ADDRESS_INFO)MdlMemory, Addr, Port);
			DbgPrint(NK_DRIVER_NAME ": ip %x, port %d\n", *Addr, *Port);
		}
	}
	__finally
	{
		if (Mdl)
			IoFreeMdl(Mdl);

		if (MdlMemory)
			FreeMemory(MdlMemory);
	}
out_exit:
	return status;
}

NTSTATUS TdiOnConnect(IN PDEVICE_OBJECT DeviceObject, IN OUT PIRP Irp)
{
	PIO_STACK_LOCATION			IrpSp;
	PTDI_REQUEST_KERNEL_CONNECT	TdiRequest;
	PTDI_ADDRESS_IP				IpAddr;
	ULONG						Ip, Port;

	IrpSp = IoGetCurrentIrpStackLocation(Irp);
	if (!IrpSp || Irp->CurrentLocation <= 1)
		return CallAttachedDevice(DeviceObject, Irp);

	TdiRequest = (PTDI_REQUEST_KERNEL_CONNECT)(&IrpSp->Parameters);
	if (TdiRequest && TdiRequest->RequestConnectionInformation->RemoteAddressLength > 0)
	{
		PTA_ADDRESS TaAddress = ((PTRANSPORT_ADDRESS)(TdiRequest->RequestConnectionInformation->RemoteAddress))->Address;
		IpAddr = (PTDI_ADDRESS_IP)TaAddress->Address;
		Ip = IpAddr->in_addr;
		Port = NkNtohs(IpAddr->sin_port);
		DbgPrint(NK_DRIVER_NAME ": on connect: %x %d\n", Ip, Port);
	}

	return CallAttachedDevice(DeviceObject, Irp);
}

NTSTATUS TdiOnAssociateAddress(IN PDEVICE_OBJECT DeviceObject, IN OUT PIRP Irp)
{
	return CallAttachedDevice(DeviceObject, Irp);
}

NTSTATUS TdiOnDisassociateAddress(IN PDEVICE_OBJECT DeviceObject, IN OUT PIRP Irp)
{
	return CallAttachedDevice(DeviceObject, Irp);
}

NTSTATUS TdiOnSetEventHandler(IN PDEVICE_OBJECT DeviceObject, IN OUT PIRP Irp)
{
	return CallAttachedDevice(DeviceObject, Irp);
}

NTSTATUS TdiOnReceive(IN PDEVICE_OBJECT DeviceObject, IN OUT PIRP Irp)
{
	NTSTATUS					status;
	PIO_STACK_LOCATION			IrpSp;
	PNK_DEVICE_EXTENSION		DeviceExt;

	PNK_TDI_RECV_COMPLETE_CONTEXT TdiRecvContext;
	TdiRecvContext = (PNK_TDI_RECV_COMPLETE_CONTEXT)AllocMemory(sizeof(*TdiRecvContext));
	if (!TdiRecvContext)
	{
		return CallAttachedDevice(DeviceObject, Irp);
	}

	IrpSp = IoGetCurrentIrpStackLocation(Irp);
	TdiRecvContext->FileObject = IrpSp->FileObject;
	TdiRecvContext->Direction = NK_PACKAGE_DIRECTION_RECV;
	TdiRecvContext->Pid = 0;

	if (Irp->CurrentLocation > 1)
	{
		TdiRecvContext->NeedCallBack = FALSE;
		IoCopyCurrentIrpStackLocationToNext(Irp);
		IoSetCompletionRoutine(Irp, TdiSendReceiveCompleteRoutine, TdiRecvContext, TRUE, TRUE, TRUE);
		DeviceExt = (PNK_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
		status = IoCallDriver(DeviceExt->AttachedDevice, Irp);
	}
	else
	{
		TdiRecvContext->NeedCallBack = TRUE;
		TdiRecvContext->CompleteRoutine = IrpSp->CompletionRoutine;
		TdiRecvContext->Context = IrpSp->Context;
		TdiRecvContext->Control = IrpSp->Control;

		IrpSp->CompletionRoutine = TdiSendReceiveCompleteRoutine;
		IrpSp->Context = TdiRecvContext;
		IrpSp->Control = SL_INVOKE_ON_CANCEL | SL_INVOKE_ON_SUCCESS | SL_INVOKE_ON_ERROR;
		return CallAttachedDevice(DeviceObject, Irp);
	}
	return status;
}

NTSTATUS TdiOnReceiveDatagram(IN PDEVICE_OBJECT DeviceObject, IN OUT PIRP Irp)
{
	return TdiOnReceive(DeviceObject, Irp);
}

NTSTATUS TdiOnSend(IN PDEVICE_OBJECT DeviceObject, IN OUT PIRP Irp)
{
	NTSTATUS					status;
	PFILE_OBJECT				FileObject;
	PIO_STACK_LOCATION			IrpSp;
	PTDI_REQUEST_KERNEL_SENDDG	TdiRequest;
	ULONG						SendLength;

	IrpSp = IoGetCurrentIrpStackLocation(Irp);
	FileObject = IrpSp->FileObject;
	TdiRequest = (PTDI_REQUEST_KERNEL_SENDDG)(&IrpSp->Parameters);
	SendLength = TdiRequest->SendLength;
	DbgPrint(NK_DRIVER_NAME ": TdiOnSend: %u\n", SendLength);
	return CallAttachedDevice(DeviceObject, Irp);
}

NTSTATUS TdiOnSendDatagram(IN PDEVICE_OBJECT DeviceObject, IN OUT PIRP Irp)
{
	NTSTATUS					status;
	PFILE_OBJECT				FileObject;
	PIO_STACK_LOCATION			IrpSp;
	PTDI_REQUEST_KERNEL_SENDDG	TdiRequest;
	ULONG						SendLength;

	IrpSp = IoGetCurrentIrpStackLocation(Irp);
	FileObject = IrpSp->FileObject;
	TdiRequest = (PTDI_REQUEST_KERNEL_SENDDG)(&IrpSp->Parameters);
	SendLength = TdiRequest->SendLength;
	DbgPrint(NK_DRIVER_NAME ": TdiOnSendDatagram: %u\n", SendLength);
	return CallAttachedDevice(DeviceObject, Irp);
}
///////////////////////////////////////////////////////////////////////////////
NTSTATUS IrpNetCreate(IN PDEVICE_OBJECT DeviceObject, IN OUT PIRP Irp)
{
	NTSTATUS status =			STATUS_SUCCESS;
	PIO_STACK_LOCATION IrpSp =	IoGetCurrentIrpStackLocation(Irp);
	PFILE_OBJECT				FileObject;
	PEPROCESS					Process;
	PFILE_FULL_EA_INFORMATION	EaInfo;
	CONNECTION_CONTEXT			ConnCtx;
	NK_IRP_COMPLETE_CONTEXT		IrpContext;
	PNK_DEVICE_EXTENSION		DeviceExt;
	ULONG						Ip, Port;
	HANDLE						Pid;
	
	if (Irp->CurrentLocation <= 1)
	{
		return CallAttachedDevice(DeviceObject, Irp);
	}
	
	FileObject = IrpSp->FileObject;
	Process = PsGetCurrentProcess();
	Pid = PsGetProcessId(Process);
	EaInfo = (PFILE_FULL_EA_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
	if (EaInfo)
	{
		DbgPrint(NK_DRIVER_NAME ": ealen %d\n", EaInfo->EaNameLength);
	}
	else
	{
		DbgPrint(NK_DRIVER_NAME ": eainfo is empty\n");
	}
	if (EaInfo && EaInfo->EaNameLength == TDI_TRANSPORT_ADDRESS_LENGTH && 
		TDI_TRANSPORT_ADDRESS_LENGTH == RtlCompareMemory(EaInfo->EaName, TdiTransportAddress, TDI_TRANSPORT_ADDRESS_LENGTH)
		)
	{
		IoCopyCurrentIrpStackLocationToNext(Irp);
		KeInitializeEvent(&IrpContext.Event, NotificationEvent, FALSE);
		IoSetCompletionRoutine(Irp, TransportAddrCompleteRoutine, &IrpContext, TRUE, TRUE, TRUE);
		DeviceExt = (PNK_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
		status = IoCallDriver(DeviceExt->AttachedDevice, Irp);
		if (STATUS_PENDING == status)
		{
			KeWaitForSingleObject(&IrpContext.Event, Executive, KernelMode, FALSE, NULL);
		}

		if (!NT_SUCCESS(IrpContext.Status))
			return IrpContext.Status;

		//IoCompleteRequest(Irp, IO_NO_INCREMENT);
		DbgPrint(NK_DRIVER_NAME ": tdi create: get address\n");

		//Process = 
		GetAddressInfo(FileObject->DeviceObject, FileObject, &Ip, &Port);
		
		DbgPrint(NK_DRIVER_NAME ": pid %d\n", Pid);
	}
	else if (EaInfo && EaInfo->EaNameLength == TDI_CONNECTION_CONTEXT_LENGTH && 
		TDI_CONNECTION_CONTEXT_LENGTH == RtlCompareMemory(EaInfo->EaName, TdiConnectionContext, TDI_CONNECTION_CONTEXT_LENGTH)
		)
	{
		IoCopyCurrentIrpStackLocationToNext(Irp);
		KeInitializeEvent(&IrpContext.Event, NotificationEvent, FALSE);
		IoSetCompletionRoutine(Irp, TransportAddrCompleteRoutine, &IrpContext, TRUE, TRUE, TRUE);
		DeviceExt = (PNK_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
		status = IoCallDriver(DeviceExt->AttachedDevice, Irp);
		if (STATUS_PENDING == status)
		{
			KeWaitForSingleObject(&IrpContext.Event, Executive, KernelMode, FALSE, NULL);
		}

		ConnCtx = *(CONNECTION_CONTEXT *)(EaInfo->EaName + EaInfo->EaNameLength + 1);
	}
	else
	{
		return CallAttachedDevice(DeviceObject, Irp);
	}

	return status;
	
}

NTSTATUS IrpNetInternalDeviceControl(IN PDEVICE_OBJECT DeviceObject, IN OUT PIRP Irp)
{
	NTSTATUS status;
	switch (IoGetCurrentIrpStackLocation(Irp)->MinorFunction)
	{
	default:
		status = CallAttachedDevice(DeviceObject, Irp);
		break;

	case TDI_CONNECT:
		status = TdiOnConnect(DeviceObject, Irp);
		break;

	case TDI_ASSOCIATE_ADDRESS:
		status = TdiOnAssociateAddress(DeviceObject, Irp);
		break;
	case TDI_DISASSOCIATE_ADDRESS:
		status = TdiOnDisassociateAddress(DeviceObject, Irp);
		break;
	case TDI_SET_EVENT_HANDLER:
		status = TdiOnSetEventHandler(DeviceObject, Irp);
		break;

	case TDI_RECEIVE:
		status = TdiOnReceive(DeviceObject, Irp);
		break;
	case TDI_RECEIVE_DATAGRAM:
		status = TdiOnReceiveDatagram(DeviceObject, Irp);
		break;
	case TDI_SEND:
		status = TdiOnSend(DeviceObject, Irp);
		break;
	case TDI_SEND_DATAGRAM:
		status = TdiOnSendDatagram(DeviceObject, Irp);
		break;
	}
	return status;
}