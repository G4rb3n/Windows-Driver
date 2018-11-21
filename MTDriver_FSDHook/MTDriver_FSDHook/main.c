#include <ntddk.h>
#include <ntddkbd.h>

extern POBJECT_TYPE *IoDriverObjectType;
PDRIVER_OBJECT kbdDriver = NULL;

typedef NTSTATUS(*POldReadDispatch)(PDEVICE_OBJECT pDevice, PIRP pIrp);

POldReadDispatch OldReadDispatch = NULL;

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


//设备卸载函数
NTSTATUS DriverUnload(PDRIVER_OBJECT pDriver)
{
	DbgPrint("The Driver is Unloading...\n");
	// 卸载驱动时，别忘了还原
	if (kbdDriver != NULL)
	{
		kbdDriver->MajorFunction[IRP_MJ_READ] = OldReadDispatch;
	}
	return STATUS_SUCCESS;
}

// Hook函数
NTSTATUS HookDispatch(PDEVICE_OBJECT pDevice,PIRP pIrp)
{
	// 当键盘有击键时，该函数就会被调用，可以自定义该函数的功能，这里仅作演示打印一句话
	DbgPrint("----Hook KeyBoard Read----\n");
	// 最后再调用回原来的派遣函数，以让键盘正常工作
	return OldReadDispatch(pDevice, pIrp);
}

// 驱动入口函数
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver, PUNICODE_STRING RegPath)
{
	pDriver->DriverUnload = DriverUnload;		// 注册驱动卸载函数

	UNICODE_STRING kbdName = RTL_CONSTANT_STRING(L"\\Driver\\Kbdclass");
	NTSTATUS status = ObReferenceObjectByName(&kbdName, OBJ_CASE_INSENSITIVE, NULL, 0, *IoDriverObjectType, KernelMode, NULL, &kbdDriver);	// 获取键盘驱动的对象，保存在kbdDriver
	if (!NT_SUCCESS(status))
	{
		DbgPrint("Open Keyboard Driver Failed\n");
		return status;
	}
	else
	{
		// 解引用
		ObDereferenceObject(kbdDriver);
	}

	OldReadDispatch = (POldReadDispatch)kbdDriver->MajorFunction[IRP_MJ_READ];		// 在替换之前先保存键盘驱动的READ派遣函数地址，以便后续调用
	kbdDriver->MajorFunction[IRP_MJ_READ] = HookDispatch;							// 将键盘驱动的READ派遣函数替换为我们的Hook函数

	return status;
}