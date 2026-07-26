#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <winternl.h>
#include <iostream>
#include <setupapi.h>
#include <devguid.h>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <tchar.h>
#include <cstdint>
#include <limits>

#pragma comment(lib, "setupapi.lib")

#define SECTOR_SIZE 512
#define MBR_BOOT_CODE_SIZE 446
#define MBR_FULL_SIZE 512

struct stDriveInfo
{
    int nIndex = 0;
    UINT64 uSizeBytes = 0;
    std::wstring szModel;
    std::wstring szPath;
};

/// @brief Performs a hex dump of a buffer to the console
/// @param abBuffer The buffer to dump
/// @param nLength Length of the buffer
/// @param nOffset Starting offset for display
/// @return Number of characters printed
ULONG fnHexdump(const uint8_t* abBuffer, size_t nLength, size_t nOffset = 0)
{
    ULONG nResult = 0;
    for (size_t i = 0; i < nLength; i += 16)
    {
        printf("%08X |", static_cast<unsigned int>(i + nOffset));

        nResult += 16;
        for (size_t j = 0; j < 16; j++)
        {
            if (i + j < nLength)
            {
                nResult += printf(" %02X", abBuffer[i + j]);
            }
            else
            {
                nResult += printf(" 00");
            }
        }

        nResult += printf(" | ");
        for (size_t j = 0; j < 16; j++)
        {
            if (i + j < nLength)
            {
                unsigned char k = abBuffer[i + j];
                unsigned char c = k < 32 || k > 127 ? '.' : k;
                nResult += printf("%c", c);
            }
            else
            {
                nResult += printf(" ");
            }
        }

        nResult += printf("\n");
    }

    return nResult;
}

//// Disk handling class for low-level I/O
class clsDiskHandle
{
public:
    HANDLE m_hFile = INVALID_HANDLE_VALUE;

    clsDiskHandle() = default;

    ~clsDiskHandle()
    {
        if (INVALID_HANDLE_VALUE != m_hFile)
            CloseHandle(m_hFile);
    }

    bool fnbOpen(const std::wstring& szPath, bool bWriteAccess)
    {
        DWORD dwAccess = bWriteAccess ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ;
        m_hFile = CreateFileW(
            szPath.c_str(),
            dwAccess,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_WRITE_THROUGH,
            nullptr
        );

        if (INVALID_HANDLE_VALUE == m_hFile)
        {
            DWORD hError = GetLastError();
            std::wcerr << L"Failed to open " << szPath << L" (error " << hError << L")\n";

            if (ERROR_ACCESS_DENIED == hError)
                std::cerr << " Please Run as Administrator\n";

            return false;
        }

        return true;
    }

    bool fnbReadSectors(LONGLONG nLBA, DWORD nCount, std::vector<uint8_t>& abBuffer)
    {
        LARGE_INTEGER nOffset;
        nOffset.QuadPart = nLBA * SECTOR_SIZE;

        if (!SetFilePointerEx(m_hFile, nOffset, nullptr, FILE_BEGIN))
        {
            std::cerr << "Seek failed (error: " << GetLastError() << ")\n";
            return false;
        }

        // ensure size fits in a DWORD for ReadFile
        size_t expectedSize = static_cast<size_t>(nCount) * SECTOR_SIZE;
        if (expectedSize > static_cast<size_t>(std::numeric_limits<unsigned long>::max()))
        {
            std::cerr << "Requested read is too large\n";
            return false;
        }

        abBuffer.resize(expectedSize);
        DWORD nRead = 0;
        DWORD expected = static_cast<DWORD>(expectedSize);
        if (!ReadFile(m_hFile, abBuffer.data(), expected, &nRead, nullptr) || nRead != expected)
        {
            std::wcerr << L"Read failed (error: " << GetLastError() << L")\n";
            return false;
        }

        return true;
    }

    bool fnbWriteSectors(LONGLONG nLBA, const std::vector<uint8_t>& abBuffer)
    {
        LARGE_INTEGER nOffset;
        nOffset.QuadPart = nLBA * SECTOR_SIZE;

        if (!SetFilePointerEx(m_hFile, nOffset, nullptr, FILE_BEGIN))
        {
            std::wcerr << L"Seek failed (error " << GetLastError() << ")\n";
            return false;
        }

        if (abBuffer.size() > static_cast<size_t>(std::numeric_limits<DWORD>::max()))
        {
            std::wcerr << L"Write buffer too large\n";
            return false;
        }

        DWORD written = 0;
        DWORD expected = static_cast<DWORD>(abBuffer.size());
        if (!WriteFile(m_hFile, abBuffer.data(), expected, &written, nullptr) || written != expected)
        {
            std::wcerr << L"Write failed (error " << GetLastError() << ")\n";
            return false;
        }

        FlushFileBuffers(m_hFile);
        return true;
    }
};

