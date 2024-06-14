#include <ntifs.h>

extern "C" {
	NTKERNELAPI NTSTATUS IoCreateDriver(PUNICODE_STRING DriverName, 
										PDRIVER_INITIALIZE InitializationFunction);

	NTKERNELAPI NTSTATUS MmCopyVirtualMemory(PEPROCESS SourceProcess, PVOID SourceAddress, 
										     PEPROCESS TargetProcess, PVOID TargetAddress, 
											 SIZE_T BufferSize, KPROCESSOR_MODE PreviousMode, 
											 PSIZE_T ReturnSize);
}

void debugPrint(PCSTR text) {
#ifndef DEBUG
	UNREFERENCED_PARAMETER(text);
#endif 
	KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, text));
}

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

	NTSTATUS create(PDEVICE_OBJECT deviceObject, PIRP irp) {
		UNREFERENCED_PARAMETER(deviceObject);

		IoCompleteRequest(irp, IO_NO_INCREMENT);

		return irp->IoStatus.Status;
	}

	NTSTATUS close(PDEVICE_OBJECT deviceObject, PIRP irp) {
		UNREFERENCED_PARAMETER(deviceObject);

		IoCompleteRequest(irp, IO_NO_INCREMENT);

		return irp->IoStatus.Status;
	}

	// Needed to send a msg from usermode to kernel mode w/ IOCTL
	NTSTATUS deviceControl(PDEVICE_OBJECT deviceObject, PIRP irp) {
		UNREFERENCED_PARAMETER(deviceObject);

		debugPrint("[+] Device control called\n");

		NTSTATUS status = STATUS_UNSUCCESSFUL;

		// We need this to determine which control code was passed through (attach, read, write)
		PIO_STACK_LOCATION stackIrp = IoGetCurrentIrpStackLocation(irp);

		// Access the request object sent from usermode
		auto request = reinterpret_cast<Request*>(irp->AssociatedIrp.SystemBuffer);

		
		if (stackIrp == nullptr || request == nullptr) {
			IoCompleteRequest(irp, IO_NO_INCREMENT);
			return status;
		}

		// The target process we want to read/write memory to and from
		static PEPROCESS targetProcess = nullptr;

		// Code that we passed in from usermode
		const ULONG controlCode = stackIrp->Parameters.DeviceIoControl.IoControlCode;
		switch (controlCode) {
			case codes::attach:
				status = PsLookupProcessByProcessId(request->processId, &targetProcess);
				break;
			case codes::read:
				if (targetProcess != nullptr) {
					status = MmCopyVirtualMemory(targetProcess, request->target, PsGetCurrentProcess(), request->buffer, request->size, KernelMode, &request->returnSize);
				}
				break;
			case codes::write:
				if (targetProcess != nullptr) {
					status = MmCopyVirtualMemory(PsGetCurrentProcess(), request->buffer, targetProcess, request->target, request->size, KernelMode, &request->returnSize);
				}
				break;
			default:
				break;
		}

		irp->IoStatus.Status = status;
		irp->IoStatus.Information = sizeof(Request);

		IoCompleteRequest(irp, IO_NO_INCREMENT);

		return status;
	}
} // namespace driver

// Our "real" entry point with manual mapping
NTSTATUS DriverMain(PDRIVER_OBJECT driverObject, PUNICODE_STRING registryPath) {
	UNREFERENCED_PARAMETER(registryPath);

	UNICODE_STRING deviceName = {};
	RtlInitUnicodeString(&deviceName, L"\\Device\\Kernel-Driver-CS2");

	// Create driver device object
	PDEVICE_OBJECT deviceObject = nullptr;
	NTSTATUS status = IoCreateDevice(driverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN,
									 FILE_DEVICE_SECURE_OPEN, FALSE, &deviceObject);

	if (status != STATUS_SUCCESS) {
		debugPrint("[-] Failed to create driver device.\n");
		return status;
	}

	debugPrint("[+] Driver device creation successful.\n");

	UNICODE_STRING symbolicLink = {};
	RtlInitUnicodeString(&symbolicLink, L"\\DosDevices\\Kernel-Driver-CS2");

	status = IoCreateSymbolicLink(&symbolicLink, &deviceName);
	if (status != STATUS_SUCCESS) {
		debugPrint("[-] Failed to establish a symbolic link.\n");
		return status;
	}

	debugPrint("[+] Driver symbolic link successfully established.\n");

	// Allow us to send small amounts of data between UM/KM
	SetFlag(deviceObject->Flags, DO_BUFFERED_IO);

	// Set the driver handlers to our functions with our custom logic
	driverObject->MajorFunction[IRP_MJ_CREATE] = driver::create;
	driverObject->MajorFunction[IRP_MJ_CREATE] = driver::close;
	driverObject->MajorFunction[IRP_MJ_CREATE] = driver::deviceControl;

	// Device is now initialized
	ClearFlag(deviceObject->Flags, DO_DEVICE_INITIALIZING);

	debugPrint("[+] Driver initialized successfully.\n");

	return status;
}

// KdMapper calls this "entry point" but the parameters will be null
NTSTATUS DriverEntry() {
	debugPrint("Printing from the kernel\n");

	UNICODE_STRING driverName = {};
	RtlInitUnicodeString(&driverName, L"\\Driver\\Kernel-Driver-CS2");

	return IoCreateDriver(&driverName, &DriverMain);
}