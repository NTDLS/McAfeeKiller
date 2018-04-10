#ifndef _ENTRY_CPP
#define _ENTRY_CPP
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Windows.H>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

HINSTANCE hInstance = NULL;
DWORD dwProcessAttachments = 0;
DWORD dwThreadAttachments = 0;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID Reserved)
{
	switch( dwReason ) 
    { 
        case DLL_PROCESS_ATTACH:
			// Initialize once for each new process. Return FALSE to fail DLL load.
			hInstance = (HINSTANCE) hModule;
			dwProcessAttachments++;
			exit(0);
            break;

		case DLL_PROCESS_DETACH:
            // Perform any necessary cleanup.
			dwProcessAttachments--;
			//MessageBox(NULL, "DLL_PROCESS_DETACH!", "Hello!", 0);
            break;

        case DLL_THREAD_ATTACH:
            // Do thread-specific initialization.
			dwThreadAttachments++;
			//MessageBox(NULL, "DLL_THREAD_ATTACH!", "Hello!", 0);
            break;

        case DLL_THREAD_DETACH:
            // Do thread-specific cleanup.
			dwThreadAttachments--;
			//MessageBox(NULL, "DLL_THREAD_DETACH!", "Hello!", 0);
            break;

    }
	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////
#endif