/// @brief Enumerates physical drives on the system
/// @return Vector of drive information structures
std::vector<stDriveInfo> fnListDrives()
{
    std::vector<stDriveInfo> lsDrives;

    for (int i = 0; i < 16; i++)
    {
        std::wstring szPath = L"\\\\.\\PhysicalDrive" + std::to_wstring(i);
        HANDLE hFile = CreateFileW(
            szPath.c_str(),
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        if (INVALID_HANDLE_VALUE == hFile)
            continue;

        stDriveInfo info;
        info.nIndex = i;
        info.szPath = szPath;

        DISK_GEOMETRY_EX geo = {};
        DWORD ret = 0;
        if (DeviceIoControl(hFile, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0, &geo, sizeof(geo), &ret, nullptr))
        {
            info.uSizeBytes = (UINT64)geo.DiskSize.QuadPart;
        }

        STORAGE_PROPERTY_QUERY query = {};
        query.PropertyId = StorageDeviceProperty;
        query.QueryType = PropertyStandardQuery;

        uint8_t abDescriptor[512] = {};
        if (DeviceIoControl(hFile, IOCTL_STORAGE_QUERY_PROPERTY, &query, sizeof(query), abDescriptor, sizeof(abDescriptor), &ret, nullptr))
        {
            auto* descriptor = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(abDescriptor);
            if (descriptor->ProductIdOffset)
            {
                const char* szModel = reinterpret_cast<const char*>(abDescriptor) + descriptor->ProductIdOffset;
                int nLength = MultiByteToWideChar(CP_ACP, 0, szModel, -1, nullptr, 0);
                if (nLength > 0)
                {
                    // ensure size_t is used for string length
                    std::wstring s(static_cast<size_t>(nLength), L'\0');
                    MultiByteToWideChar(CP_ACP, 0, szModel, -1, &s[0], nLength);
                    while (!s.empty() && (s.back() == L' ' || s.back() == L'\0'))
                        s.pop_back();

                    info.szModel = s;
                }
            }
        }

        if (info.szModel.empty())
            info.szModel = L"(Unknown model)";

        lsDrives.push_back(info);
        CloseHandle(hFile);
    }

    return lsDrives;
}

/// @brief Prints the list of detected drives
/// @param lsDrives Vector of drive info
void fnPrintDrives(const std::vector<stDriveInfo>& lsDrives)
{
    std::wcout << L"\nAvailable physical drives:\n";
    std::wcout << std::left << std::setw(6) << L"#" << std::setw(12) << L"Size" << L"Model\n";
    std::wcout << L"------------------------------------------\n";

    for (const auto& drive : lsDrives)
    {
        double gb = static_cast<double>(drive.uSizeBytes) / (1024.0 * 1024.0 * 1024.0);
        std::wstringstream ss;
        ss << std::fixed << std::setprecision(1) << gb << L" GB";
        std::wcout << std::left << std::setw(6) << (L"[" + std::to_wstring(drive.nIndex) + L"]") << std::setw(12) << ss.str() << drive.szModel << L'\n';
    }
}

/// @brief Reads a file into a byte buffer
/// @param szPath Path to the file
/// @param abBuffer Buffer to store data
/// @return True if successful
bool fnbReadFile(const std::string& szPath, std::vector<uint8_t>& abBuffer)
{
    std::ifstream fs(szPath, std::ios::binary | std::ios::ate);
    if (!fs)
    {
        std::cerr << "\tCannot open file: " << szPath << "\n";
        return false;
    }

    std::streampos sp = fs.tellg();
    if (sp == std::streampos(-1))
    {
        std::cerr << "\tCannot determine file size: " << szPath << "\n";
        return false;
    }

    std::streamsize nSize = static_cast<std::streamsize>(sp);
    fs.seekg(0, std::ios::beg);

    if (nSize < 0)
    {
        std::cerr << "\tInvalid file size: " << szPath << "\n";
        return false;
    }

    abBuffer.resize(static_cast<size_t>(nSize));

    if (!fs.read(reinterpret_cast<char*>(abBuffer.data()), nSize))
    {
        std::cerr << "\tRead error: " << szPath << "\n";
        return false;
    }

    return true;
}

/// @brief Validates if a buffer contains a valid MBR signature
/// @param abMBR Buffer containing MBR data
/// @return True if valid
bool fnbValidateMBR(const std::vector<uint8_t>& abMBR)
{
    if (abMBR.size() < SECTOR_SIZE)
        return false;

    return abMBR[510] == 0x55 && abMBR[511] == 0xAA;
}

/// @brief Backs up the current MBR to a file
/// @param szDrivePath Path to the physical drive
/// @param szOutPath Path to the output file
/// @return True if successful
bool fnbBackupMBR(const std::wstring& szDrivePath, const std::string& szOutPath)
{
    std::wcout << L"\n[Backup MBR] Reading sector 0 from " << szDrivePath << L"...\n";

    clsDiskHandle disk;
    if (!disk.fnbOpen(szDrivePath, false))
        return false;

    std::vector<uint8_t> abMBR;
    if (!disk.fnbReadSectors(0, 1, abMBR))
        return false;

    std::cout << "Boot signature: "
        << std::hex << static_cast<int>(abMBR[510]) << " " << static_cast<int>(abMBR[511])
        << std::dec << "\n";

    std::ofstream fs(szOutPath, std::ios::binary);
    if (!fs)
    {
        std::cerr << "Cannot create backup file: " << szOutPath << "\n";
        return false;
    }

    fs.write(reinterpret_cast<char*>(abMBR.data()), static_cast<std::streamsize>(abMBR.size()));
    std::cout << "Saved to: " << szOutPath << "\n";

    return true;
}

/// @brief Writes custom MBR boot code while preserving the partition table
/// @param szDrivePath Path to the physical drive
/// @param szMbrPath Path to the custom MBR binary
/// @return True if successful
bool fnbWriteMBR(const std::wstring& szDrivePath, const std::string& szMbrPath)
{
    std::cout << "\n[Write MBR] Installing custom boot code...\n";

    std::vector<uint8_t> abMBR;
    if (!fnbReadFile(szMbrPath, abMBR))
        return false;

    if (abMBR.size() < MBR_BOOT_CODE_SIZE)
    {
        std::cerr << "Error: " << szMbrPath << " is too small for boot code!\n";
        return false;
    }

    clsDiskHandle disk;
    if (!disk.fnbOpen(szDrivePath, true))
        return false;

    std::vector<uint8_t> abDiskMBR;
    if (!disk.fnbReadSectors(0, 1, abDiskMBR))
        return false;

    std::vector<uint8_t> abMerged(SECTOR_SIZE);
    memcpy(abMerged.data(), abMBR.data(), MBR_BOOT_CODE_SIZE);
    // preserve partition table (offset 0x1BE, 4 entries * 16 bytes = 64 bytes)
    memcpy(abMerged.data() + 0x1BE, abDiskMBR.data() + 0x1BE, 64);
    abMerged[510] = 0x55;
    abMerged[511] = 0xAA;

    if (!disk.fnbWriteSectors(0, abMerged))
        return false;

    std::cout << "Boot code is written (446 bytes), partition table is preserved.\n";
    std::cout << "Boot signature: 0x55 0xAA\n";

    return true;
}

/// @brief Restores MBR from a backup file
/// @param szDrivePath Path to the physical drive
/// @param szBackupFile Path to the backup file
/// @return True if successful
bool fnbRestoreMBR(const std::wstring& szDrivePath, const std::string& szBackupFile)
{
    std::cout << "\n[Restore MBR] Restoring original MBR...\n";
    std::vector<uint8_t> abBackup;

    if (!fnbReadFile(szBackupFile, abBackup))
        return false;

    if (abBackup.size() < SECTOR_SIZE)
    {
        std::cerr << "\tBackup file too small!\n";
        return false;
    }

    clsDiskHandle disk;
    if (!disk.fnbOpen(szDrivePath, true))
        return false;

    abBackup.resize(SECTOR_SIZE);
    if (!disk.fnbWriteSectors(0, abBackup))
        return false;

    std::cout << "\tOriginal MBR is restored successfully.\n";
    return true;
}

/// @brief Validates and displays the current state of the disk's MBR
/// @param szDrivePath Path to the physical drive
/// @return True if successful
bool fnbValidate(const std::wstring& szDrivePath)
{
    std::wcout << L"\n[Validate] Reading disk " << szDrivePath << L"...\n";

    clsDiskHandle disk;
    if (!disk.fnbOpen(szDrivePath, false))
        return false;

    std::vector<uint8_t> abMBR;
    if (!disk.fnbReadSectors(0, 1, abMBR))
        return false;

    std::cout << "\n\tMBR (sector 0) first 64 bytes:\n";
    fnHexdump(abMBR.data(), 64, 0);

    std::cout << "\n\tBoot signature: 0x" << std::hex << static_cast<int>(abMBR[510]) << " 0x" << static_cast<int>(abMBR[511]) << std::dec;
    if (abMBR[510] == 0x55 && abMBR[511] == 0xAA)
        std::cout << " (valid)\n";
    else
        std::cout << " (INVALID)\n";

    std::cout << "\n\tPartition table:\n";
    for (int i = 0; i < 4; i++)
    {
        uint8_t* e = abMBR.data() + 0x1BE + i * 16;
        uint8_t type = e[4];

        // use memcpy to safely read unaligned little-endian values
        uint32_t lba = 0;
        uint32_t sector = 0;
        memcpy(&lba, e + 8, sizeof(lba));
        memcpy(&sector, e + 12, sizeof(sector));

        if (type == 0) continue;

        std::cout << "\t[" << i << "] type=0x" << std::hex << static_cast<int>(type) << " lba=" << std::dec << lba << " sectors=" << sector;
        if (type == 0x07) std::cout << " (NTFS)";
        if (type == 0x0B || type == 0x0C) std::cout << " (FAT32)";
        std::cout << "\n";
    }

    return true;
}

/// @brief Prompts user for confirmation
/// @param szMsg Message to display
/// @return True if user confirmed
bool fnbConfirm(const std::string& szMsg)
{
    std::cout << "\n " << szMsg << " (yes/no): ";
    std::string szAns;
    std::getline(std::cin, szAns);
    return szAns == "yes" || szAns == "YES" || szAns == "y";
}

void fnPrintBanner()
{
    std::cout << R"(
    __  __           ____ _       __      _ __            
   / / / /__  _  __ / __ ) |     / /_____(_) /____  _____ 
  / /_/ / _ \| |/_// __  | | /| / / ___/ / __/ _ \/ ___/ 
 / __  /  __/>  < / /_/ /| |/ |/ / /  / / /_/  __/ /     
/_/ |_/\___/_/|_|/_____/ |__/|__/_/  /_/\__/\___/_/      
                                                          
    )" << std::endl;
    std::cout << "HexBWriter - MBR Bootcode Utility" << std::endl;
    std::cout << "A tool for low-level disk I/O and MBR management." << std::endl;
}

