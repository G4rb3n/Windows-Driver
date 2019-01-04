#include <ntddk.h>
#include <windef.h>

PVOID updatetimeAddr = NULL;

PVOID querycounterAddr = NULL;

const DWORD g_dwSpeedBase = 100;		// 变速基数
DWORD g_dwSpeed_X = 1000;				// 变数数值


LARGE_INTEGER g_originCounter;

LARGE_INTEGER g_returnCounter;

// 驱动卸载函数
NTSTATUS DriverUnload(PDRIVER_OBJECT dDriver)
{
	return STATUS_SUCCESS;
}


// 添加内存写属性函数
void __declspec(naked) WPOFF()
{
	__asm
	{
		cli
		mov eax, cr0
		and eax, not 0x10000
		mov cr0, eax
		ret
	}
}


// 去除内存写属性函数
void __declspec(naked) WPON()
{
	__asm
	{
		mov eax, cr0
		or eax, 0x10000
		mov cr0, eax
		sti
		ret
	}
}


// KeUpdateSystemTime的备份函数
void __declspec(naked) __cdecl updatetimeOriginCode()
{
	__asm
	{
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		mov esi, updatetimeAddr
		add esi, 7
		jmp esi
	}
}


// KeQueryPerformanceCounter的备份函数
LARGE_INTEGER __declspec(naked) __stdcall querycounterOriginCode(OUT PLARGE_INTEGER PerformanceFrequency)
{
	__asm
	{
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		mov eax, querycounterAddr
		add eax, 5
		jmp eax
	}
}


// KeUpdateSystemTime的Hook函数
void __declspec(naked) __cdecl fakeupdatetimeAddr()
{
	__asm
	{

		mul g_dwSpeed_X							// 在调用KeUpdateSystemTime之前对参数EAX进行修改
		div g_dwSpeedBase						// 实现变速（EAX * 当前速度值 / 速度基数）
		jmp updatetimeOriginCode
	}
}


// KeQueryPerformanceCounter的Hook函数
LARGE_INTEGER __stdcall fakequerycounterAddr(OUT PLARGE_INTEGER PerformanceFrequency)
{
	LARGE_INTEGER realTime;
	LARGE_INTEGER fakeTime;

	realTime = querycounterOriginCode(PerformanceFrequency);		// 获取当前时间

	fakeTime.QuadPart = g_returnCounter.QuadPart + (realTime.QuadPart - g_originCounter.QuadPart) * g_dwSpeed_X / g_dwSpeedBase;	// 返回伪造时间

	g_originCounter.QuadPart = realTime.QuadPart;	// 保存原始时间
	g_returnCounter.QuadPart = fakeTime.QuadPart;		// 保存伪造时间

	return fakeTime;
}


// 驱动入口函数
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver, PUNICODE_STRING RegPath)
{
	pDriver->DriverUnload = DriverUnload;		// 注册驱动卸载函数

	UNICODE_STRING updatetimeName = RTL_CONSTANT_STRING(L"KeUpdateSystemTime");
	updatetimeAddr = MmGetSystemRoutineAddress(&updatetimeName);				// 获取KeUpdateSystemTime的地址
	UNICODE_STRING querycounterName = RTL_CONSTANT_STRING(L"KeQueryPerformanceCounter");
	querycounterAddr = MmGetSystemRoutineAddress(&querycounterName);			// 获取KeQueryPerformanceCounter的地址

	g_originCounter.QuadPart = 0;
	g_returnCounter.QuadPart = 0;
	g_originCounter = KeQueryPerformanceCounter(NULL);
	g_returnCounter.QuadPart = g_originCounter.QuadPart;	// 在变速前先获取下当前系统时间

	BYTE updatetimeJmpCode[5] = { 0xE9, 0x00, 0x00, 0x00, 0x00 };		// KeUpdateSystemTime的JmpCode
	BYTE querycounterJmpCode[5] = { 0xE9, 0x00, 0x00, 0x00, 0x00 };		// KeQueryPerformanceCounter的JmpCode
	*(DWORD*)(updatetimeJmpCode + 1) = (DWORD)fakeupdatetimeAddr - ((DWORD)updatetimeAddr + 5);				// 计算跳转偏移
	*(DWORD*)(querycounterJmpCode + 1) = (DWORD)fakequerycounterAddr - ((DWORD)querycounterAddr + 5);		// 计算跳转偏移

	WPOFF();		// 修改当前进程（system）的内存属性为可写
	KIRQL Irql = KeRaiseIrqlToDpcLevel();		// 提高中断级，避免操作被打断
	RtlCopyMemory(updatetimeOriginCode, updatetimeAddr, 7);				// 将KeUpdateSystemTime原始代码的前5字节备份到updatetimeOriginCode
	RtlCopyMemory((BYTE*)updatetimeAddr, updatetimeJmpCode, 5);			// 将JmpCode覆盖到KeUpdateSystemTime函数的起始地址处

	RtlCopyMemory(querycounterOriginCode, querycounterAddr, 5);			// 将KeQueryPerformanceCounter原始代码的前5字节备份到querycounterOriginCode
	RtlCopyMemory((BYTE*)querycounterAddr, querycounterJmpCode, 5);		// 将JmpCode覆盖到KeQueryPerformanceCounter函数的起始地址处

	KeLowerIrql(Irql);		// 还原中断级
	WPON();					// 还原内存属性

	return STATUS_SUCCESS;
}