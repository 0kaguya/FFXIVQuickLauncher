////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 头文件
#include "dllmain.h"
#include <cstdio>
#include <filesystem>
#include <Windows.h>
#include <iostream>
#include <string.h>

#include "shlwapi.h"
#pragma comment(lib, "shlwapi")

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 原函数地址指针
int (*pfnSDOLGetModule)(UINT64 *a, UINT64 *b);
int (*pfnSDOLInitialize)(UINT64 *a);
int (*pfnSDOLTerminal)();
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const LPCWSTR title = TEXT("DalamudLoginEntry64");
// AheadLib 命名空间
namespace AheadLib
{
    HMODULE m_hModule = NULL;  // 原始模块句柄
    DWORD m_dwReturn[3] = {0}; // 原始函数返回地址

    // 获取原始函数地址
    FARPROC WINAPI GetAddress(PCSTR pszProcName)
    {
        FARPROC fpAddress;
        CHAR szProcName[16];
        TCHAR tzTemp[MAX_PATH];

        fpAddress = GetProcAddress(m_hModule, pszProcName);
        printf_s("%s@%p\n", pszProcName, fpAddress);

        if (fpAddress == NULL)
        {
            if (HIWORD(pszProcName) == 0)
            {
                wsprintfA(szProcName, "%d", pszProcName);
                pszProcName = szProcName;
            }

            wsprintf(tzTemp, TEXT("Can not find %s"), pszProcName);
            MessageBox(NULL, tzTemp, title, MB_ICONSTOP);
            ExitProcess(-2);
        }

        return fpAddress;
    }

    // 初始化原始函数地址指针
    inline VOID WINAPI InitializeAddresses()
    {
        pfnSDOLGetModule = (int (*)(UINT64 *, UINT64 *))GetAddress("SDOLGetModule");
        pfnSDOLInitialize = (int (*)(UINT64 *))GetAddress("SDOLInitialize");
        pfnSDOLTerminal = (int (*)())GetAddress("SDOLTerminal");
    }

    // 加载原始模块
    inline BOOL WINAPI Load(HMODULE hMod)
    {
        TCHAR tzPath[MAX_PATH];
        TCHAR tzTemp[MAX_PATH * 2];
        GetModuleFileName(hMod, tzPath, MAX_PATH);
        PathRemoveFileSpec(tzPath);
        PathAppend(tzPath, TEXT("sdologinentry64.sdo.dll"));
        printf_s("%ls\n", tzPath);
        m_hModule = LoadLibrary(tzPath);
        if (m_hModule == NULL)
        {
            wsprintf(tzTemp, TEXT("Can not load %s"), tzPath);
            MessageBox(NULL, tzTemp, title, MB_ICONSTOP);
            ExitProcess(-2);
        }
        else
        {
            InitializeAddresses();
        }

        return (m_hModule != NULL);
    }

    // 释放原始模块
    inline VOID WINAPI Free()
    {
        if (m_hModule)
        {
            FreeLibrary(m_hModule);
        }
    }
}
using namespace AheadLib;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FILE *fpstdin = stdin, *fpstdout = stdout, *fpstderr = stderr;
void SetupConsole()
{
    AllocConsole();
    freopen_s(&fpstdin, "CONIN$", "r", stdin);
    freopen_s(&fpstdout, "CONOUT$", "w", stdout);
    freopen_s(&fpstderr, "CONOUT$", "w", stderr);
}

BOOL IsSdo = true;

// Client::System::Framework::GameWindow
// [FieldOffset(0x00)] public ulong ArgumentCount;
// [FieldOffset(0x08)] public byte** Arguments; // string argList[ArgumentCount]
// [FieldOffset(0x61)] public byte Flag;
// 0x88 jb
// [FieldOffset(0xA0)] public byte* SessionId
// [FieldOffset(0xA8)] public byte* SndaID //AID
// [FieldOffset(0xB8)] public byte* cmdLine prams
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 入口函数
BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, PVOID pvReserved)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
#ifdef _DEBUG
        SetupConsole();
