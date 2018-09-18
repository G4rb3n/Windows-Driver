#include <ntddk.h>

// 定义一个值为0x800的控制码
#define IOCTL_KILL CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
// 定义设备名和符号名
#define DEVICE_NAME L"\\Device\\MTKillDevice"
#define SYM_LINK_NAME L"\\??\\MTKill"

PDEVICE_OBJECT pDevice;
UNICODE_STRING DeviceName;
UNICODE_STRING SymLinkName;

// 驱动卸载函数
NTSTATUS DriverUnload(PDRIVER_OBJECT Driver)
{
	NTSTATUS status;

	// 删除符号和设备
	IoDeleteSymbolicLink(&SymLinkName);
	IoDeleteDevice(pDevice);
	DbgPrint("This Driver Is Unloading...\n");
	return STATUS_SUCCESS;
}

// 设备共用函数
NTSTATUS DeviceApi(PDEVICE_OBJECT Device, PIRP pIrp)
{
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	// I/O请求处理完毕
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

// 杀死进程函数
BOOLEAN KillProcess(LONG pid)
{
	HANDLE ProcessHandle;
	NTSTATUS status;
	OBJECT_ATTRIBUTES ObjectAttributes;
	CLIENT_ID Cid;

	// 初始化ObjectAttributes和Cid
	InitializeObjectAttributes(&ObjectAttributes, 0, 0, 0, 0);
	Cid.UniqueProcess = (HANDLE)pid;
	Cid.UniqueThread = 0;
	// 打开进程句柄
	status = ZwOpenProcess(&ProcessHandle, PROCESS_ALL_ACCESS, &ObjectAttributes, &Cid);
	if (NT_SUCCESS(status))
	{
		DbgPrint("Open Process %d Successful!\n", pid);
		// 结束进程
		ZwTerminateProcess(ProcessHandle, status);
		// 关闭句柄
		ZwClose(ProcessHandle);
		return TRUE;
	}
	DbgPrint("Open Process %d Failed!\n", pid);
	return FALSE;
}

// 设备I/O控制函数
NTSTATUS DeviceIoctl(PDEVICE_OBJECT Device, PIRP pIrp)
{
	NTSTATUS status;
	// 获取IRP消息的数据
	PIO_STACK_LOCATION irps = IoGetCurrentIrpStackLocation(pIrp);
	// 获取传过来的控制码
	ULONG CODE = irps->Parameters.DeviceIoControl.IoControlCode;
	ULONG info = 0;

	switch (CODE)
	{
	// 若控制等于我们约定的IOCTL_KILL（0x800）
	case IOCTL_KILL:
	{
		DbgPrint("Enter the IO \n");
		// 获取要杀死的进程的PID
		LONG pid = *(PLONG)(pIrp->AssociatedIrp.SystemBuffer);
		DbgPrint("Get PID : %d\n", pid);
		if (KillProcess(pid))
		{
			DbgPrint("Kill Successful\n");
		}
		else
		{
			DbgPrint("Kill Failed\n");
		}
		status = STATUS_SUCCESS;
		break;
	}
	default:
		DbgPrint("Unknown CODE!\n");
		status = STATUS_UNSUCCESSFUL;
		break;
	}

	// I/O请求处理完毕
	pIrp->IoStatus.Status = status;
	pIrp->IoStatus.Information = info;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return status;
}

// 驱动入口函数
NTSTATUS DriverEntry(PDRIVER_OBJECT Driver, PUNICODE_STRING RegPath)
{
	NTSTATUS status;
	
	// 注册驱动卸载函数
	Driver->DriverUnload = DriverUnload;

	// 通过循环将设备创建、读写、关闭等函数设置为通用的DeviceApi
	for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)
	{
		Driver->MajorFunction[i] = DeviceApi;
	}
	// 单独把控制函数设置为DeviceIoctl
	Driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoctl;

	// 将设备名转换为Unicode字符串
	RtlInitUnicodeString(&DeviceName, DEVICE_NAME);
	// 创建设备对象
	status = IoCreateDevice(Driver, 0, &DeviceName, FILE_DEVICE_UNKNOWN, 0, NULL, &pDevice);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("Create Device Faild!\n");
		return STATUS_UNSUCCESSFUL;
	}

	// 将符号名转换为Unicode字符串
	RtlInitUnicodeString(&SymLinkName, SYM_LINK_NAME);
	// 将符号与设备关联
	status = IoCreateSymbolicLink(&SymLinkName, &DeviceName);
	if (!NT_SUCCESS(status))
	{
		DbgPrint("Create SymLink Faild!\n");
		IoDeleteDevice(pDevice);
		return STATUS_UNSUCCESSFUL;
	}

	DbgPrint("Initialize Success\n");

	// 设置pDevice以缓冲区方式读取
	pDevice->Flags = DO_BUFFERED_IO;

	return STATUS_SUCCESS;
}

