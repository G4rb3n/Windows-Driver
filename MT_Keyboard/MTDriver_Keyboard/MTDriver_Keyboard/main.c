#include <ntddk.h>
#include <ntddkbd.h>

// 设备扩展结构
typedef struct _Dev_exten
{
	ULONG Size;						// 该结构大小
	PDEVICE_OBJECT FilterDevice;	// 过滤设备对象
	PDEVICE_OBJECT TargeDevice;		// 下一设备对象
	PDEVICE_OBJECT LowDevice;		// 最底层设备对象
	KSPIN_LOCK IoRequestSpinLock;	// 自旋锁
	KEVENT IoInProgressEvent;		// 事件
	PIRP pIrp;						// IRP
} DEV_EXTENSION, *PDEV_EXTENSION;


// 声明微软未公开的ObReferenceObjectByName()函数
NTSTATUS ObReferenceObjectByName(
	PUNICODE_STRING ObjectName,
	ULONG Attributes,
	PACCESS_STATE AccessState,
	ACCESS_MASK DesiredAccess,
	POBJECT_TYPE ObjectType,
	KPROCESSOR_MODE AccessMode,
	PVOID ParseContest,
	PVOID *Object
);

extern POBJECT_TYPE *IoDriverObjectType;


//解除绑定
NTSTATUS DeAttach(PDEVICE_OBJECT pdevice)
{
	PDEV_EXTENSION devExt;
	devExt = (PDEV_EXTENSION)pdevice->DeviceExtension;

	IoDetachDevice(devExt->TargeDevice);
	devExt->TargeDevice = NULL;
	IoDeleteDevice(pdevice);
	devExt->FilterDevice = NULL;

	return STATUS_SUCCESS;
}


//设备卸载函数
NTSTATUS DriverUnload(PDRIVER_OBJECT pDriver)
{
	PDEVICE_OBJECT pDevice;
	PDEV_EXTENSION devExt;

	UNREFERENCED_PARAMETER(pDriver);
	DbgPrint("DriverEntry Unloading...\n");

	pDevice = pDriver->DeviceObject;
	while (pDevice)
	{
		DeAttach(pDevice);
		pDevice = pDevice->NextDevice;
	}

	pDriver->DeviceObject = NULL;

	return STATUS_SUCCESS;
}


// 设备操作通用分发函数
NTSTATUS GeneralDispatch(PDEVICE_OBJECT pDevice, PIRP pIrp)
{
	NTSTATUS status;

	DbgPrint("General Diapatch\n");
	PDEV_EXTENSION devExt = (PDEV_EXTENSION)pDevice->DeviceExtension;
	PDEVICE_OBJECT lowDevice = devExt->LowDevice;
	IoSkipCurrentIrpStackLocation(pIrp);
	status = IoCallDriver(lowDevice, pIrp);
	
	return status;
}


// IRP读操作的完成回调函数
NTSTATUS ReadComp(PDEVICE_OBJECT pDevice, PIRP pIrp, PVOID Context)
{
	NTSTATUS status;
	PIO_STACK_LOCATION stack;
	ULONG keyNumber;
	PKEYBOARD_INPUT_DATA myData;
	stack = IoGetCurrentIrpStackLocation(pIrp);
	if (NT_SUCCESS(pIrp->IoStatus.Status))
	{
		// 获取键盘数据
		myData = pIrp->AssociatedIrp.SystemBuffer;
		keyNumber = (ULONG)(pIrp->IoStatus.Information / sizeof(PKEYBOARD_INPUT_DATA));
		for (ULONG i = 0; i < keyNumber; i++)
		{
			DbgPrint("numkey:%u\n", keyNumber);
			DbgPrint("sancode:%x\n", myData->MakeCode);
			DbgPrint("%s\n", myData->Flags ? "Up" : "Down");
			
			if (myData->MakeCode == 0x1f)
			{
				myData->MakeCode = 0x20;
			}
		}
	}
	if (pIrp->PendingReturned)
	{
		IoMarkIrpPending(pIrp);
	}
	return pIrp->IoStatus.Status;
}


// IRP读分发函数
NTSTATUS ReadDispatch(PDEVICE_OBJECT pDevice, PIRP pIrp)
{
		NTSTATUS status = STATUS_SUCCESS;
		PDEV_EXTENSION devExt;
		PDEVICE_OBJECT lowDevice;
		PIO_STACK_LOCATION stack;
		if (pIrp->CurrentLocation == 1)
		{
			DbgPrint("irp send error..\n");
			status = STATUS_INVALID_DEVICE_REQUEST;
			pIrp->IoStatus.Status = status;
			pIrp->IoStatus.Information = 0;
			IoCompleteRequest(pIrp, IO_NO_INCREMENT);
			return status;
		}
		// 得到设备扩展。目的是之后为了获得下一个设备的指针。
		devExt = pDevice->DeviceExtension;
		lowDevice = devExt->LowDevice;
		stack = IoGetCurrentIrpStackLocation(pIrp);

		// 复制IRP栈
		IoCopyCurrentIrpStackLocationToNext(pIrp);
		// 设置IRP完成回调函数
		IoSetCompletionRoutine(pIrp, ReadComp, pDevice, TRUE, TRUE, TRUE);
		status = IoCallDriver(lowDevice, pIrp);
		return status;
}


// 电源IRP分发函数
NTSTATUS PowerDispatch(PDEVICE_OBJECT pDevice, PIRP pIrp)
{
	PDEV_EXTENSION devExt;
	devExt = (PDEV_EXTENSION)pDevice->DeviceExtension;

	PoStartNextPowerIrp(pIrp);
	IoSkipCurrentIrpStackLocation(pIrp);
	return PoCallDriver(devExt->TargeDevice, pIrp);
}


