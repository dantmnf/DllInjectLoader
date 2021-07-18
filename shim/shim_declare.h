/* This file will be included twice, */
/* provided different definition of SHIM() macro.*/

SHIM(GetFileVersionInfoA)
SHIM(GetFileVersionInfoByHandle)
SHIM(GetFileVersionInfoExA)
SHIM(GetFileVersionInfoExW)
SHIM(GetFileVersionInfoSizeA)
SHIM(GetFileVersionInfoSizeExA)
SHIM(GetFileVersionInfoSizeExW)
SHIM(GetFileVersionInfoSizeW)
SHIM(GetFileVersionInfoW)
SHIM(VerFindFileA)
SHIM(VerFindFileW)
SHIM(VerInstallFileA)
SHIM(VerInstallFileW)
SHIM(VerLanguageNameA)
SHIM(VerLanguageNameW)
SHIM(VerQueryValueA)
SHIM(VerQueryValueW)

#ifdef SHIM_DECLARE
/* included in global scope */
#include <windows.h>
static HMODULE LoadTargetLibrary() {
    wchar_t *dllPath = (wchar_t*)HeapAlloc(GetProcessHeap(), 0, 65536);
    if (GetSystemDirectoryW(dllPath, 32768) != 0) {
        lstrcatW(dllPath, L"\\version.dll");
        HMODULE hDll = LoadLibraryW(dllPath);
        HeapFree(GetProcessHeap(), 0, dllPath);
        return hDll;
    }
    return NULL;
}

#endif

#ifdef SHIM_FILL
/* included in function scope */

#endif
