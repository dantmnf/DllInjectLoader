#include <cstdint>
#include <cstdio>
#include <cwctype>
#include <string>
#include <vector>
#include <format>
#include <optional>
#include <fstream>
#include <filesystem>
#include <windows.h>
#include <detours.h>

using namespace std::string_literals;
using namespace std::string_view_literals;

static const wchar_t* w32strerror(DWORD err) {
    __declspec(thread) static wchar_t buf[4096];
    FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, buf, sizeof(buf), NULL);
    return buf;
}

static inline void throw_win32_error(LONG err) {
    if (err == ERROR_SUCCESS) return;
    throw std::system_error(std::error_code(err, std::system_category()));
}

static size_t GetNextArgumentItemLength(const wchar_t *start) {
	int inquote = 0;
	size_t result = 0;
	int end = 0;
	wchar_t next;
	do {
		switch (*start) {
			case L'"':
				inquote = !inquote;
				break;
			case L' ':
			case L'\t':
				if (!inquote) end = 1;
				break;
			case L'\\':
				next = *(++start);
				if (!next)
					end = 1;
				else
					result++;
				break;
			case 0:
				end = 1;
				break;
		}
		if (!end) result++;
		start++;
	} while (!end);
	return result;
}

static std::optional<std::wstring_view> GetNextArgumentItem(const wchar_t* &next) {
	size_t l = GetNextArgumentItemLength(next);
	if (!l) return {};
	std::wstring_view result(next, l);
	next += l;
	while(*next == L' ' || *next == L'\t') next++;
	return result;
}

std::string u16toacp(std::wstring_view u16str) {
    auto len = WideCharToMultiByte(CP_ACP, 0, u16str.data(), u16str.size(), nullptr, 0, nullptr, nullptr);
    std::string result(len, 0);
    WideCharToMultiByte(CP_ACP, 0, u16str.data(), u16str.size(), result.data(), result.size(), nullptr, nullptr);
    return result;
}

std::wstring acptou16(std::string_view acpstr) {
    auto len = MultiByteToWideChar(CP_ACP, 0, acpstr.data(), acpstr.size(), nullptr, 0);
    std::wstring result(len, 0);
    MultiByteToWideChar(CP_ACP, 0, acpstr.data(), acpstr.size(), result.data(), result.size());
    return result;
}

std::string_view ltrim(std::string_view s) {
    s.remove_prefix(std::distance(s.cbegin(), std::find_if(s.cbegin(), s.cend(),
         [](int c) {return !std::isspace(c);})));

    return s;
}

std::string_view rtrim(std::string_view s) {
    s.remove_suffix(std::distance(s.crbegin(), std::find_if(s.crbegin(), s.crend(),
        [](int c) {return !std::isspace(c);})));

    return s;
}

std::string_view trim(std::string_view s) {
    return ltrim(rtrim(s));
}

std::wstring tolower(std::wstring_view s) {
    std::wstring result(s);
    std::transform(result.begin(), result.end(), result.begin(), std::towlower);
    return result;
}

static std::wstring argv0;
static bool hasConsole;

static void show_message(std::wstring_view message) {
#ifdef CONSOLE
    if (hasConsole) {
        WriteConsoleW(GetStdHandle(STD_OUTPUT_HANDLE), message.data(), message.size(), nullptr, nullptr);
    } else {
        auto acpmsg = u16toacp(message);
        DWORD written;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), acpmsg.data(), acpmsg.size(), &written, nullptr);
    }
#else
    MessageBoxW(nullptr, std::wstring(message).c_str(), argv0.c_str(), 0);
#endif
}

static void show_error(std::wstring_view message) {
#ifdef CONSOLE
    if (hasConsole) {
        WriteConsoleW(GetStdHandle(STD_ERROR_HANDLE), message.data(), message.size(), nullptr, nullptr);
    } else {
        auto acpmsg = u16toacp(message);
        DWORD written;
        WriteFile(GetStdHandle(STD_ERROR_HANDLE), acpmsg.data(), acpmsg.size(), &written, nullptr);
    }
#else
    MessageBoxW(nullptr, std::wstring(message).c_str(), argv0.c_str(), MB_ICONERROR);
#endif
}