// 即插即用IRP分发函数
NTSTATUS PnPDispatch(PDEVICE_OBJECT pDevice, PIRP pIrp)
{
	PDEV_EXTENSION devExt;
	PIO_STACK_LOCATION stack;
	NTSTATUS status = STATUS_SUCCESS;

	devExt = (PDEV_EXTENSION)pDevice->DeviceExtension;
	stack = IoGetCurrentIrpStackLocation(pIrp);

	switch (stack->MinorFunction)
	{
	case IRP_MN_REMOVE_DEVICE :
		// 首先把请求发下去
		IoSkipCurrentIrpStackLocation(pIrp);
		IoCallDriver(devExt->LowDevice, pIrp);
		// 然后解除绑定。
		IoDetachDevice(devExt->LowDevice);
		// 删除我们自己生成的虚拟设备。
		IoDeleteDevice(pDevice);
		status = STATUS_SUCCESS;
		break;

	default :
		// 对于其他类型的IRP，全部都直接下发即可。 
		IoSkipCurrentIrpStackLocation(pIrp);
		status = IoCallDriver(devExt->LowDevice, pIrp);
	}
	return status;
}


// 初始化扩展设备
NTSTATUS DevExtInit(PDEV_EXTENSION devExt, PDEVICE_OBJECT filterDevice, PDEVICE_OBJECT targetDevice, PDEVICE_OBJECT lowDevice)
{
	memset(devExt, 0, sizeof(DEV_EXTENSION));
	devExt->FilterDevice = filterDevice;
	devExt->TargeDevice = targetDevice;
	devExt->LowDevice = lowDevice;
	devExt->Size = sizeof(DEV_EXTENSION);
	KeInitializeSpinLock(&devExt->IoRequestSpinLock);
	KeInitializeEvent(&devExt->IoInProgressEvent, NotificationEvent, FALSE);
	return STATUS_SUCCESS;
}

// 将过滤设备绑定到目标设备上
NTSTATUS AttachDevice(PDRIVER_OBJECT pDriver, PUNICODE_STRING RegPatch)
{
	UNICODE_STRING kbdName = RTL_CONSTANT_STRING(L"\\Driver\\Kbdclass");
	NTSTATUS status = 0;
	PDEV_EXTENSION devExt;			// 过滤设备的扩展设备
	PDEVICE_OBJECT filterDevice;	// 过滤设备 
	PDEVICE_OBJECT targetDevice;		// 目标设备（键盘设备）
	PDEVICE_OBJECT lowDevice;		// 底层设备（向某一个设备上加一个设备时不一定是加到此设备上，而加在设备栈的栈顶）
	PDRIVER_OBJECT kbdDriver;		// 用于接收打开的物理键盘设备

	// 获取键盘驱动的对象，保存在kbdDriver
	status = ObReferenceObjectByName(&kbdName, OBJ_CASE_INSENSITIVE, NULL, 0, *IoDriverObjectType, KernelMode, NULL, &kbdDriver);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("Open KeyBoard Driver Failed\n");
		return status;
	}
	else
	{
		// 解引用
		ObDereferenceObject(kbdDriver);
	}

	// 获取键盘驱动设备链中的第一个设备
	targetDevice = kbdDriver->DeviceObject;
	// 像链表操作一样，遍历键盘键盘设备链中的所有设备
	while (targetDevice)
	{
		// 创建一个过滤设备
		status = IoCreateDevice(pDriver, sizeof(DEV_EXTENSION), NULL, targetDevice->DeviceType, targetDevice->Characteristics, FALSE, &filterDevice);
		if (!NT_SUCCESS(status))
		{
			DbgPrint("Create New FilterDevice Failed\n");
			filterDevice = targetDevice = NULL;
			return status;
		}
		// 绑定，lowDevice是绑定之后得到的下一个设备。
		lowDevice = IoAttachDeviceToDeviceStack(filterDevice, targetDevice);
		if (!lowDevice)
		{
			DbgPrint("Attach Faided!\n");
			IoDeleteDevice(filterDevice);
			filterDevice = NULL;
			return status;
		}
		// 初始化设备扩展
		devExt = (PDEV_EXTENSION)filterDevice->DeviceExtension;
		DevExtInit(devExt, filterDevice, targetDevice, lowDevice);

		filterDevice->DeviceType = lowDevice->DeviceType;
		filterDevice->Characteristics = lowDevice->Characteristics;
		filterDevice->StackSize = lowDevice->StackSize + 1;
		filterDevice->Flags |= lowDevice->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO | DO_POWER_PAGABLE);
		// 遍历下一个设备
		targetDevice = targetDevice->NextDevice;
	}
	DbgPrint("Create And Attach Finshed...\n");
	return status;
}


// 驱动入口函数
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver, PUNICODE_STRING RegPatch)
{
	ULONG i;
	NTSTATUS status = STATUS_SUCCESS;

	pDriver->DriverUnload = DriverUnload;					// 注册驱动卸载函数

	for (i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		pDriver->MajorFunction[i] = GeneralDispatch;		// 注册通用的IRP分发函数
	}

	pDriver->MajorFunction[IRP_MJ_READ] = ReadDispatch;		// 注册读IRP分发函数
	pDriver->MajorFunction[IRP_MJ_POWER] = PowerDispatch;	// 注册电源IRP分发函数
	pDriver->MajorFunction[IRP_MJ_PNP] = PnPDispatch;		// 注册即插即用IRP分发函数

	AttachDevice(pDriver, RegPatch);						// 绑定设备

	return status;
}