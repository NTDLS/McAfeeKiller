///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Copyright © NetworkDLS 2002, All rights reserved
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A 
// PARTICULAR PURPOSE.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _CSERVICE_H
#define _CSERVICE_H
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define _SVC_YES_	  1
#define _SVC_NO_	  2
#define _SVC_ERROR_   3
#define _SVC_SUCCESS_ 4

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class CService{

public:
	CService::CService();

	typedef void (*OnShutDown)(void);
	typedef bool (*ServiceThread)(LPVOID lpVoid);

	char sServiceName[255];
	char sServiceEventName[300];
	
	int Command(DWORD dwCommand);
	int Install(char *sDisplayName);
	int IsActive(void);
	int IsInstalled(void);
	int RunAsService(LPVOID lpArgument);
	int RunAsService(void);
	void StopRunning(void);
	void KeepServiceRunning(void);

	int RunAsStandAlone(LPVOID lpArgument);
	int RunAsStandAlone(void);
	int Start(void);
	int Stop(void);
	int TriggerEvent(char *sEventName);
	int Uninstall(void);
	int SetDescription(char *sDescr);
	void ShutdownEvent(void);
	
	ServiceThread pServiceThread;
	OnShutDown pOnShutDown;

	friend void WINAPI ControlHandler(DWORD dwMessage);
	friend void WINAPI ServiceMain(DWORD dwNumServiceArgs, LPTSTR *lpServiceArgs);
	friend DWORD WINAPI ServiceThreadProc(LPVOID lpVoid);

	bool Initialize(const char *sName, ServiceThread pServiceThread, OnShutDown pOnShutDown);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	bool RunningAsService()
	{
		return this->bRunningAsService;
	}
	bool Alive()
	{
		return this->bAlive;
	}

protected:
	bool bRunningAsService;
	bool bAlive;

	LPVOID pThreadParam;

	HANDLE hServiceEventHandle;
	HANDLE hServiceThreadProc;
	SERVICE_STATUS stServiceStatus;
	SERVICE_STATUS_HANDLE sthServiceStatusHandler;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

extern CService *gInternalCServicePointer;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
