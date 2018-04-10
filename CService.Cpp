///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Copyright © NetworkDLS 2002, All rights reserved
//
// THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF 
// ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A 
// PARTICULAR PURPOSE.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef _CSERVICE_CPP
#define _CSERVICE_CPP
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Windows.H>
#include <StdIO.H>
#include <StdLib.H>

#include "CService.H"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CService *gInternalCServicePointer; //Only one service per proccess, sorry!

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CService::CService()
{
	memset(this->sServiceName, 0, sizeof(this->sServiceName));
	memset(this->sServiceEventName, 0, sizeof(this->sServiceEventName));

	this->bRunningAsService = false;
	this->bAlive = false;

	pServiceThread = NULL;
	pOnShutDown = NULL;

	gInternalCServicePointer = this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool CService::Initialize(const char *sName, ServiceThread lpServiceThread, OnShutDown lpOnShutDown)
{
	strcpy_s(this->sServiceName, sizeof(this->sServiceName), sName);
	sprintf_s(this->sServiceEventName, sizeof(this->sServiceEventName), "Global\\%s Event", sName);

	this->bRunningAsService = false;
	this->bAlive = false;

	pServiceThread = lpServiceThread;
	pOnShutDown = lpOnShutDown;

	gInternalCServicePointer = this;

	return true;
}
	
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CService::RunAsStandAlone(LPVOID lpArgument)
{
	this->bRunningAsService = false;
	this->bAlive = true;

	SECURITY_DESCRIPTOR SD;
	SECURITY_ATTRIBUTES SA;

	if(!InitializeSecurityDescriptor(&SD, SECURITY_DESCRIPTOR_REVISION))
	{
		return _SVC_ERROR_;
	}

	if(!SetSecurityDescriptorDacl(&SD, TRUE, 0, FALSE))
	{
		return _SVC_ERROR_;
	}

	SA.nLength				= sizeof(SA);
	SA.lpSecurityDescriptor = &SD;
	SA.bInheritHandle		= FALSE;

	// Create a semaphore to keep service alive
	this->hServiceEventHandle =
		CreateEvent(&SA, FALSE, FALSE, gInternalCServicePointer->sServiceEventName);

	this->pServiceThread(this->pThreadParam);
	this->ShutdownEvent();

	return _SVC_SUCCESS_;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CService::RunAsStandAlone(void)
{
	return this->RunAsStandAlone(NULL);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CService::RunAsService(void)
{
	return this->RunAsService(NULL);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CService::RunAsService(LPVOID lpArgument)
{
	this->pThreadParam = lpArgument;

	// Create Service Data Array (Contains Information About Service)
	SERVICE_TABLE_ENTRY SrvTable[2];

	// Verify if service is running
	hServiceEventHandle = OpenEvent(SYNCHRONIZE, FALSE, sServiceEventName);
	if( hServiceEventHandle )
	{
		//"The service is already active.
		CloseHandle( hServiceEventHandle );
		return _SVC_ERROR_;
	}

	SECURITY_DESCRIPTOR SD;
	SECURITY_ATTRIBUTES SA;

	InitializeSecurityDescriptor(&SD, SECURITY_DESCRIPTOR_REVISION);

	SetSecurityDescriptorDacl(&SD, TRUE, 0, FALSE);

	SA.nLength = sizeof(SA);
	SA.lpSecurityDescriptor = &SD;
	SA.bInheritHandle = FALSE;

	// Init Global Vars
	memset( &stServiceStatus, 0x00, sizeof(stServiceStatus) );
	hServiceEventHandle = NULL;

	// Load service
	SrvTable[0].lpServiceName = this->sServiceName;	// Service name
	SrvTable[0].lpServiceProc = ServiceMain;		// Service EntryPoint
	SrvTable[1].lpServiceName = NULL;				// Nothing
	SrvTable[1].lpServiceProc = NULL;				// Nothing

	this->bRunningAsService = true;
	this->bAlive = true;

	// Pass the control to ServiceMain
	if(!StartServiceCtrlDispatcher(SrvTable))
	{
		//Error starting the service.
		return _SVC_ERROR_;
	}

	return _SVC_SUCCESS_;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WINAPI ServiceMain(DWORD dwNumServiceArgs, LPTSTR *lpServiceArgs)
{
	DWORD Result = 0;

	// Register function that will receive messages from service manager
	// Must be the first action in Service Main
	gInternalCServicePointer->sthServiceStatusHandler =
		RegisterServiceCtrlHandler(gInternalCServicePointer->sServiceName, (LPHANDLER_FUNCTION)ControlHandler);

	if( gInternalCServicePointer->sthServiceStatusHandler == (SERVICE_STATUS_HANDLE)NULL)
	{
		MessageBox(NULL, "Error in ServiceMain::RegisterServiceCtrlHandler!",
			gInternalCServicePointer->sServiceName, MB_TASKMODAL|MB_ICONERROR);
		return;
	}

	//-------------------------------------------------------------------------------
	SECURITY_DESCRIPTOR SD;
	SECURITY_ATTRIBUTES SA;

	if(!InitializeSecurityDescriptor(&SD, SECURITY_DESCRIPTOR_REVISION))
	{
		return;
	}

	if(!SetSecurityDescriptorDacl(&SD, TRUE, 0, FALSE))
	{
		return;
	}

	SA.nLength				= sizeof(SA);
	SA.lpSecurityDescriptor = &SD;
	SA.bInheritHandle		= FALSE;

	// Create a semaphore to keep service alive
	gInternalCServicePointer->hServiceEventHandle =
		CreateEvent(&SA, FALSE, FALSE, gInternalCServicePointer->sServiceEventName);

	gInternalCServicePointer->stServiceStatus.dwServiceType 			= SERVICE_WIN32 | SERVICE_INTERACTIVE_PROCESS;
	gInternalCServicePointer->stServiceStatus.dwCurrentState			= SERVICE_RUNNING;
	gInternalCServicePointer->stServiceStatus.dwControlsAccepted		= SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	gInternalCServicePointer->stServiceStatus.dwWin32ExitCode			= NO_ERROR;
	gInternalCServicePointer->stServiceStatus.dwServiceSpecificExitCode = NO_ERROR;
	gInternalCServicePointer->stServiceStatus.dwCheckPoint			    = 0;
	gInternalCServicePointer->stServiceStatus.dwWaitHint				= 0;

	// Make the first Status update
	Result = SetServiceStatus(
		gInternalCServicePointer->sthServiceStatusHandler,
		&gInternalCServicePointer->stServiceStatus);
	if(!Result)
	{
		MessageBox(NULL, "Error in ServiceMain::SetServiceStatus!",
			gInternalCServicePointer->sServiceName, MB_TASKMODAL|MB_ICONERROR);
		return;
	}

	if(gInternalCServicePointer->pServiceThread)
	{
		gInternalCServicePointer->pServiceThread(gInternalCServicePointer->pThreadParam);
	}

	//Service received a termination command
	gInternalCServicePointer->ShutdownEvent();

	gInternalCServicePointer->stServiceStatus.dwCurrentState = SERVICE_STOPPED;
	SetServiceStatus(gInternalCServicePointer->sthServiceStatusHandler, &gInternalCServicePointer->stServiceStatus);

	return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CService::KeepServiceRunning(void)
{
	//Keep Service Running
	WaitForSingleObject(this->hServiceEventHandle, INFINITE);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void CService::ShutdownEvent(void)
{
	this->bAlive = false;

	if(this->pOnShutDown)
	{
		this->pOnShutDown();
	}

	if(this->hServiceThreadProc)
	{
		DWORD dwExitCode = 0;
		int iWaitCount = 0;

		while(GetExitCodeThread(this->hServiceThreadProc, &dwExitCode))
		{
			if(dwExitCode != STILL_ACTIVE || iWaitCount == 60000)
			{
				if(iWaitCount == 60000)
				{
					TerminateThread(this->hServiceThreadProc, 1);
				}

				CloseHandle(this->hServiceThreadProc);
			}
			iWaitCount++;
			Sleep(1);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WINAPI ControlHandler(DWORD dwMessage)
{
	DWORD Result = 0;

	// Receive messages from service control dispatcher
	switch (dwMessage)
	{
//		case SERVICE_CONTROL_PAUSE :
//			stServiceStatus.dwWaitHint	 = 1000;
//			stServiceStatus.dwCurrentState = SERVICE_PAUSED;
//			break;

//		case SERVICE_CONTROL_CONTINUE :
//			stServiceStatus.dwWaitHint	 = 1000;
//			stServiceStatus.dwCurrentState = SERVICE_RUNNING;
//			break;

		case SERVICE_CONTROL_INTERROGATE :
			gInternalCServicePointer->stServiceStatus.dwWaitHint	 = 500;
			break;

		case SERVICE_CONTROL_SHUTDOWN:
			gInternalCServicePointer->stServiceStatus.dwWaitHint	 = 5000;
			gInternalCServicePointer->stServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
			break;

		case SERVICE_CONTROL_STOP :
			gInternalCServicePointer->stServiceStatus.dwWaitHint	 = 5000;
			gInternalCServicePointer->stServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
			break;
	}

	// Update service status
	Result = SetServiceStatus(gInternalCServicePointer->sthServiceStatusHandler, &gInternalCServicePointer->stServiceStatus );
	if(!Result)
	{
		MessageBox(NULL, "Error in ControlHandler::SetServiceStatus!",
			gInternalCServicePointer->sServiceName, MB_TASKMODAL|MB_ICONERROR);
		SetEvent(gInternalCServicePointer->hServiceEventHandle );
	}

	// If message is a stop or shutdown so, service is going to be stoped
	if((dwMessage == SERVICE_CONTROL_STOP) || (dwMessage == SERVICE_CONTROL_SHUTDOWN))
	{
		SetEvent(gInternalCServicePointer->hServiceEventHandle );
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
	To be called by the actual application that is running as a service to stop the local service manually.
*/
void CService::StopRunning(void)
{
	SetEvent(this->hServiceEventHandle);
	this->bAlive = false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
SERVICE_CONTROL_STOP
	Requests the service to stop. The hService handle must have SERVICE_STOP access.
SERVICE_CONTROL_PAUSE
	Requests the service to pause. The hService handle must have SERVICE_PAUSE_CONTINUE access.
SERVICE_CONTROL_CONTINUE
	Requests the paused service to resume. The hService handle must have SERVICE_PAUSE_CONTINUE access.
SERVICE_CONTROL_INTERROGATE
	Requests the service to update immediately its current status information to the service control manager. The hService handle must have SERVICE_INTERROGATE access.
SERVICE_CONTROL_SHUTDOWN
	The ControlService function fails if this control code is specified.
*/

int CService::Command(DWORD dwCommand)
{
	SERVICE_STATUS stLocalServiceStatus;
	SC_HANDLE hServiceControlHandle = NULL;
	SC_HANDLE hServiceHandle 	   = NULL;

	BOOL Result = FALSE;

	// UnInstall The Service From Service Control Manager Database
	hServiceControlHandle = OpenSCManager( NULL, NULL, SC_MANAGER_CREATE_SERVICE );
	if( hServiceControlHandle == NULL )
	{
		//Error in Command:OpenSCManager.
		return _SVC_ERROR_;
	}

	hServiceHandle = OpenService( hServiceControlHandle, this->sServiceName, SERVICE_ALL_ACCESS);
	if( hServiceHandle == NULL )
	{
		//Error in CommandService:OpenService
		CloseServiceHandle(hServiceControlHandle);
		return _SVC_ERROR_;
	}

	Result = ControlService(
		hServiceHandle,
		dwCommand,
		&stLocalServiceStatus 
	);

	if(Result == FALSE)
	{
		//Error in CommandService::ControlService.
		CloseServiceHandle(hServiceHandle);
		CloseServiceHandle(hServiceControlHandle);
		return _SVC_ERROR_;
	}

	CloseServiceHandle(hServiceHandle);
	CloseServiceHandle(hServiceControlHandle);

	return _SVC_SUCCESS_;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CService::Stop(void)
{
	return this->Command(SERVICE_CONTROL_STOP);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CService::Start(void)
{
	SC_HANDLE hServiceControlHandle = NULL;
	SC_HANDLE hServiceHandle = NULL;
	BOOL Result = FALSE;

	// UnInstall The Service From Service Control Manager Database
	hServiceControlHandle = OpenSCManager( NULL, NULL, SC_MANAGER_CREATE_SERVICE );
	if( hServiceControlHandle == NULL )
	{
		//Error in StartService:OpenSCManager
		return _SVC_ERROR_;
	}

	hServiceHandle = OpenService(hServiceControlHandle, this->sServiceName, SERVICE_ALL_ACCESS);

	if( hServiceHandle == NULL )
	{
		//Error in StartService:OpenService
		CloseServiceHandle(hServiceControlHandle);
		return _SVC_ERROR_;
	}

	Result = StartService(
		hServiceHandle,	// Handle Of Service
		NULL,			// Number Of Arguments
		NULL			// Address Of Array Of Argument String Pointers
	);

	if(Result == FALSE)
	{
		//Error in StartService:StartService
		CloseServiceHandle(hServiceHandle);
		CloseServiceHandle(hServiceControlHandle);
		return _SVC_ERROR_;
	}

	CloseServiceHandle(hServiceHandle);
	CloseServiceHandle(hServiceControlHandle);

	return _SVC_SUCCESS_;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CService::Install(char *sDisplayName)
{
	SC_HANDLE hServiceControlHandle;
	SC_HANDLE hServiceHandle;

	// Install the Service In The Control Manager Database
	hServiceControlHandle = OpenSCManager( NULL, NULL, SC_MANAGER_CREATE_SERVICE );
	if( hServiceControlHandle == NULL )
	{
		//Error in InstallService-OpenSCManager
		return _SVC_ERROR_;
	}

	char sApplication[MAX_PATH];
	char sSrvcCmdLine[MAX_PATH];

	GetModuleFileName(GetModuleHandle(NULL), sApplication, sizeof(sApplication));
	sprintf_s(sSrvcCmdLine, sizeof(sSrvcCmdLine), "%s /RunService", sApplication);

	hServiceHandle = CreateService(
		hServiceControlHandle,
		this->sServiceName,
		sDisplayName,
		SERVICE_ALL_ACCESS,
		SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,
		SERVICE_DEMAND_START,
		SERVICE_ERROR_NORMAL,
		sSrvcCmdLine,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL );

	if( hServiceHandle == NULL )
	{
		//Error in InstallService-CreateService
		CloseServiceHandle( hServiceControlHandle );
		return _SVC_ERROR_;
	}

	CloseServiceHandle( hServiceHandle );
	CloseServiceHandle( hServiceControlHandle );

	return _SVC_SUCCESS_;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CService::Uninstall(void)
{
	SC_HANDLE hServiceControlHandle;
	SC_HANDLE hServiceHandle;

	// UnInstall The Service From Service Control Manager Database
	hServiceControlHandle = OpenSCManager( NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if( hServiceControlHandle == NULL )
	{
		//Error in UninstallService-OpenSCManager
		return _SVC_ERROR_;
	}

	hServiceHandle = OpenService( hServiceControlHandle, this->sServiceName, SERVICE_ALL_ACCESS);

	if( hServiceHandle == NULL )
	{
		//Error in UninstallService-OpenService
		CloseServiceHandle(hServiceControlHandle);
		return _SVC_ERROR_;
	}

	if( !DeleteService(hServiceHandle) )
	{
		//Error in UninstallService-DeleteService
		CloseServiceHandle(hServiceHandle);
		CloseServiceHandle(hServiceControlHandle);
		return _SVC_ERROR_;
	}

	CloseServiceHandle(hServiceHandle);
	CloseServiceHandle(hServiceControlHandle);

	return _SVC_SUCCESS_;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CService::IsInstalled(void)
{
	SC_HANDLE hServiceControlHandle;
	SC_HANDLE hServiceHandle;

	hServiceControlHandle = OpenSCManager( NULL,  NULL, SC_MANAGER_CREATE_SERVICE);
	if( hServiceControlHandle == NULL )
	{
		//Error in IsServiceInstalled-OpenSCManager
		return _SVC_ERROR_;
	}

	hServiceHandle = OpenService( hServiceControlHandle, this->sServiceName, SERVICE_ALL_ACCESS);

	if( hServiceHandle == NULL)
	{
		CloseServiceHandle( hServiceHandle );
		CloseServiceHandle( hServiceControlHandle );
		return _SVC_NO_;
	}

	CloseServiceHandle( hServiceHandle );
	CloseServiceHandle( hServiceControlHandle );

	return _SVC_YES_;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CService::IsActive(void)
{
	HANDLE hEventHandle = OpenEvent(SYNCHRONIZE, FALSE, sServiceEventName);
	if(hEventHandle)
	{
		CloseHandle( hEventHandle );
		return _SVC_YES_;
	}

    return _SVC_NO_;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CService::TriggerEvent(char *sEventName)
{
	HANDLE hLocalEventHandle = NULL;
	int iResult = _SVC_ERROR_;

	if((hLocalEventHandle = OpenEvent(EVENT_MODIFY_STATE, FALSE, sEventName)) == NULL)
	{
        return _SVC_ERROR_;
	}

    if(!SetEvent(hLocalEventHandle))
	{
		iResult = _SVC_ERROR_;
	}
	else{
		iResult = _SVC_SUCCESS_;
	}

	CloseHandle(hLocalEventHandle);

	return iResult;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int CService::SetDescription(char *sDescr)
{
	SC_HANDLE hServiceControlHandle = NULL;
	SC_HANDLE hServiceHandle = NULL;
	BOOL Result = FALSE;

	// UnInstall The Service From Service Control Manager Database
	hServiceControlHandle = OpenSCManager( NULL, NULL, SC_MANAGER_CREATE_SERVICE );
	if( hServiceControlHandle == NULL )
	{
		//Error in SetServiceDescription-OpenSCManager
		return _SVC_ERROR_;
	}

	hServiceHandle = OpenService( hServiceControlHandle, this->sServiceName, SERVICE_ALL_ACCESS);

	if( hServiceHandle == NULL )
	{
		//Error in SetServiceDescription-OpenService
		CloseServiceHandle(hServiceControlHandle);
		return _SVC_ERROR_;
	}

	SERVICE_DESCRIPTION sd;

	memset(&sd, 0, sizeof(sd));
	sd.lpDescription = sDescr;

	ChangeServiceConfig2(hServiceHandle, SERVICE_CONFIG_DESCRIPTION, &sd);

	CloseServiceHandle(hServiceHandle);
	CloseServiceHandle(hServiceControlHandle);

	return _SVC_SUCCESS_;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
