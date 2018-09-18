#include <ntddk.h>


NTSTATUS DriverUnload(PDRIVER_OBJECT Driver)
{
	DbgPrint("This driver is unloading...\n");	//打印卸载信息

	return STATUS_SUCCESS;
}


NTSTATUS DriverEntry(PDRIVER_OBJECT Driver, PUNICODE_STRING RegPath)
{
	Driver->DriverUnload = DriverUnload;		// 声明驱动卸载函数
	DbgPrint("%ws\n", RegPath->Buffer);			// 打印RegPath
	return STATUS_SUCCESS;
}