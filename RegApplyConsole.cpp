#include <windows.h>
#include <tlhelp32.h>
#include <sddl.h>

#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#pragma comment(linker, "/ENTRY:wWinMainCRTStartup")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "User32.lib")

// Function to retrieve User SID and Username
BOOL GetUserInfo(WCHAR* szSid, DWORD sidSize, WCHAR* szName, DWORD nameSize) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return FALSE;

    PROCESSENTRY32W pe = { sizeof(pe) };
    DWORD dwPid = 0;
    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            if (lstrcmpiW(pe.szExeFile, L"explorer.exe") == 0) {
                dwPid = pe.th32ProcessID;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe));
    }
    CloseHandle(hSnapshot);
    if (dwPid == 0) return FALSE;

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, dwPid);
    if (!hProcess) return FALSE;

    HANDLE hToken = NULL;
    BOOL bRes = FALSE;
    if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
        DWORD dwLen = 0;
        GetTokenInformation(hToken, TokenUser, NULL, 0, &dwLen);
        PTOKEN_USER pUser = (PTOKEN_USER)LocalAlloc(LPTR, dwLen);
        if (pUser) {
            if (GetTokenInformation(hToken, TokenUser, pUser, dwLen, &dwLen)) {
                LPWSTR pSidStr = NULL;
                if (ConvertSidToStringSidW(pUser->User.Sid, &pSidStr)) {
                    if (lstrcpynW(szSid, pSidStr, sidSize) != NULL) {
                        WCHAR szDom[256];
                        DWORD dwU = nameSize, dwD = 256;
                        SID_NAME_USE snu;
                        if (LookupAccountSidW(NULL, pUser->User.Sid, szName, &dwU, szDom, &dwD, &snu)) {
                            bRes = TRUE;
                        }
                    }
                    LocalFree(pSidStr);
                }
            }
            LocalFree(pUser);
        }
        CloseHandle(hToken);
    }
    CloseHandle(hProcess);
    return bRes;
}

// Manual string replacement for wide buffers
WCHAR* ReplaceStr(WCHAR* src, const WCHAR* find, const WCHAR* replace) {
    size_t srcLen = lstrlenW(src);
    size_t findLen = lstrlenW(find);
    size_t repLen = lstrlenW(replace);

    int count = 0;
    WCHAR* p = src;
    while ((p = wcsstr(p, find)) != NULL) { count++; p += findLen; }
    if (count == 0) return src;

    size_t newLen = srcLen + (repLen - findLen) * count + 1;
    WCHAR* dest = (WCHAR*)LocalAlloc(LPTR, newLen * sizeof(WCHAR));
    if (!dest) return src;

    WCHAR* d = dest;
    p = src;
    while (TRUE) {
        WCHAR* next = wcsstr(p, find);
        if (!next) {
            lstrcpyW(d, p);
            break;
        }
        size_t partLen = next - p;
        memcpy(d, p, partLen * sizeof(WCHAR));
        d += partLen;
        lstrcpyW(d, replace);
        d += repLen;
        p = next + findLen;
    }
    return dest;
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR lpCmd, int nShow) {
    // Check for silent argument (/s or -s)
    BOOL bSilent = (wcsstr(lpCmd, L"/s") != NULL || wcsstr(lpCmd, L"-s") != NULL);

    WCHAR szSid[128] = { 0 }, szUserName[256] = { 0 };
    if (!GetUserInfo(szSid, 128, szUserName, 256)) return 1;

    if (!bSilent) {
        WCHAR szConfirm[512];
        wsprintfW(szConfirm, L"Apply policies for:\nUser: %s\nSID: %s?", szUserName, szSid);
        if (MessageBoxW(NULL, szConfirm, L"Registry Deployer", MB_YESNO | MB_ICONQUESTION) != IDYES) return 0;
    }

    HANDLE hFile = CreateFileW(L"main.reg", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (!bSilent) MessageBoxW(NULL, L"Error: main.reg not found.", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    DWORD dwSize = GetFileSize(hFile, NULL);
    BYTE* pRaw = (BYTE*)LocalAlloc(LPTR, dwSize + 2);
    DWORD dwRead;
    if (!ReadFile(hFile, pRaw, dwSize, &dwRead, NULL)) return 1;
    CloseHandle(hFile);

    WCHAR* pWBuf = NULL;
    if (dwSize >= 2 && pRaw[0] == 0xFF && pRaw[1] == 0xFE) {
        pWBuf = (WCHAR*)LocalAlloc(LPTR, dwSize + 2);
        memcpy(pWBuf, pRaw + 2, dwSize - 2);
    }
    else {
        int wLen = MultiByteToWideChar(CP_UTF8, 0, (char*)pRaw, dwSize, NULL, 0);
        pWBuf = (WCHAR*)LocalAlloc(LPTR, (wLen + 1) * sizeof(WCHAR));
        MultiByteToWideChar(CP_UTF8, 0, (char*)pRaw, dwSize, pWBuf, wLen);
    }
    LocalFree(pRaw);

    WCHAR szNewRoot[256];
    lstrcpyW(szNewRoot, L"HKEY_USERS\\");
    lstrcatW(szNewRoot, szSid);

    WCHAR* pFinal = ReplaceStr(pWBuf, L"HKEY_CURRENT_USER", szNewRoot);

    HANDLE hTemp = CreateFileW(L"temp_mod.reg", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (hTemp != INVALID_HANDLE_VALUE) {
        unsigned short bom = 0xFEFF;
        DWORD dwWr;
        const WCHAR* szHeader = L"Windows Registry Editor Version 5.00\r\n\r\n";
        WriteFile(hTemp, &bom, 2, &dwWr, NULL);
        WriteFile(hTemp, szHeader, lstrlenW(szHeader) * 2, &dwWr, NULL);
        WriteFile(hTemp, pFinal, lstrlenW(pFinal) * 2, &dwWr, NULL);
        CloseHandle(hTemp);
    }

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"open";
    sei.lpFile = L"reg.exe";
    sei.lpParameters = L"import temp_mod.reg /reg:64";
    sei.nShow = SW_HIDE;
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;

    if (ShellExecuteExW(&sei)) {
        WaitForSingleObject(sei.hProcess, 15000);
        CloseHandle(sei.hProcess);
    }

    DeleteFileW(L"temp_mod.reg");
    if (pFinal != pWBuf) LocalFree(pFinal);
    LocalFree(pWBuf);

    if (!bSilent) MessageBoxW(NULL, L"Deployment completed successfully.", L"Success", MB_OK | MB_ICONINFORMATION);
    return 0;
}