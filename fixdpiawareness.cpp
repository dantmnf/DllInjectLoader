#include <utility>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <detours.h>
#include <strsafe.h>

void AttachCreateWindow();

BOOL WINAPI DllMain(HINSTANCE hMod, DWORD fdwReason, LPVOID lpvReserved) {
    if (DetourIsHelperProcess()) return TRUE;
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hMod);
        AttachCreateWindow();
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

static decltype(CreateWindowExW)* realCreateWindowExW;

template<typename Invalid> struct MyCreateWindowExWTemplate;
template<typename... Args>
struct MyCreateWindowExWTemplate<HWND(WINAPI)(Args...)> {
    static HWND WINAPI func(Args... args) {
        SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
        return realCreateWindowExW(std::forward<Args>(args)...);
    }
};
static constexpr auto MyCreateWindowExW = MyCreateWindowExWTemplate<decltype(CreateWindowExW)>::func;

static void AttachCreateWindow() {
    auto user32 = GetModuleHandleW(L"user32.dll");
    if (!user32) return;
    realCreateWindowExW = reinterpret_cast<decltype(realCreateWindowExW)>(GetProcAddress(user32, "CreateWindowExW"));
    if (!realCreateWindowExW) return;
    

    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(reinterpret_cast<void**>(&realCreateWindowExW), reinterpret_cast<void*>(MyCreateWindowExW));
    DetourTransactionCommit();
}
