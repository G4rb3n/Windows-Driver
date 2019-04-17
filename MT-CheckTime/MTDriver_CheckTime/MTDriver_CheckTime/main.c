#include <ntddk.h>
#include <windef.h>

#define SECOND_OF_DAY 86400

UINT8 DayOfMon[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
ULONG BanedTime = 1568431212;	// 2019.9.14 11:20:12

extern POBJECT_TYPE* PsThreadType;
PETHREAD pThreadObj = NULL;
BOOLEAN TimeSwitch = FALSE;

// 驱动卸载函数
NTSTATUS DriverUnload(PDRIVER_OBJECT dDriver)
{
	TimeSwitch = TRUE;
	// 等待线程退出
	KeWaitForSingleObject(pThreadObj, Executive, KernelMode, FALSE, NULL);
	ObReferenceObject(pThreadObj);
	return STATUS_SUCCESS;
}

// 校验时间函数
BOOLEAN CheckTimeLocal()
{
	LARGE_INTEGER snow, now, tickcount;
	TIME_FIELDS now_fields;

	// 获取标准时间
	KeQuerySystemTime(&snow);

	// 转换为当地时间
	ExSystemTimeToLocalTime(&snow, &now);

	// 整理出年、月、日、时、分、秒
	RtlTimeToTimeFields(&now, &now_fields);

	// 打印年月日
	DbgPrint("当前时间：%d-%d-%d\n", now_fields.Year, now_fields.Month, now_fields.Day);
	
	SHORT i, Cyear = 0;
	ULONG CountDay = 0;

	// 计算时间戳算法
	for ( i = 1970; i < now_fields.Year; i++)
	{
		if ((i % 4 == 0) && (i % 100 != 0) || (i % 400 == 0))
		{
			Cyear++;
		}
	}
	CountDay = Cyear * 366 + (now_fields.Year - 1970 - Cyear) * 365;
	for ( i = 1; i < now_fields.Month; i++)
	{
		if ((i == 2) && (((now_fields.Year % 4 == 0) && (now_fields.Year % 100 != 0)) || (now_fields.Year % 400 == 0)))
		{
			CountDay += 29;
		}
		else
		{
			CountDay += DayOfMon[i - 1];
		}
		CountDay += (now_fields.Day - 1);

		CountDay = CountDay * SECOND_OF_DAY + (unsigned long)now_fields.Hour * 3600 + (unsigned long)now_fields.Minute * 60 + now_fields.Second;

		// 对比时间戳
		DbgPrint("时间戳 ：%d", CountDay);
		if (CountDay < BanedTime)
		{
			return TRUE;
		}
		return FALSE;
	}
}

// 时间校验线程
VOID CheckTimeThread()
{
	LARGE_INTEGER SleepTime;
	SleepTime.QuadPart = -20000000;

	DbgPrint("Enter The Thread\n");
	while (1)
	{
		if (TimeSwitch)
		{
			break;
		}
		if (!CheckTimeLocal())
		{
			DbgPrint("驱动无效\n");
		}
		KeDelayExecutionThread(KernelMode, FALSE, &SleepTime);
	}
	PsTerminateSystemThread(STATUS_SUCCESS);
}

// 驱动入口函数
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver, PUNICODE_STRING RegPath)
{
	pDriver->DriverUnload = DriverUnload;		// 注册驱动卸载函数

	OBJECT_ATTRIBUTES ObjAddr = { 0 };
	HANDLE ThreadHandle = 0;
	// 初始化对象
	InitializeObjectAttributes(&ObjAddr, NULL, OBJ_KERNEL_HANDLE, 0, NULL);
	// 创建线程
	NTSTATUS status = PsCreateSystemThread(&ThreadHandle, THREAD_ALL_ACCESS, &ObjAddr, NULL, NULL, CheckTimeThread, NULL);
	if (!NT_SUCCESS(status))
	{
		return STATUS_NOT_SUPPORTED;
	}
	// 获取线程对象
	status = ObReferenceObjectByHandle(ThreadHandle, THREAD_ALL_ACCESS, *PsThreadType, KernelMode, &pThreadObj, NULL);
	if (!NT_SUCCESS(status))
	{
		ZwClose(ThreadHandle);
		return STATUS_NOT_SUPPORTED;
	}
	ZwClose(ThreadHandle);

	DbgPrint("驱动开始工作\n");

	return STATUS_SUCCESS;
}