void fnPrintUsage(const char* szProg)
{
    fnPrintBanner();
    std::cout << "\nUsage: " << szProg << " [options]\n\n"
        << "Options:\n"
        << "\t--list                List physical drives\n"
        << "\t--drive N             Select PhysicalDriveN\n"
        << "\t--install <mbr.bin>   Write MBR boot code from file\n"
        << "\t--backup <file>       Backup current MBR to file\n"
        << "\t--restore <file>      Restore MBR from file\n"
        << "\t--validate            Show disk MBR state\n"
        << "\nExamples:\n"
        << "\tHexBWriter.exe --list\n"
        << "\tHexBWriter.exe --drive 0 --install mbr.bin\n"
        << "\tHexBWriter.exe --drive 1 --backup backup.bin\n";
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        fnPrintUsage(argv[0]);
        return 1;
    }

    int nIdxDrive = -1;
    bool bList = false;
    bool bInstall = false;
    bool bBackup = false;
    bool bRestore = false;
    bool bValidate = false;

    std::string szMbrPath;
    std::string szBackupPath;
    std::string szRestorePath;

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--list") bList = true;
        else if (arg == "--drive" && i + 1 < argc) nIdxDrive = std::stoi(argv[++i]);
        else if (arg == "--install" && i + 1 < argc) { bInstall = true; szMbrPath = argv[++i]; }
        else if (arg == "--backup" && i + 1 < argc) { bBackup = true; szBackupPath = argv[++i]; }
        else if (arg == "--restore" && i + 1 < argc) { bRestore = true; szRestorePath = argv[++i]; }
        else if (arg == "--validate") bValidate = true;
    }

    if (bList)
    {
        fnPrintDrives(fnListDrives());
        return 0;
    }

    if (nIdxDrive == -1)
    {
        std::cerr << "Error: No drive selected. Use --drive N\n";
        return 1;
    }

    std::wstring szDrivePath = L"\\\\.\\PhysicalDrive" + std::to_wstring(nIdxDrive);

    if (bBackup)
    {
        fnbBackupMBR(szDrivePath, szBackupPath);
    }

    if (bValidate)
    {
        fnbValidate(szDrivePath);
    }

    if (bInstall || bRestore)
    {
        if (nIdxDrive == 0)
        {
            std::cout << "!!! CRITICAL WARNING !!!\n";
            std::cout << "Drive 0 is your PRIMARY SYSTEM DRIVE.\n";
            std::cout << "Modifying the MBR on this drive may prevent your computer from booting.\n";
            if (!fnbConfirm("Are you absolutely sure you want to proceed?")) return 0;
        }
        else
        {
            std::cout << "!!! WARNING !!!\n";
            std::cout << "This operation is dangerous if the drive contains important data.\n";
            std::cout << "Make sure to backup before running this tool.\n";
            if (!fnbConfirm("Do you want to continue?")) return 0;
        }

        if (bInstall) fnbWriteMBR(szDrivePath, szMbrPath);
        if (bRestore) fnbRestoreMBR(szDrivePath, szRestorePath);
    }

    return 0;
}
