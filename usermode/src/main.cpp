#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include "client.dll.hpp"
#include "offsets.hpp"
#include "driver.hpp"

static DWORD getProcessId(const wchar_t* processName) {
	DWORD processId = 0;

	// Creates a snapshot of all processes on the system
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
	if (snapshot == INVALID_HANDLE_VALUE) {
		return processId;
	}

	PROCESSENTRY32W entry = {};
	entry.dwSize = sizeof(decltype(entry));

	// Loops through all entries in our snapshot until we find the target
	if (Process32FirstW(snapshot, &entry) == TRUE) {
		if (_wcsicmp(processName, entry.szExeFile) == 0) {
			processId = entry.th32ProcessID;
		}
		else {
			while (Process32NextW(snapshot, &entry) == TRUE) {
				if (_wcsicmp(processName, entry.szExeFile) == 0) {
					processId = entry.th32ProcessID;
					break;
				}
			}
		}
	}

	CloseHandle(snapshot);
	return processId;
}

static std::uintptr_t getModuleBase(const DWORD processId, const wchar_t* moduleName) {
	std::uintptr_t moduleBaseAddress = 0;

	// Create a snapshot of all the target process modules (DLLs)
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
	if (snapshot == INVALID_HANDLE_VALUE) {
		return moduleBaseAddress;
	}

	MODULEENTRY32W entry = {};
	entry.dwSize = sizeof(decltype(entry));

	if (Module32FirstW(snapshot, &entry) == TRUE) {
		if (wcsstr(moduleName, entry.szModule) != nullptr) {
			moduleBaseAddress = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
		}
		else {
			while (Module32NextW(snapshot, &entry) == TRUE) {
				if (wcsstr(moduleName, entry.szModule) != nullptr) {
					moduleBaseAddress = reinterpret_cast<std::uintptr_t>(entry.modBaseAddr);
					break;
				}
			}
		}
	}

	CloseHandle(snapshot);
	return moduleBaseAddress;
}

int main() {
	const DWORD processId = getProcessId(L"cs2.exe");

	if (processId == 0) {
		std::cout << "Failed to find CS2.exe\n";
		std::cin.get();
		return 1;
	}

	const HANDLE driver = CreateFile(L"\\\\.\\Kernel-Driver-CS2", GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (driver == INVALID_HANDLE_VALUE) {
		std::cout << "Failed to create driver handle\n";
		std::cin.get();
		return 1;
	}

	if (driver::attachToProcess(driver, processId) == true) {
		std::cout << "Attachment successful\n";

		if (const std::uintptr_t client = getModuleBase(processId, L"client.dll"); client != 0) {
			std::cout << "Client found.\n";

			while (true) {
				if (GetAsyncKeyState(VK_END)) {
					break;
				}

				const auto localPlayerPawn = driver::readProcessMemory<std::uintptr_t>(driver, client + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn);

				if (localPlayerPawn == 0) {
					continue;
				}

				const auto flags = driver::readProcessMemory<std::uint32_t>(driver, localPlayerPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_fFlags);
				const bool playerInAir = flags & (1 << 0);
				const bool spacePressed = GetAsyncKeyState(VK_SPACE);
				const auto forceJump = driver::readProcessMemory<DWORD>(driver, client + cs2_dumper::offsets::client_dll::dwForceJump);

				if (spacePressed && playerInAir) {
					Sleep(6);
					driver::writeProcessMemory(driver, client + cs2_dumper::offsets::client_dll::dwForceJump, 65537);
				}
				else if (spacePressed && !playerInAir) {
					driver::writeProcessMemory(driver, client + cs2_dumper::offsets::client_dll::dwForceJump, 256);
				}
				else if (!spacePressed && forceJump == 65537) {
					driver::writeProcessMemory(driver, client + cs2_dumper::offsets::client_dll::dwForceJump, 256);
				}				
			}
		}
	}

	CloseHandle(driver);
	return 0;
}