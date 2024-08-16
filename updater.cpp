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

std::wstring logMessages; // Global variable to store log messages
HWND hwndButton; // Handle for the OK button

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        DrawText(hdc, logMessages.c_str(), -1, &rect, DT_LEFT | DT_TOP | DT_WORDBREAK); // Use DrawText for multi-line text
        EndPaint(hwnd, &ps);
    }
                 return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) { // OK button clicked
            PostQuitMessage(0);
        }
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void DisplayLogWindow() {
    const wchar_t CLASS_NAME[] = L"LogWindowClass";
    WNDCLASS wc = {};

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"OK Updater", // Changed window name
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, // Non-resizable window styles
        CW_USEDEFAULT, CW_USEDEFAULT, 500, 400,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    if (hwnd == NULL) {
        return;
    }

    // Create OK button with higher margin from the bottom
    hwndButton = CreateWindow(
        L"BUTTON",  // Predefined class; Unicode assumed 
        L"OK",      // Button text 
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
        200,        // x position 
        300,        // y position (higher margin)
        100,        // Button width
        30,         // Button height
        hwnd,       // Parent window
        (HMENU)1,   // Button ID
        (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
        NULL);      // Pointer not needed

    ShowWindow(hwnd, SW_SHOW);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void LogMessage(const std::wstring& message) {
    // Print the log message to the console
    std::wcout << message << std::endl;

    // Update the global logMessages variable
    logMessages += message + L"\n";

    // Invalidate the window to trigger a repaint
    HWND hwnd = FindWindow(L"LogWindowClass", L"OK Updater");
    if (hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

void LogError(const std::wstring& message) {
    // Prefix the message with "Error:"
    std::wstring errorMessage = L"Error: " + message + L"\n";

    // Update the global logMessages variable
    logMessages += errorMessage;

    // Invalidate the window to trigger a repaint
    HWND hwnd = FindWindow(L"LogWindowClass", L"OK Updater");
    if (hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
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

std::vector<int> parsePids(const std::wstring& pidsStr) {
    std::vector<int> pids;
    std::wstringstream ss(pidsStr);
    std::wstring pid;
    while (std::getline(ss, pid, L',')) {
        pids.push_back(std::stoi(pid));
    }
    return pids;
}

std::wstring ConvertToWString(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    std::thread logWindowThread(DisplayLogWindow);

    std::string cmdLine(lpCmdLine);
    std::istringstream iss(cmdLine);
    std::string param1, param2, param3, param4;

    iss >> param1 >> param2 >> param3 >> param4;

    std::string sourceDir = param1;
    std::string targetDir = param2;
    std::string exe = param3;
    std::vector<int> pids = parsePids(ConvertToWString(param4));

    ExecuteBatchLogic(ConvertToWString(sourceDir), ConvertToWString(targetDir), ConvertToWString(exe), pids);

    logWindowThread.join();
    return 0;
}

