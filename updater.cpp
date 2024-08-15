#include <windows.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <filesystem>

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

void LogMessage(const std::string& message) {
    // Update the global logMessages variable
    logMessages += std::wstring(message.begin(), message.end()) + L"\n";

    // Invalidate the window to trigger a repaint
    HWND hwnd = FindWindow(L"LogWindowClass", L"OK Updater");
    if (hwnd) {
        InvalidateRect(hwnd, NULL, TRUE);
    }
}

void LogError(const std::string& message) {
    // Prefix the message with "Error:"
    std::string errorMessage = "Error: " + message;
    std::wstring wErrorMessage = std::wstring(errorMessage.begin(), errorMessage.end()) + L"\n";

    // Update the global logMessages variable
    logMessages += wErrorMessage;

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
            LogMessage("Waiting for PID " + std::to_string(pid) + " to exit...");
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
            if (hProcess == NULL) {
                LogMessage("PID " + std::to_string(pid) + " has exited.");
                isRunning = false;
                break;
            }
            CloseHandle(hProcess);
            std::this_thread::sleep_for(std::chrono::seconds(1));
            elapsedTime++;
        }

        if (isRunning) {
            LogMessage("PID " + std::to_string(pid) + " is still running after " + std::to_string(checkDuration) + " seconds, killing it...");
            HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
            if (hProcess != NULL) {
                TerminateProcess(hProcess, 1);
                CloseHandle(hProcess);
                LogMessage("PID " + std::to_string(pid) + " has been terminated.");
            }
            else {
                LogMessage("Failed to open process for PID " + std::to_string(pid));
            }
        }
    }
}

void DeleteAndCopyDirectory(const std::string& sourceDir, const std::string& targetDir) {
    try {
        for (const auto& entry : fs::directory_iterator(sourceDir)) {
            fs::path targetPath = fs::path(targetDir) / entry.path().filename();
            if (fs::exists(targetPath)) {
                fs::remove_all(targetPath);
                LogMessage("Deleted: " + targetPath.string());
            }
            fs::copy(entry.path(), targetPath, fs::copy_options::recursive);
            LogMessage("Copied: " + entry.path().string() + " to " + targetPath.string());
        }
    }
    catch (const fs::filesystem_error& e) {
        LogMessage("Filesystem error: " + std::string(e.what()));
    }
}

void ExecuteBatchLogic(const std::string& sourceDir, const std::string& targetDir, const std::string& exe, const std::vector<int>& pids) {
    if (sourceDir.empty()) {
        LogError("Source directory is not defined.");
        return;
    }

    if (targetDir.empty()) {
        LogError("Target directory is not defined.");
        return;
    }

    if (pids.empty()) {
        LogError("pid_list is not defined.");
        return;
    }

    if (exe.empty()) {
        LogError("exe is not defined.");
        return;
    }

    LogMessage("Source directory: " + sourceDir);
    LogMessage("Target directory: " + targetDir);
    LogMessage("Executable: " + exe);

    CheckAndTerminatePIDs(pids, 10);

    DeleteAndCopyDirectory(sourceDir, targetDir);

    LogMessage("Update Done");
}

std::vector<int> parsePids(const std::string& pidsStr) {
    std::vector<int> pids;
    std::stringstream ss(pidsStr);
    std::string pid;
    while (std::getline(ss, pid, ',')) {
        pids.push_back(std::stoi(pid));
    }
    return pids;
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
    std::vector<int> pids = parsePids(param4);

    ExecuteBatchLogic(sourceDir, targetDir, exe, pids);

    logWindowThread.join();
    return 0;
}