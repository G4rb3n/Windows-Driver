#include <ntddk.h>

// 定义设备名和符号名
#define DEVICE_NAME L"\\Device\\MTReadDevice"
#define SYM_LINK_NAME L"\\??\\MTRead"

PDEVICE_OBJECT pDevice;
UNICODE_STRING DeviceName;
UNICODE_STRING SymLinkName;

// 设备创建函数
NTSTATUS DeviceCreate(PDEVICE_OBJECT Device, PIRP pIrp)
{
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	// I/O请求处理完毕
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	DbgPrint("Create Device Success\n");
	return STATUS_SUCCESS;
}

// 设备读操作函数
NTSTATUS DeviceRead(PDEVICE_OBJECT Device, PIRP pIrp)
{
	// 获取指向IRP的堆栈的指针
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(pIrp);
	// 获取堆栈长度
	ULONG length = stack->Parameters.Read.Length;
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = length;
	// 将堆栈上的数据全设置为0xAA
	memset(pIrp->AssociatedIrp.SystemBuffer, 0xAA, length);
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	DbgPrint("Read Device Success\n");
	return STATUS_SUCCESS;
}

// 设备关闭函数
NTSTATUS DeviceClose(PDEVICE_OBJECT Device, PIRP pIrp)
{
	// 跟设备创建函数相同
	pIrp->IoStatus.Status = STATUS_SUCCESS;
	pIrp->IoStatus.Information = 0;
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	DbgPrint("Close Device Success\n");
	return STATUS_SUCCESS;
}

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

// 驱动入口函数
NTSTATUS DriverEntry(PDRIVER_OBJECT Driver, PUNICODE_STRING RegPath)
{
	NTSTATUS status;
	
	// 注册设备创建函数、设备读函数、设备关闭函数、驱动卸载函数
	Driver->MajorFunction[IRP_MJ_CREATE] = DeviceCreate;
	Driver->MajorFunction[IRP_MJ_READ] = DeviceRead;
	Driver->MajorFunction[IRP_MJ_CLOSE] = DeviceClose;
	Driver->DriverUnload = DriverUnload;

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