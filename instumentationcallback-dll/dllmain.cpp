#include "pch.h"
#define WIN32_NO_STATUS
#include <windows.h>
#include <Winternl.h>
#include <DbgHelp.h>
#undef WIN32_NO_STATUS
#include <cstdint>
#include <functional>
#pragma comment(lib, "ntdll.lib")
#pragma comment (lib, "imagehlp.lib")
#ifdef _WIN64
#define CALLBACK_VERSION 0
#else
#define CALLBACK_VERSION 1
#endif
using CallbackFn = void(*)();
using PROCESS_INSTRUMENTATION_CALLBACK_INFORMATION = struct _PROCESS_INSTRUMENTATION_CALLBACK_INFORMATION{
    ULONG Version;
    ULONG Reserved;
    CallbackFn Callback;
};
using MEMORY_INFORMATION_CLASS = enum _MEMORY_INFORMATION_CLASS {
    MemoryBasicInformation
};
extern "C" NTSTATUS DECLSPEC_IMPORT NTAPI NtSetInformationProcess(HANDLE, PROCESS_INFORMATION_CLASS, PVOID, ULONG);
extern "C" NTSTATUS DECLSPEC_IMPORT NTAPI NtQueryVirtualMemory(HANDLE, PVOID, MEMORY_INFORMATION_CLASS, PVOID, SIZE_T, PSIZE_T);
extern "C" VOID medium(VOID);
extern "C" uintptr_t hook(uintptr_t R10, uintptr_t RAX/* ... */);
#include <cstdio>
#include <vector>
#include <iostream>
#include"MemoizationSearch.h"
PROCESS_INSTRUMENTATION_CALLBACK_INFORMATION callback = { 0 };
std::atomic_bool flag = false;
thread_local std::mutex mtx;
std::string SymInfoFromAddr(uintptr_t address) {
	uint8_t buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = { 0 };
	const auto symbol_info = (PSYMBOL_INFO)buffer;
	symbol_info->SizeOfStruct = sizeof(SYMBOL_INFO);
	symbol_info->MaxNameLen = MAX_SYM_NAME;
	uintptr_t displacement;
	SymFromAddr(GetCurrentProcess(), address, &displacement, symbol_info);
	return symbol_info->Name;
}
auto& SymInfoFromAddrCache = nonstd::makecached(SymInfoFromAddr,INFINITE);
std::unordered_map<uintptr_t, bool> map;//记录函数是否被hook
uintptr_t hook(uintptr_t R10, uintptr_t RAX/* ... */) {
	DWORD dwThreadId = GetCurrentThreadId();//不进入内核
	auto iter = map.find(dwThreadId);
	bool &flag = iter->second;
	if (!flag) {
		flag = true;
		auto symbol_name = SymInfoFromAddrCache(R10);
		printf("[+] function: %s\n\treturn value: 0x%llx\n\treturn address: 0x%llx\n", symbol_name.c_str(), RAX, R10);
		flag = false;
	}
	return RAX;
}
BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call,LPVOID lpReserved){
    switch (ul_reason_for_call){
    case DLL_PROCESS_ATTACH:
		AllocConsole();
		freopen("CONOUT$", "w", stdout);
		freopen("CONIN$", "r", stdin);
		SymSetOptions(SYMOPT_UNDNAME);
		SymInitialize(GetCurrentProcess(), nullptr, TRUE);
		callback.Reserved = 0;
		callback.Version = CALLBACK_VERSION;
		callback.Callback = medium;
		NtSetInformationProcess(GetCurrentProcess(), (PROCESS_INFORMATION_CLASS)0x28, &callback, sizeof(callback));
		printf("[+] hooked\n");
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

