# DllInjectLoader

Universal DLL injecting loader based on [Detours](https://github.com/microsoft/Detours).

## Usage

```
Usage: loader [options] target
Options:
    /setdebugger      set loader as debugger of target exe with current options
                      in Image File Execution Options
    /unsetdebugger    unset debugger of target in Image File Execution Options
    /with:dllname     launch target with specified DLL injected
                      can be specified multiple times
    /noconf           disable reading withdll.conf in target directory
    /wait | /nowait   sets if loader waits for target process to exit
                      and return its exit code
    
Without /noconf, loader reads DLL names from withdll.conf in target directory 
(ANSI encoded, one DLL name each line). 
DLL names in /with and withdll.conf shoule be basename only (no path separator)
as the names are injected into import table.

For more detailed behavior and limitations, see Detours docs.
```

### Loader Executables

* **loader.exe**: Console subsystem
* **loaderw.exe**: Windows subsystem (no console window)

Use same architecture for loader executable and target executable, or use DLLs with [Detours ABI detecting convention][1] (x86 and amd64 only).

Download prebuilt executables from Actions artifacts: [![build](https://github.com/dantmnf/DllInjectLoader/actions/workflows/build.yml/badge.svg)](https://github.com/dantmnf/DllInjectLoader/actions/workflows/build.yml)

## Example

This repository contains a example that overrides DPI awareness to system awareness, which will make system scale the application when DPI changed (instead of application scaling that gives a pixel perfect view).

The example contains a `version.dll` shim that can be used by DLL hijacking. For some reasons DLL hijacking doesn't work, use `loader /with:version.dll Telegram.exe` instead.

Use `loader /setdebugger Telegram.exe` to set image hijacking for `Telegram.exe`. This will apply to all `Telegram.exe` in the system regardless of location and hijack it to current loader executable. Unset with `loader /unsetdebugger Telegram.exe`.

## Caveats

See [Detours documentation][1] for detailed behavior and limitations.

If you removed the loader executable without cleaning `HKEY_LOACL_MACHINE\Software\Windows NT\CurrentVersion\Image File Execution Options\Telegram.exe` in registry (either with `loader /unsetdebugger` or cleaning it manually), all `Telegram.exe` in the system may fail to run.


[1]: https://github.com/microsoft/Detours/wiki/DetourCreateProcessWithDlls