static void show_usage() {
    auto message = std::format(
L"Usage: {} [options] target\n"
"Options:\n"
"    /setdebugger      set loader as debugger of target exe with current options\n"
"                      in Image File Execution Options\n"
"    /unsetdebugger    unset debugger of target in Image File Execution Options\n"
"    /with:dllname     launch target with specified DLL injected\n"
"                      can be specified multiple times\n"
"    /noconf           disable reading withdll.conf in target directory\n"
"    /wait | /nowait   sets if loader waits for target process to exit\n"
"                      and return its exit code\n"
"    \n"
"Without /noconf, loader reads DLL names from withdll.conf in target directory \n"
"(ANSI encoded, one DLL name each line). \n"
"DLL names in /with and withdll.conf shoule be basename only (no path separator)\n"
"as the names are injected into import table.\n"
"\n"
"For more detailed behavior and limitations, see Detours docs.\n"
                                , argv0);
    show_message(message);
}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR pszCmdLine, int nCmdShow) {
    const wchar_t *cmdline_it = GetCommandLineW();
    auto argv0opt = GetNextArgumentItem(cmdline_it);
    argv0 = argv0opt.value_or(L"loader"sv);
    const wchar_t *target_cmdline = cmdline_it;
    auto argv1 = GetNextArgumentItem(cmdline_it);
    DWORD dummy;
    AttachConsole(ATTACH_PARENT_PROCESS);
    hasConsole = GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &dummy);
    if (!argv1) {
        show_usage();
        return 1;
    }
    
    std::vector<std::string> setdlls, confdlls;
    bool noconf = false;
    bool setup = false;
    bool unset = false;
    std::optional<bool> wait;
    do {
        auto argv1sv = argv1.value();
        if (tolower(argv1sv.substr(0, 6)) == L"/with:"sv) {
            setdlls.emplace_back(u16toacp(argv1sv.substr(6)));
        } else if (tolower(argv1sv) == L"/noconf"sv) {
            noconf = true;
        } else if (tolower(argv1sv) == L"/nowait"sv) {
            wait = false;
        } else if (tolower(argv1sv) == L"/wait"sv) {
            wait = true;
        } else if (tolower(argv1sv) == L"/setdebugger"sv) {
            setup = true;
        } else if (tolower(argv1sv) == L"/unsetdebugger"sv) {
            setup = true;
            unset = true;
        } else if (argv1sv == L"/?"sv) {
            show_usage();
            return 0;
        } else if (argv1sv[0] == L'/') {
            show_error(std::format(L"unrecognized option {}"sv, argv1sv));
        } else {
            break;
        }
        target_cmdline = cmdline_it;
        argv1 = GetNextArgumentItem(cmdline_it);
    } while(argv1);
    if (!argv1) {
        show_usage();
    }
    std::wstring target_exe(argv1.value());
    std::filesystem::path exepath = target_exe;
    if (!target_exe.empty() && target_exe[0] == L'"') {
        target_exe.erase(0, 1);
        target_exe.pop_back();
    }
    std::wstring target_cmdline_buf(target_cmdline);
    auto message = std::format(L"target_exe = {}\ntarget_cmdline = {}\nwait = {}\n"sv, target_exe, target_cmdline_buf, wait.has_value() ? (wait.value() ? L"true"sv : L"false"sv) : L"<unset>"sv);
    OutputDebugStringW(message.c_str());

    if (!noconf) {
        std::filesystem::path confpath = exepath;
        confpath.replace_filename(L"withdll.conf"sv);
        message = std::format(L"conffile = {}\n"sv, confpath.wstring());
        OutputDebugStringW(message.c_str());
        std::ifstream fs(confpath);
        std::string line;
        if (!fs.fail()) {
            while (std::getline(fs, line)) {
                auto trimmed = trim(line);
                if (!trimmed.empty()) {
                    confdlls.emplace_back(trim(line));
                }
            }
        }
    }
    std::string dllsmsg;
    dllsmsg.reserve(1024);
    dllsmsg.append("dlls:");
    std::vector<const char*> dllptrs;
    dllptrs.reserve(setdlls.size() + confdlls.size());
    for (auto &s : setdlls) {
        dllptrs.push_back(s.c_str());
        dllsmsg.push_back(' ');
        dllsmsg.append(s);
    }
    for (auto& s : confdlls) {
        dllptrs.push_back(s.c_str());
        dllsmsg.push_back(' ');
        dllsmsg.append(s);
    }
    OutputDebugStringA(dllsmsg.c_str());
    if (setup) {
        std::wstring filename(32768, 0);
        auto len = GetModuleFileNameW(hInstance, filename.data(), 32768);
        filename.resize(len);
        HKEY keyIFEO, keyExeFile;
        try {
            throw_win32_error(RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options", 0, KEY_CREATE_SUB_KEY, &keyIFEO));
            auto keyname = exepath.filename().native();
            throw_win32_error(RegCreateKeyExW(keyIFEO, keyname.c_str(), 0, 0, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &keyExeFile, nullptr));
            std::wstring debugger;
            debugger.reserve(32768);
            if (filename.find(L' ') != std::string::npos) {
                debugger.append(L"\""sv);
                debugger.append(filename);
                debugger.append(L"\""sv);
            } else {
                debugger.append(filename);
            }
            for (auto& s : setdlls) {
                debugger.append(std::format(L" /with:{}"sv, acptou16(s)));
            }
            if (noconf) debugger.append(L" /noconf"sv);
            if (wait.has_value()) debugger.append(wait.value() ? L" /wait"sv : L" /nowait"sv);
            if (unset) {
                throw_win32_error(RegDeleteKeyValueW(keyExeFile, L"", L"Debugger"));
                show_message(std::format(L"Successfully cleared debugger for {}"sv, keyname));
            } else {
                throw_win32_error(RegSetValueExW(keyExeFile, L"Debugger", 0, REG_SZ, reinterpret_cast<const BYTE*>(debugger.data()), debugger.size() * sizeof(wchar_t)));
                show_message(std::format(L"Successfully set debugger for {}"sv, keyname));
            }
            return 0;
        } catch (std::system_error &e) {
            // std::system_error::what() and std::error_code::message() returns in ACP
            show_error(w32strerror(e.code().value()));
            return 1;
        }
    }
    STARTUPINFOW si{sizeof(si), 0};
    PROCESS_INFORMATION pi;
    // Create with DEBUG_PROCESS or DEBUG_ONLY_THIS_PROCESS to ignore the Debugger key in Image File Execution Options
    if(DetourCreateProcessWithDllsW(nullptr, target_cmdline_buf.data(), nullptr, nullptr, FALSE, DEBUG_ONLY_THIS_PROCESS, nullptr, nullptr, &si, &pi, dllptrs.size(), dllptrs.data(), nullptr)) {
        DebugSetProcessKillOnExit(FALSE);
        DebugActiveProcessStop(pi.dwProcessId);
        CloseHandle(pi.hThread);
        if (wait.value_or(hasConsole)) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            DWORD ex = 255;
            GetExitCodeProcess(pi.hProcess, &ex);
            return ex;
        } else {
            return 0;
        }
    } else {
        auto err = GetLastError();
#if CONSOLE
        show_error(w32strerror(err));
#else
        OutputDebugStringW(w32strerror(err));
#endif
        return (int)err;
    }
    return 255;
    
}

int main() {
    STARTUPINFOW si{sizeof(si), 0};
    GetStartupInfoW(&si);
    return wWinMain(GetModuleHandle(nullptr), nullptr, GetCommandLineW(), si.wShowWindow);
}