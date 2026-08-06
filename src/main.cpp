#include <windows.h>
#include <string>
#include <map>
#include <TlHelp32.h>
#include <format>
#include <sstream>
#include <Psapi.h>
#include <locale>
#include <iomanip>
#include <stdexcept>
#include <bitset>
#include <vector>

std::map<uintptr_t, std::vector<byte>> steam_api_patches{
	{0xB68D, {0xB8,0x01, 0x00, 0x00, 0x00}},		// mov eax,1
	{0xB692, {0xC2,0x0C,0x00}},			// ret c
	{0x9669, {0xEB,0x19}},			// jmp 9684+steam_api
	{0x7587, {0xEB,0x35}},			// jmp 75BE+steam_api
	{0x9723, {0x90}},				// nop
	{0x9724, {0x90}},				// nop
	{0x944D, {0x90}},				// nop
	{0x944E, {0x90}},				// nop
	{0x7589, {0x90}},				// nop
	{0x758a, {0x90}},				// nop
	{0x758b, {0x90}},				// nop
};
std::map<uintptr_t, std::vector<byte>> pdd_patches{
	{0xC8250, {0x31,0xC0}},			// xor eax,eax
	{0xC8252, {0xC3} },			// ret
	{0xC8B30, {0x31,0xC0}},			// xor eax,eax
	{0xC8B32, {0xC2,0x08,0x00}},		// ret 8
	{0x1D1D00, {0x31,0xC0}},			// xor eax,eax
	{0x1D1D02, {0xC3}},			// ret
	{0x1D1D0C, {0x90}},			// nop
	{0x1D1D0d, {0x90}},			// nop
	{0xC8D10, {0x31,0xC0}},			// xor eax,eax
	{0xC8D12, {0xC3}},			// ret
	{0x5BFCF4, {0xEB,0x18}},			// jmp pdd+5BFD0E
	{0x618046, {0xE9,0x82,0x00,0x00,0x00}},	// jmp pdd+6180CD
	{0x6180A2, {0xEB,0x29}},			// jmp pdd+6180CD
};
std::map<uintptr_t, std::vector<byte>> mangalore_patches{
	{0x6D40, {0x31,0xC0}},		// xor eax,eax
	{0x6D42, {0xC2,0x10,0x00}},		// ret 10
};

#define EXIT_WITH_ERROR( e ) { MessageBoxA(0, e, "Error!", MB_ICONERROR); return 1; }
#define EXIT_WITH_ERROR_NULL( e ) { MessageBoxA(0, e, "Error!", MB_ICONERROR); return NULL; }
#define PRINT_GET_LAST_ERROR() { std::string t = "Error: " + std::to_string(GetLastError()); MessageBoxA(0, t.c_str(), "Error!", MB_ICONERROR); return NULL; }

HMODULE GetModule(DWORD pid, const wchar_t* moduleName) {
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
	if (hSnapshot == INVALID_HANDLE_VALUE) {
		EXIT_WITH_ERROR_NULL("[-] CreateToolhelp32Snapshot failed.");
	}

	MODULEENTRY32W entry;
	entry.dwSize = sizeof(MODULEENTRY32W);

	if (!Module32FirstW(hSnapshot, &entry)) {;
		EXIT_WITH_ERROR_NULL("[-] Module32FirstW returned false.");
	}

	do {
		if (_wcsicmp(entry.szModule, moduleName) == 0) {
			CloseHandle(hSnapshot);
			return entry.hModule;
		}
	} while (Module32NextW(hSnapshot, (LPMODULEENTRY32W)(&entry)));

	CloseHandle(hSnapshot);
	return NULL;
}

int main(void) {
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	wchar_t szWorkDir[] = L"D:\\SteamLibrary\\steamapps\\common\\City Car Driving\\bin\\win32";
	if (!CreateProcessW(
		L"D:\\SteamLibrary\\steamapps\\common\\City Car Driving\\bin\\win32\\starter.exe",
		NULL,
		NULL,
		NULL,
		0,
		DEBUG_PROCESS,
		NULL,
		szWorkDir,
		&si,
		&pi
	)) {
		EXIT_WITH_ERROR("[-] Failed to create process.");
	}

	DEBUG_EVENT dEvent;

	time_t start_time = time(NULL);
	byte libsCount = 0;
	while (1) {
		if (!WaitForDebugEvent(&dEvent, INFINITE)) {
			EXIT_WITH_ERROR("[-] Failed to wait for debug event.");
		}

		
		if (dEvent.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT) {
			wchar_t szName[MAX_PATH];
			if (GetFinalPathNameByHandle(dEvent.u.LoadDll.hFile, szName, MAX_PATH, VOLUME_NAME_DOS))
			{
				std::wstring ws(szName);
				if (ws.find(L"steam_api.dll") != std::wstring::npos) {
					uintptr_t pSteamApi = (uintptr_t)dEvent.u.LoadDll.lpBaseOfDll;
					for (const auto& [address, patch] : steam_api_patches) {
						if (!WriteProcessMemory(pi.hProcess, (LPVOID)(pSteamApi + address), patch.data(), patch.size(), NULL)) {
							EXIT_WITH_ERROR("[-] Failed to patch steam_api");
						}
					}
					libsCount++;
				}

				if (ws.find(L"pdd.dll") != std::wstring::npos) {
					uintptr_t pPdd = (uintptr_t)dEvent.u.LoadDll.lpBaseOfDll;
					for (const auto& [address, patch] : pdd_patches) {
						if (!WriteProcessMemory(pi.hProcess, (LPVOID)(pPdd + address), patch.data(), patch.size(), NULL)) {
							EXIT_WITH_ERROR("[-] Failed to patch pdd");
						}
					}
					libsCount++;
				}

				if (ws.find(L"mangalore.dll") != std::wstring::npos) {
					uintptr_t pMangalore = (uintptr_t)dEvent.u.LoadDll.lpBaseOfDll;
					for (const auto& [address, patch] : mangalore_patches) {
						if (!WriteProcessMemory(pi.hProcess, (LPVOID)(pMangalore + address), patch.data(), patch.size(), NULL)) {
							EXIT_WITH_ERROR("[-] Failed to patch mangalore");
						}
					}
					libsCount++;
				}
			}
			else {
				PRINT_GET_LAST_ERROR();
			}
			CloseHandle(dEvent.u.LoadDll.hFile);
			
		}
		ContinueDebugEvent(dEvent.dwProcessId, dEvent.dwThreadId, DBG_CONTINUE);

		if (difftime(time(NULL), start_time) > 10 || libsCount==3) {
			break;
		}
	}

	if (!DebugActiveProcessStop(pi.dwProcessId)) {
		EXIT_WITH_ERROR("[-] Failed to stop debugging.");
	}

	WaitForSingleObject(pi.hProcess, INFINITE);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	return 0;
}