// netavailw.cpp : Program to test the health of a network by periodically
// pinging a host.  The results are displayed in a window, and logged to a file.
// Mark Riordan  2024-05-14 (also see the recent netavail.go, which isn't reliable)

#include "framework.h"
#include "netavailw.h"
#include <iphlpapi.h>
#include <icmpapi.h>
#include <stdio.h>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
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

struct struct_settings {
    std::string strRemoteIP = "8.8.8.8";
    long        msBadPing = 400;
    DWORD       msSleep = 10000;
} Settings;

std::string strHostname;


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

void SetErrorText(const char* msg)
{
    //std::string strFull = GetTimeStr() + " ";
    //strFull += msg;
    SetDlgItemText(hDlgGlobal, IDC_STATIC_ERROR, msg);
}

std::vector<std::string> splitIP(const std::string& s) {
    std::vector<std::string> result;
    std::istringstream iss(s);

    for (std::string token; std::getline(iss, token, '.'); ) {
        result.push_back(std::move(token));
    }

    return result;
}

std::vector<std::string> EnumerateLocalIPs() {
    std::vector<std::string> vectIPs;
    IP_ADAPTER_INFO AdapterInfo[16];       // Allocate information for up to 16 NICs
    DWORD dwBufLen = sizeof(AdapterInfo);  // Save memory size of buffer

    do {
        DWORD dwStatus = GetAdaptersInfo(      // Call GetAdapterInfo
            AdapterInfo,                 // [out] buffer to receive data
            &dwBufLen);                  // [in] size of receive data buffer
        if (dwStatus != ERROR_SUCCESS) {    // Check for errors
            //printf("GetAdaptersInfo failed with error: %d\n", dwStatus);
            break;
        }

        PIP_ADAPTER_INFO pAdapterInfo = AdapterInfo;// Contains pointer to current adapter info
        do {
            //printf("\nAdapter name: %s\n", pAdapterInfo->AdapterName);
            //printf("Adapter description: %s\n", pAdapterInfo->Description);
            //printf("Adapter IP address: %s\n", pAdapterInfo->IpAddressList.IpAddress.String);
            vectIPs.push_back(std::string(pAdapterInfo->IpAddressList.IpAddress.String));
            // Process next adapter
            pAdapterInfo = pAdapterInfo->Next;
        } while (pAdapterInfo);  // Terminate if last adapter
    } while (false);
    return vectIPs;
}

// Return the machine's local IP address.  Use the IP address most likely
// to be actually used for the pinging.  The result is a numeric dotted IP.
std::string GetLikelyLocalIP()
{
    std::string strIP;
    std::vector<std::string> vectIPs = EnumerateLocalIPs();
    for (std::vector<std::string>::iterator iter = vectIPs.begin();
        vectIPs.end() != iter; iter++) {
        std::vector<std::string> vectOctets = splitIP(*iter);
        // Check the octets to look for a pattern that is likely NOT one
        // of the weird IP addresses assigned by software like VMware. 
        if (vectOctets.size() == 4 && vectOctets[0] == "192" && vectOctets[1] == "168") {
            if (strIP.length() == 0) {
                strIP = *iter;
            } else if (vectOctets[3] != "1") {
                strIP = *iter;
            }
        }
    }
    return strIP;
}

// Log a record to the log file.
// Records look like:
// timestamp,action,hostname,localIP,remoteIP,details
void LogToFile(std::string action, std::string details)
{
    std::string fullMsg = GetTimeStr() + "," + action;
    fullMsg += "," + strHostname;
    // We recompute the local IP address each time, because the user
    // may have connected to a different network during program execution.
    fullMsg += "," + GetLikelyLocalIP();
    fullMsg += "," + Settings.strRemoteIP;
    fullMsg += "," + details;
    std::ofstream file;

    // Open the file in append mode
    file.open("netavailw.csv", std::ios_base::app);

    if (file.is_open()) {
        // Write the string to the file
        file << fullMsg << std::endl;

        // Close the file
        file.close();
    } else {
        // error
    }
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

            sprintf_s(szbuf, "%ld", msPing);
            LogToFile("ping", szbuf);
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
            LogToFile("error", strError);
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
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            PostQuitMessage(0);
            return (INT_PTR)TRUE;
        } else if (LOWORD(wParam) == IDC_BUTTON_PROBLEMS) {
            MessageBox(hDlg, "Viewing problems", "List of warning conditions", MB_OK);
        }
        break;

    case WM_CLOSE:
        LogToFile("stop", "");
        EndDialog(hDlg, 0);
        return TRUE;
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

   // Record the local computer's name.  This will help log analysis.
   char szComputerName[64];
   DWORD size = sizeof(szComputerName);
   GetComputerName(szComputerName, &size);
   strHostname = szComputerName;

   LogToFile("start", "");

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
