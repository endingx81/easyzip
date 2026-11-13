#include <windows.h>
#include <stdio.h>
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved) {
    return TRUE;
}
extern "C" __declspec(dllexport) void Test() {
    printf("Hello from DLL!\n");
}
