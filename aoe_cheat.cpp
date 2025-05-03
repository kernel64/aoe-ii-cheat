// aoe_cheat.cpp : Entry point of the program

#include <iostream>
#include <Windows.h>
#include <vector>
#include <string>
#include <psapi.h>
#include <tlhelp32.h>
#include <thread>
#include <chrono>

using namespace std;

HANDLE hProcess = NULL;

const wchar_t* processName = L"empires2.exe";
const wchar_t* moduleName = L"empires2.exe";

const int increment = 70;

float wood = 200;
float food = 200;
float gold = 200;
float stones = 200;

DWORD baseAddress;

// Utility function to get the PID of a process by name
DWORD GetProcessIdByName(const wchar_t* processName) {
    PROCESSENTRY32W entry = { sizeof(entry) };
    DWORD pid = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName) == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return pid;
}

// Follows a pointer chain and returns the final address
uintptr_t ReadPointerChain(HANDLE hProcess, uintptr_t baseAddress, const vector<uintptr_t>& offsets) {
    uintptr_t address = baseAddress;

    for (size_t i = 0; i < offsets.size(); ++i) {
        if (!ReadProcessMemory(hProcess, (LPCVOID)address, &address, sizeof(uintptr_t), nullptr)) {
            return 0;
        }
        address += offsets[i];
    }

    return address;
}

// Returns the base address of the module
uintptr_t GetModuleBaseAddress(DWORD pid, const wchar_t* moduleName) {
    uintptr_t baseAddress = 0;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W modEntry;
        modEntry.dwSize = sizeof(modEntry);
        if (Module32FirstW(snapshot, &modEntry)) {
            do {
                if (_wcsicmp(modEntry.szModule, moduleName) == 0) {
                    baseAddress = (uintptr_t)modEntry.modBaseAddr;
                    break;
                }
            } while (Module32NextW(snapshot, &modEntry));
        }
        CloseHandle(snapshot);
    }
    return baseAddress;
}

// Helper function to write memory and print error if needed
bool WriteFloat(HANDLE process, uintptr_t address, float value, const string& label) {
    if (!WriteProcessMemory(process, (LPVOID)address, &value, sizeof(float), nullptr)) {
        cerr << "WriteProcessMemory failed for " << label
            << ". Error code: " << dec << GetLastError() << endl;
        return false;
    }
    return true;
}

// Writes the values to memory
int SetValues(HANDLE hProcess, uintptr_t startAddress, const vector<uintptr_t>& offsets) {
    uintptr_t finalAddress = ReadPointerChain(hProcess, baseAddress + startAddress, offsets);
    if (finalAddress == 0) {
        cerr << "Failed to resolve pointer chain." << endl;
        return 0;
    }

    // Write all resources
    if (!WriteFloat(hProcess, finalAddress, wood, "wood") ||
        !WriteFloat(hProcess, finalAddress - 0x4, food, "food") ||
        !WriteFloat(hProcess, finalAddress + 0x4, stones, "stones") ||
        !WriteFloat(hProcess, finalAddress + 0x8, gold, "gold")) {
        return 0;
    }

    return 1;
}

// Handles CTRL+C or close signals
BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT ||
        signal == CTRL_BREAK_EVENT || signal == CTRL_LOGOFF_EVENT ||
        signal == CTRL_SHUTDOWN_EVENT) {

        cout << "Shutdown signal detected, cleaning up..." << endl;

        if (hProcess != NULL) {
            CloseHandle(hProcess);
            cout << "Process handle closed successfully." << endl;
        }

        return TRUE; // Signal handled
    }

    return FALSE;
}

int main() {
    DWORD pid = GetProcessIdByName(processName);
    if (pid == 0) {
        wcerr << L"Process not found." << endl;
        return 1;
    }

    cout << "AoE II PID: " << pid << endl;

    hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (hProcess == NULL) {
        cerr << "OpenProcess failed. Error code: " << GetLastError() << endl;
        return EXIT_FAILURE;
    }

    baseAddress = GetModuleBaseAddress(pid, moduleName);
    cout << "Base address: 0x" << hex << uppercase << baseAddress << endl;

    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
        cerr << "Failed to set console control handler." << endl;
        return 1;
    }

    uintptr_t startAddress = 0x00341F58;
    vector<uintptr_t> offsets = { 0, 0x4C, 0x4, 0x80, 0x58, 0xA8, 0x4 };

    while (true) {
        if (!SetValues(hProcess, startAddress, offsets)) {
            cerr << "SetValues failed." << endl;
            break;
        }

        cout << "Wood:   " << dec << (int)wood << endl;
        cout << "Food:   " << dec << (int)food << endl;
        cout << "Stones: " << dec << (int)stones << endl;
        cout << "Gold:   " << dec << (int)gold << endl;

        this_thread::sleep_for(chrono::seconds(5));

        wood += increment;
        food += increment;
        gold += increment;
        stones += increment;
    }

    if (hProcess != NULL) {
        CloseHandle(hProcess);
    }

    return EXIT_SUCCESS;
}
