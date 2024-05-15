// netavailw.cpp : Program to test the health of a network by periodically
// pinging a host.  The results are displayed in a window, and logged to a file.
// Mark Riordan  2024-05-14 (also see the recent netavail.go, which isn't reliable)

#include "framework.h"
#include "netavailw.h"
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdio.h>
#include <fstream>
#include <string>
#include <time.h>

#define _WINSOCK_DEPRECATED_NO_WARNINGS 
#include <winsock2.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

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

struct struct_settings {
    std::string strRemoteIP = "8.8.8.8";
    long        msBadPing = 50;
    DWORD       msSleep = 5000;
} Settings;


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

void LogToFile(std::string msg)
{
    std::string fullMsg = GetTimeStr() + " " + msg;
    std::ofstream file;

    // Open the file in append mode
    file.open("netavailw.log", std::ios_base::app);

    if (file.is_open()) {
        // Write the string to the file
        file << fullMsg << std::endl;

        // Close the file
        file.close();
    } else {
        // error
    }
}

void SetErrorText(const char* msg)
{
    //std::string strFull = GetTimeStr() + " ";
    //strFull += msg;
    SetDlgItemText(hDlgGlobal, IDC_STATIC_ERROR, msg);
}

long Ping(const char* address, std::string &strError)
{
    HANDLE hIcmp;
    unsigned long ipaddr = INADDR_NONE;
    DWORD dwRetVal = 0;
    char SendData[32] = "Data Buffer";
    LPVOID ReplyBuffer;
    DWORD ReplySize = 0;

    ipaddr = inet_addr(address);
    if (ipaddr == INADDR_NONE) {
        strError = "inet_addr failed; IP: ";
        strError += address;
        return -1;
    }

    hIcmp = IcmpCreateFile();
    if (hIcmp == INVALID_HANDLE_VALUE) {
        strError = "Unable to open handle.";
        return -1;
    }

    ReplySize = sizeof(ICMP_ECHO_REPLY) + sizeof(SendData);
    ReplyBuffer = (VOID*)malloc(ReplySize);
    if (ReplyBuffer == NULL) {
        strError = "Unable to allocate memory";
        return -1;
    }

    dwRetVal = IcmpSendEcho(hIcmp, ipaddr, SendData, sizeof(SendData),
        NULL, ReplyBuffer, ReplySize, 1000);
    if (dwRetVal != 0) {
        PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY)ReplyBuffer;
        struct in_addr ReplyAddr;
        ReplyAddr.S_un.S_addr = pEchoReply->Address;
        printf("\tSent icmp message to %s\n", address);
        if (dwRetVal > 0) {
            //printf("\tReceived %ld icmp message responses\n", dwRetVal);
            //printf("\tInformation from the first response:\n");
            //printf("\t  Received from %s\n", inet_ntoa(ReplyAddr));
            //printf("\t  Roundtrip time = %ld milliseconds\n", pEchoReply->RoundTripTime);
            long roundTripTime = pEchoReply->RoundTripTime;
            free(ReplyBuffer);
            IcmpCloseHandle(hIcmp);
            return roundTripTime;
        } else {
            strError = "Call to IcmpSendEcho failed.";
            free(ReplyBuffer);
            IcmpCloseHandle(hIcmp);
            return -1;
        }
    } else {
        free(ReplyBuffer);
        strError = "Call to IcmpSendEcho failed 2.";
        IcmpCloseHandle(hIcmp);
        return -1;
    }
}

DWORD WINAPI PingThreadFunction(LPVOID lpParam)
{
    do {
        std::string strError;
        long msPing = Ping(Settings.strRemoteIP.c_str(), strError);
        if (msPing >= 0) {
            char szbuf[64];
            sprintf_s(szbuf, "%ld ms", msPing);
            std::string msg = GetTimeStr() + "  " + std::string(szbuf);
            SetDlgItemText(hDlgGlobal, IDC_STATIC_PINGMS, msg.c_str());
            LogToFile(std::string(szbuf));
            if (msPing >= Settings.msBadPing) {
                msg = GetTimeStr() + "  Long ping time: ";
                char szBuf[32];
                _itoa_s(msPing, szBuf, 10);
                msg += szBuf;
                SetErrorText(msg.c_str());
            }
        } else {
            std::string msg = GetTimeStr() + "  Error: " + strError;
            SetDlgItemText(hDlgGlobal, IDC_STATIC_PINGMS, msg.c_str());
            SetErrorText(msg.c_str());
            LogToFile("Error: " + msg);
        }
        Sleep(Settings.msSleep);
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
    static HBRUSH hbrBkgnd;

    switch (message)
    {
    case WM_INITDIALOG:
        hDlgGlobal = hDlg; // Store the dialog handle
        if (!LaunchPingThread()) {
            MessageBox(NULL, "Cannot launch ping thread", "Error", MB_OK | MB_ICONHAND);
        }
        // Create a brush with the desired background color
        hbrBkgnd = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
        return (INT_PTR)TRUE;

    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC) wParam;
        HWND hwndStatic = (HWND) lParam;
        if (GetDlgCtrlID(hwndStatic) == IDC_STATIC_ERROR) 
        {
            SetTextColor(hdcStatic, RGB(255, 0, 0)); // Set text color to red
            // Set the background color
            SetBkColor(hdcStatic, GetSysColor(COLOR_BTNFACE));
            return (INT_PTR)hbrBkgnd;
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
