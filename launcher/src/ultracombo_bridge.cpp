// SPDX-License-Identifier: GPL-3.0-only
// Part of cps2recomp (the user's fork).
//
// The collection's program for a game that plays in UltraCombo (github.com/thefixinhixon/UltraCombo; description.cpp,
// UltraComboGame): built as the game's id beside the launcher (kinst, kinst2), it starts the UltraCombo.exe the launcher
// gives it (--rom) with --game ULTRACOMBO_GAME, in UltraCombo's own folder, and ends as it ends. It has no features to
// list (--list-features: none), and the launcher's other arguments are not UltraCombo's: it leaves them out.

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifndef ULTRACOMBO_GAME
#error "ULTRACOMBO_GAME (ki1 or ki2) is not defined"
#endif
#define WIDE_(text) L##text
#define WIDE(text) WIDE_(text)

int main(int argc, char** argv) {
    std::string program;
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--list-features") == 0) {
            return 0;
        }
        if (std::strcmp(argv[i], "--rom") == 0 && i + 1 < argc) {
            program = argv[++i];
        }
    }
    if (program.empty()) {
        std::fprintf(stderr, "UltraCombo isn't chosen: open the launcher and choose its UltraCombo.exe.\n");
        return 1;
    }
    const std::filesystem::path folder = std::filesystem::path(program).parent_path();
#ifdef _WIN32
    // (the path as the launcher wrote it: UTF-8)
    auto wide = [](const std::string& text) {
        int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        std::wstring out(static_cast<size_t>(size > 0 ? size - 1 : 0), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, out.data(), size);
        return out;
    };
    const std::wstring exe = wide(program);
    const std::wstring directory = wide(folder.string());
    std::wstring line = L"\"" + exe + L"\" --game " WIDE(ULTRACOMBO_GAME);
    STARTUPINFOW startup{};
    startup.cb = sizeof startup;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(exe.c_str(), line.data(), nullptr, nullptr, FALSE, 0, nullptr, directory.c_str(), &startup, &process)) {
        std::fprintf(stderr, "UltraCombo could not be started from %s (error %lu).\n", program.c_str(), GetLastError());
        return 1;
    }
    CloseHandle(process.hThread);
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hProcess);
    return static_cast<int>(code);
#else
    pid_t pid = fork();
    if (pid == 0) {
        if (chdir(folder.c_str()) == 0) {
            execl(program.c_str(), program.c_str(), "--game", ULTRACOMBO_GAME, static_cast<char*>(nullptr));
        }
        _exit(127);
    }
    int status = 0;
    if (pid < 0 || waitpid(pid, &status, 0) < 0) {
        std::fprintf(stderr, "UltraCombo could not be started from %s.\n", program.c_str());
        return 1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
}
