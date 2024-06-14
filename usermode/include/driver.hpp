#pragma once
#include <Windows.h>
#include <cstdint>

namespace driver {
	namespace codes {
		// Driver setup
		constexpr ULONG attach = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x696, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

		// Read process memory
		constexpr ULONG read = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x697, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);

		// Write process memory
		constexpr ULONG write = CTL_CODE(FILE_DEVICE_UNKNOWN, 0x698, METHOD_BUFFERED, FILE_SPECIAL_ACCESS);
	} // namespace codes

	// Shared between kernel mode and user mode
	struct Request {
		HANDLE processId;

		// Our target and buffer for read and write process memory
		PVOID target;
		PVOID buffer;

		SIZE_T size;
		SIZE_T returnSize;;
	};

	bool attachToProcess(HANDLE driverHandle, const DWORD processId) {
		Request r = {};
		r.processId = reinterpret_cast<HANDLE>(processId);

		// Send through to the driver (sends a control code to do the op)
		return DeviceIoControl(driverHandle, codes::attach, &r, sizeof(r), &r, sizeof(r), nullptr, nullptr);
	}

	template <class T>
	T readProcessMemory(HANDLE driverHandle, const std::uintptr_t address) {
		T temp = {};

		Request r;
		r.target = reinterpret_cast<PVOID>(address);
		r.buffer = &temp; // pass address of temp through the buffer to the kernel, the driver will put the result of reading the target into temp
		r.size = sizeof(T);

		return DeviceIoControl(driverHandle, codes::read, &r, sizeof(r), &r, sizeof(r), nullptr, nullptr);
	}

	template <class T>
	void writeProcessMemory(HANDLE driverHandle, const std::uintptr_t address, const T& value) {
		Request r;
		r.target = reinterpret_cast<PVOID>(address);
		r.buffer = (PVOID)&value;
		r.size = sizeof(T);

		return DeviceIoControl(driverHandle, codes::write, &r, sizeof(r), &r, sizeof(r), nullptr, nullptr);
	}
} // namespace driver