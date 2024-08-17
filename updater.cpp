#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <locale>
#include <codecvt>

namespace fs = std::filesystem;


std::vector<int> parsePids(const std::wstring& pidsStr) {
    std::vector<int> pids;
    std::wstringstream ss(pidsStr);
    std::wstring pid;
    while (std::getline(ss, pid, L',')) {
        pids.push_back(std::stoi(pid));
    }
    return pids;
}

std::wstring pidsToString(const std::vector<int>& pids) {
    std::wstringstream ss;
    for (size_t i = 0; i < pids.size(); ++i) {
        if (i > 0) ss << L",";
        ss << pids[i];
    }
    return ss.str();
}

std::wstring ConvertToWString(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

std::wstring targetDir;
std::wstring exe;
std::wstring logMessages; // Global variable to store log messages
HWND hwndButton; // Handle for the OK button
HWND hwndLog;    // Handle for the log edit control

// Custom message for starting batch logic execution
#define WM_START_EXECUTION (WM_USER + 1)

void LogMessage(const std::wstring& message) {
    // Print the log message to the console
    std::wcout << message << std::endl;

    // Update the global logMessages variable
    logMessages += message + L"\r\n";

    // Update the content of the log window
    if (hwndLog) {
        SetWindowText(hwndLog, logMessages.c_str());

        // Scroll to the end
        SendMessage(hwndLog, EM_SETSEL, logMessages.length(), logMessages.length());
        SendMessage(hwndLog, EM_SCROLLCARET, 0, 0);
    }
}

void LogError(const std::wstring& message) {
    LogMessage(L"Error: " + message);
}


void CheckAndTerminatePIDs(const std::vector<int>& pids, int checkDuration) {
    for (int pid : pids) {
        int elapsedTime = 0;
        bool isRunning = true;

        while (elapsedTime < checkDuration) {
            LogMessage(L"Waiting for PID " + std::to_wstring(pid) + L" to exit...");
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
            if (hProcess == NULL) {
                LogMessage(L"PID " + std::to_wstring(pid) + L" has exited.");
                isRunning = false;
                break;
            }
            CloseHandle(hProcess);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            elapsedTime++;
        }

        if (isRunning) {
            LogMessage(L"PID " + std::to_wstring(pid) + L" is still running after " + std::to_wstring(checkDuration) + L" seconds, killing it...");
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
            if (hProcess != NULL) {
                TerminateProcess(hProcess, 1);
                CloseHandle(hProcess);
                LogMessage(L"PID " + std::to_wstring(pid) + L" has been terminated.");
            }
            else {
                LogMessage(L"Failed to open process for PID " + std::to_wstring(pid));
            }
        }
    }
}

void DeleteAndCopyDirectory(const std::wstring& sourceDir, const std::wstring& targetDir) {
    try {
        for (const auto& entry : fs::directory_iterator(sourceDir)) {
            fs::path targetPath = fs::path(targetDir) / entry.path().filename();
            if (fs::exists(targetPath)) {
                fs::remove_all(targetPath);
                LogMessage(L"Deleted: " + targetPath.wstring());
            }
            fs::copy(entry.path(), targetPath, fs::copy_options::recursive);
            LogMessage(L"Copied: " + entry.path().wstring() + L" to " + targetPath.wstring());
        }
    }
    catch (const fs::filesystem_error& e) {
        LogMessage(L"Filesystem error: " + std::wstring(e.what(), e.what() + strlen(e.what())));
    }
    catch (const std::system_error& e) {
        LogMessage(L"System error: " + std::wstring(e.what(), e.what() + strlen(e.what())) + L" Code: " + std::to_wstring(e.code().value()));
    }
    catch (const std::exception& e) {
        LogMessage(L"Exception: " + std::wstring(e.what(), e.what() + strlen(e.what())));
    }
}

void ExecuteBatchLogic(const std::wstring& sourceDir, const std::wstring& targetDir, const std::wstring& exe, const std::vector<int>& pids) {
    if (sourceDir.empty()) {
        LogError(L"Source directory is not defined.");
        return;
    }

    if (targetDir.empty()) {
        LogError(L"Target directory is not defined.");
        return;
    }

    if (pids.empty()) {
        LogError(L"pid_list is not defined.");
        return;
    }

    if (exe.empty()) {
        LogError(L"exe is not defined.");
        return;
    }

    LogMessage(L"Source directory: " + sourceDir);
    LogMessage(L"Target directory: " + targetDir);
    LogMessage(L"Executable: " + exe);

    CheckAndTerminatePIDs(pids, 10);

    DeleteAndCopyDirectory(sourceDir, targetDir);

    LogMessage(L"Update Done");
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_SIZE: {
        RECT rect;
        GetClientRect(hwnd, &rect);
        SetWindowPos(hwndLog, NULL, 0, 0, rect.right, rect.bottom - 40, SWP_NOZORDER);
        SetWindowPos(hwndButton, NULL, rect.right / 2 - 50, rect.bottom - 35, 100, 30, SWP_NOZORDER);
    }
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) { // OK button clicked

            STARTUPINFO si = { sizeof(STARTUPINFO) };
            PROCESS_INFORMATION pi;
            
            std::wstring commandLine = targetDir + L"\\" + exe;
            wchar_t* cmdLine = new wchar_t[commandLine.length() + 1];
            wcscpy_s(cmdLine, commandLine.length() + 1, commandLine.c_str());

            LogMessage(L"Run exe: " + commandLine);

            if (CreateProcessW(NULL, cmdLine, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, targetDir.c_str(), &si, &pi)) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                delete[] cmdLine;
                // Close the current application
                PostQuitMessage(0);
            }
            else {
                DWORD error = GetLastError();
                LogError(L"Failed to start the process. Error code: " + std::to_wstring(error));
            }

            delete[] cmdLine;

        }
        return 0;
    case WM_START_EXECUTION: {
        // Start the execution logic
        std::wstring* args = reinterpret_cast<std::wstring*>(lParam);
        std::wstring sourceDir = args[0];
        std::wstring targetDir = args[1];
        std::wstring exe = args[2];
        std::vector<int> pids = parsePids(args[3]);
        delete[] args;

        ExecuteBatchLogic(sourceDir, targetDir, exe, pids);
        return 0;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void DisplayLogWindow(const std::wstring& sourceDir, const std::wstring& targetDir, const std::wstring& exe, const std::vector<int>& pids) {
    const wchar_t CLASS_NAME[] = L"LogWindowClass";
    WNDCLASS wc = {};

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    // Get screen dimensions and center the window
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowWidth = screenWidth / 2;
    int windowHeight = screenHeight / 2;

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"OK Updater", // Changed window name
        WS_OVERLAPPEDWINDOW, // Allows resizing, title bar, and system menu
        (screenWidth - windowWidth) / 2, (screenHeight - windowHeight) / 2, windowWidth, windowHeight,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    if (hwnd == NULL) {
        return;
    }

    // Create an edit control for log display
    hwndLog = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        0, 0, windowWidth, windowHeight - 40,
        hwnd,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    // Create OK button with a higher margin from the bottom
    hwndButton = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"OK",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        windowWidth / 2 - 50,        // x position (centered)
        windowHeight - 35,           // y position (higher margin)
        100,        // Button width
        30,         // Button height
        hwnd,       // Parent window
        (HMENU)1,   // Button ID
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed

    ShowWindow(hwnd, SW_SHOW);

    // Send a custom message to start execution after the window is created
    std::wstring* args = new std::wstring[4]{ sourceDir, targetDir, exe, pidsToString(pids) };
    PostMessage(hwnd, WM_START_EXECUTION, 0, reinterpret_cast<LPARAM>(args));

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    std::string cmdLine(lpCmdLine);
    std::istringstream iss(cmdLine);
    std::string param1, param2, param3, param4;

    iss >> param1 >> param2 >> param3 >> param4;

    std::string sourceDir = param1;
    targetDir = ConvertToWString(param2);
    exe = ConvertToWString(param3);
    std::vector<int> pids = parsePids(ConvertToWString(param4));

    std::thread logWindowThread(DisplayLogWindow, ConvertToWString(sourceDir), targetDir, exe, pids);

    logWindowThread.join();
    return 0;
}