#endif // _DEBUG
        auto cmd = GetCommandLineW();
        printf_s("cmdline=%ls \n", cmd);
        printf_s("%s", "####################IMPORTANT!!!!!######################\n");
        printf_s("%s", "Please blur the DEV.TestSID and XL.SndaId in the following text when you send a screenshot to others.\n");
        printf_s("%s", "########################################################\n");
        if (!wcsstr(cmd, L"//") && !wcsstr(cmd, L"DEV.TestSID"))
        {
            IsSdo = true;
            return Load(hModule);
        }
        else
        {
            IsSdo = false;
        }
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        Free();
    }

    return TRUE;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 导出函数
extern "C" __declspec(dllexport) int SDOLGetModule(UINT64 *a, UINT64 *jb)
{
    printf_s("%s\n", "SDOLGetModule");
    printf_s("%p\n", a);
    printf_s("%p\n", jb);

    if (IsSdo)
    {
        return pfnSDOLGetModule(a, jb);
    }

    //.text:0000000140058DE7 48 89 6C 24 30                                mov[rsp + 28h + arg_0], rbp
    //.text:0000000140058DEC 48 8B CB                                      mov     rcx, rbx
    //.text:0000000140058DEF 48 89 74 24 38                                mov[rsp + 28h + arg_8], rsi
    //.text:0000000140058DF4 48 89 7C 24 40                                mov[rsp + 28h + arg_10], rdi
    //.text:0000000140058DF9 E8 32 FD FF FF                                call    sub_140058B30
    //.text:0000000140058DFE 48 83 7B 20 00                                cmp     qword ptr[rbx + 20h], 0
    //  别惦记你这破勾八COM了
    // rbx+20 就是jb
    //不置零后面会执行一堆乱七八糟东西，大概率crash
    *jb = 0;
    // jb - 0x20 + 0x38 : token
    // jb - 0x20 + 0x40 : sndID
    // jb - 0x20 + 0x50 : cmdLine prams

    auto pSID = jb - 0x20 / 8 + 0x38 / 8;
    auto pSndaId = jb - 0x20 / 8 + 0x40 / 8;

    auto zero = jb - 0x20 / 8 - 0x68 / 8;
    auto args= (char**)*(zero + 0x8 / 8);
    for (auto i = 0; i < *(int32_t*)zero; i++)
    {
        if (strstr(args[i], "DEV.TestSID=")) *pSID = (UINT64)args[i] + 12;
        if (strstr(args[i], "XL.SndaId=")) *pSndaId = (UINT64)args[i] + 10;
    }
    //printf_s("%u\n", *(uint32_t*)zero);
    printf_s("SessionId@%p\n", pSID);
    printf_s("SndaId@%p\n", pSndaId);

    return 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 导出函数
extern "C" __declspec(dllexport) int SDOLTerminal()
{
    printf_s("%s\n", "SDOLTerminal");
    auto ret = 1;
    if (IsSdo)
    {
        ret = pfnSDOLTerminal();
    }
    return ret;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 导出函数
extern "C" __declspec(dllexport) int SDOLInitialize(UINT64 *a)
{
    printf_s("%s\n", "SDOLInitialize");
    auto ret = 0;
    if (IsSdo)
    {
#if _DEBUG
        // MessageBox(NULL, TEXT("垃圾盛趣，溜了"), title, MB_ICONSTOP);
        // SDOLTerminal();
#endif
        ret = pfnSDOLInitialize(a);
        printf_s("%x\n", ret);
        if (ret == 0xFFFFFFFF)
        {
            MessageBox(NULL, TEXT("Failed to init COM component of sdoentry64"), title, MB_ICONSTOP);
            SDOLTerminal();
            ExitProcess(-2);
        }
    }
    return ret;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
