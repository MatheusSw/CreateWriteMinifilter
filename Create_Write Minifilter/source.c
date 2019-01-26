#include <dontuse.h>
#include <fltKernel.h>
#include <ntddk.h>

#pragma prefast(disable:__WARNING_ENCODE_MEMBER_FUNCTION_POINTER, "Not valid for kernel mode drivers")

typedef char* (*pPsGetProcessImageFileName)(PEPROCESS process);
pPsGetProcessImageFileName PsGetProcessImageFileName = NULL;

const char * TargetProcessName = NULL;
PFLT_FILTER FilterHandle = 0;

FLT_PREOP_CALLBACK_STATUS FilePreCreate(
	PFLT_CALLBACK_DATA Data,
	PCFLT_RELATED_OBJECTS FltObjects,
	PVOID *CompletionContext
)
{
	UNREFERENCED_PARAMETER(Data);
	UNREFERENCED_PARAMETER(CompletionContext);

	if (FltObjects->FileObject) {
		char * ProcessName = NULL;
		ProcessName = PsGetProcessImageFileName(PsGetCurrentProcess());
		PFILE_OBJECT file = FltObjects->FileObject;
		if (strstr(ProcessName, TargetProcessName) != NULL) {
			KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "%s is creating [ %ws ]\n\n", ProcessName, file->FileName.Buffer));
		}
	}
	return FLT_PREOP_SUCCESS_NO_CALLBACK;  //Can be FLT_PREOP_SUCCESS_WITH_CALLBACK if you want to call the PosOp
}

FLT_PREOP_CALLBACK_STATUS FilePreWrite(
	PFLT_CALLBACK_DATA Data,
	PCFLT_RELATED_OBJECTS FltObjects,
	PVOID *CompletionContext
) {
	UNREFERENCED_PARAMETER(Data);

	UNREFERENCED_PARAMETER(CompletionContext);

	if (FltObjects->FileObject) {
		char * ProcessName = NULL;
		ProcessName = PsGetProcessImageFileName(PsGetCurrentProcess());
		if (strstr(ProcessName, TargetProcessName) != NULL) {
			PFILE_OBJECT file = FltObjects->FileObject;
			KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "%s is writing to [ %ws ]\n\n", ProcessName, file->FileName.Buffer));
		}
	}
	return FLT_PREOP_SUCCESS_NO_CALLBACK;  //Can be FLT_PREOP_SUCCESS_WITH_CALLBACK if you want to call the PosOp
}

const FLT_OPERATION_REGISTRATION FltCallbacks[3] = {
	{
		IRP_MJ_CREATE,
		0,
		FilePreCreate,
		NULL,
		NULL
	},
	{
		IRP_MJ_WRITE,
		0,
		FilePreWrite,
		NULL,
		NULL
	},
	{IRP_MJ_OPERATION_END}
};

NTSTATUS
FilterUnload(
	FLT_FILTER_UNLOAD_FLAGS Flags
)
{
	UNREFERENCED_PARAMETER(Flags);
	FltUnregisterFilter(FilterHandle);
	return STATUS_SUCCESS;
}


NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS status = STATUS_SUCCESS;

	UNICODE_STRING PsGetProcessImageFileNameStr = { 0 };
	RtlInitUnicodeString(&PsGetProcessImageFileNameStr, L"PsGetProcessImageFileName");

	PsGetProcessImageFileName = (pPsGetProcessImageFileName)MmGetSystemRoutineAddress(&PsGetProcessImageFileNameStr);
	if (PsGetProcessImageFileName == NULL) {
		KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[ PsGetProcessImageFileName returned null ] \n"));
		return STATUS_UNSUCCESSFUL;
	}
	TargetProcessName = "notepad.exe"; //Process to be monitored

	FLT_REGISTRATION FilterRegistration = { 0 };
	FilterRegistration.Size = sizeof(FLT_REGISTRATION);
	FilterRegistration.Version = FLT_REGISTRATION_VERSION;
	FilterRegistration.Flags = 0;
	FilterRegistration.ContextRegistration = 0;
	FilterRegistration.OperationRegistration = FltCallbacks;
	FilterRegistration.FilterUnloadCallback = FilterUnload;

	status = FltRegisterFilter(
		DriverObject,
		&FilterRegistration,
		&FilterHandle);

	if (!NT_SUCCESS(status)) {
		KdPrintEx((DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "Erro ao registrar o MiniFilter\n"));
		FltUnregisterFilter(FilterHandle);
		return status;
	}
	return status;
}