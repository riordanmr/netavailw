// netavailw.cpp : Program to test the health of a network by periodically
// pinging a host.  The results are displayed in a window, and logged to a file.
// Mark Riordan  2024-05-14 (also see the recent netavail.go, which isn't reliable)

#include "framework.h"
#include "netavailw.h"
#include <string>
#include <time.h>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
HWND hDlgGlobal = NULL; // Global variable to store the dialog handle

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

std::string GetTimeStr() 
{
    time_t mytime = time(NULL);
    tm mytm;
    localtime_s(&mytm, &mytime);
    char sztime[32];
    strftime(sztime, sizeof(sztime), "%Y-%m-%d %H:%M:%S", &mytm);
    return std::string(sztime);
}

DWORD WINAPI PingThreadFunction(LPVOID lpParam)
{
    do {
        std::string msg = GetTimeStr();
        SetDlgItemText(hDlgGlobal, IDC_STATIC_PINGMS, msg.c_str());
        Sleep(4000);
    } while (true);

    return 0;
}

BOOL LaunchPingThread()
{
    BOOL bOK = TRUE;
    DWORD dwThreadId;
    HANDLE hThread;

    hThread = CreateThread(
        NULL,                   // default security attributes
        0,                      // use default stack size  
        PingThreadFunction,     // thread function name
        NULL,                   // argument to thread function 
        0,                      // use default creation flags 
        &dwThreadId);           // returns the thread identifier 

    if (hThread == NULL)
    {
        bOK = FALSE;
    }
    return bOK;
}

INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INITDIALOG:
        hDlgGlobal = hDlg; // Store the dialog handle
        if (!LaunchPingThread()) {
            MessageBox(NULL, "Cannot launch ping thread", "Error", MB_OK | MB_ICONHAND);
        }
        return (INT_PTR)TRUE;

    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC) wParam;
        HWND hwndStatic = (HWND) lParam;
        if (GetDlgCtrlID(hwndStatic) == IDC_STATIC_ERROR) 
        {
            SetTextColor(hdcStatic, RGB(255, 0, 0)); // Set text color to red
            return (INT_PTR)GetStockObject(NULL_BRUSH);
        }
        break;
    } 

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            PostQuitMessage(0);
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   // Create a modal dialog box
   INT_PTR success = DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, DialogProc);

   if (success == -1)
   {
      return FALSE;
   }

   return TRUE;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_NETAVAILW, szWindowClass, MAX_LOADSTRING);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    return 0;
}
