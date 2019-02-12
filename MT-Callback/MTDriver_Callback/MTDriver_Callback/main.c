#include <ntifs.h>

// 回调函数CreateProcCallback
VOID CreateProcCallback(HANDLE ParentID, HANDLE ProcessID, BOOLEAN Create)
{
	if (Create)
	{
		PEPROCESS Process = NULL;
		NTSTATUS status = PsLookupProcessByProcessId(ProcessID, &Process);					// 根据PID获取进程结构体的地址
		int i;
		if (NT_SUCCESS(status))
		{
			for (i = 0; i < 3 * PAGE_SIZE; i++)
			{
				if (!strncmp("notepad.exe", (PCHAR)Process + i, strlen("notepad.exe")))		// 判断进程名是否为“notepad.exe”
				{
					DbgPrint("Proces %s is created!\n", (PCHAR)((ULONG)Process + i));
					break;
				}
			}
		}
	}
}

//设备卸载函数
NTSTATUS DriverUnload(PDRIVER_OBJECT pDriver)
{
	NTSTATUS stats = PsSetCreateProcessNotifyRoutine(CreateProcCallback, TRUE);
	return STATUS_SUCCESS;
}


// 驱动入口函数
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver, PUNICODE_STRING RegPath)
{
	DbgPrint("Enter the driver\n");

	pDriver->DriverUnload = DriverUnload;

	NTSTATUS stats = PsSetCreateProcessNotifyRoutine(CreateProcCallback, FALSE);		// 注册进程创建事件的回调函数CreateProcCallback

	return stats;
}