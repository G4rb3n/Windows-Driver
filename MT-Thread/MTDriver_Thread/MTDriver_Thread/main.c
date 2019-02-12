#include <ntddk.h>
#include <ntddkbd.h>

PETHREAD pThreadObj = NULL;
BOOLEAN bTerminated = FALSE;


//设备卸载函数
NTSTATUS DriverUnload(PDRIVER_OBJECT pDriver)
{
	DbgPrint("The Driver is Unloading...\n");
	// 设置标志bTerminated为TRUE，以跳出死循环
	bTerminated = TRUE;			
	// 等待线程结束
	KeWaitForSingleObject(pThreadObj, Executive, KernelMode, FALSE, NULL);			
	// 解引用
	ObDereferenceObject(pThreadObj);				
	return STATUS_SUCCESS;
}


// 线程主函数
NTSTATUS TestThread(PVOID pContext)
{
	LARGE_INTEGER inteval;
	// 设置间隔时间为2s
	inteval.QuadPart = -20000000;	
	// inteval.QuadPart = 0;
	while (1)
	{
		// 每隔2s打印一条信息
		DbgPrint("----TestThread----\n");	
		if (bTerminated)
		{
			// 当标志bTerminated为TRUE时跳出死循环
			break;					
		}
		// 休眠，相当于R3环境的Sleep
		KeDelayExecutionThread(KernelMode, FALSE, &inteval);	
	}
	// 终止线程
	PsTerminateSystemThread(STATUS_SUCCESS);			
}


// 线程创建函数
NTSTATUS CreateThread(PVOID TargetEP)
{
	OBJECT_ATTRIBUTES objAddr = { 0 };
	HANDLE threadHandle = 0;
	NTSTATUS status = STATUS_SUCCESS;
	// 初始化一个OBJECT_ATTRIBUTES 对象
	InitializeObjectAttributes(&objAddr, NULL, OBJ_KERNEL_HANDLE, 0, NULL);			
	// 创建线程
	status = PsCreateSystemThread(&threadHandle, THREAD_ALL_ACCESS, &objAddr, NULL, NULL, TestThread, NULL);		
	if (NT_SUCCESS(status))
	{
		KdPrint(("Thread Created\n"));
		// 通过句柄获得线程的对象
		status = ObReferenceObjectByHandle(threadHandle, THREAD_ALL_ACCESS, *PsThreadType, KernelMode, &pThreadObj, NULL);		

		// 释放句柄
		ZwClose(threadHandle);			

		if (!NT_SUCCESS(status))
		{
			// 若获取对象失败，也设置标志为TRUE
			bTerminated = TRUE;			
		}
	}
	return status;
}


// 驱动入口函数
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver, PUNICODE_STRING RegPath)
{
	pDriver->DriverUnload = DriverUnload;				// 注册驱动卸载函数
	NTSTATUS status = status = CreateThread(NULL);		// 调用CreateThread创建线程
	return status;
}