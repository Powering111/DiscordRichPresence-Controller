#include <iostream>
#include <discord/discord.h>
#include <thread>
#include <chrono>
#include <csignal>
#include <windows.h>
#include <string>
#include <atomic>
#include <codecvt>
#include "resource.h"
discord::Core* core{};
discord::Activity activity{};
void initDiscord();
void loopDiscord();
void handleUpdate(const discord::Result& result);
void handleClear(const discord::Result& result);
void handleLogHook(discord::LogLevel level, const char* message);

void setTimerStartNow();
uint64_t getEpochSeconds();

HWND hwndEdit1, hwndEdit2, hwndEdit3, hwndEdit4, hwndEdit5;
const int PROPERTY_LENGTH = 100;
wchar_t detail_buf[PROPERTY_LENGTH];
wchar_t largeImage_buf[PROPERTY_LENGTH];
wchar_t largeText_buf[PROPERTY_LENGTH];
wchar_t smallImage_buf[PROPERTY_LENGTH];
wchar_t smallText_buf[PROPERTY_LENGTH];

uint64_t startTime;

std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;

namespace {
    volatile bool interrupted{ false };
}
bool running = false;
std::atomic<bool> hasToUpdate = false;
std::thread RPThread;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK helpWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

HINSTANCE hInst;

HWND helphwnd;

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
{

    // for debug ; allocate console.
    /*
    FILE* fp;
    AllocConsole();
    freopen_s(&fp, "CONIN$", "r", stdin);
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    */

    hInst = hInstance;
    // Register the window class.
    const wchar_t CLASS_NAME[] = L"DISCORD_RICH_PRESENCE";

    WNDCLASS wc = { };

    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    // Create the window.

    HWND hwnd = CreateWindowEx(
        0, CLASS_NAME, L"Discord Rich Presence",
        WS_OVERLAPPED | WS_MINIMIZEBOX | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 310,220,
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL)
    {
        return 0;
    }


    // help window
    WNDCLASS helpwc = { };

    helpwc.lpfnWndProc = helpWindowProc;
    helpwc.hInstance = hInstance;
    helpwc.lpszClassName = L"DISCORD_RICH_PRESENCE_HELP";
    RegisterClass(&helpwc);
    helphwnd = CreateWindowEx(
        0, L"DISCORD_RICH_PRESENCE_HELP", L"Help",
        WS_POPUP|WS_BORDER|WS_CAPTION, CW_USEDEFAULT, CW_USEDEFAULT, 550, 290, NULL, NULL, hInstance, NULL);
    if (helphwnd == NULL) {
        return 0;
    }

    // icon
    //HANDLE mainIcon = LoadImage(0, L"res/icon.ico", IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE);
    HANDLE mainIcon = LoadImage(hInstance, MAKEINTRESOURCE(MAIN_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    
    if(mainIcon) {
        SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)mainIcon);
        SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)mainIcon);
        
        SendMessage(helphwnd, WM_SETICON, ICON_SMALL, (LPARAM)mainIcon);
        SendMessage(helphwnd, WM_SETICON, ICON_BIG, (LPARAM)mainIcon);
    }

    // initialization
    ShowWindow(hwnd, nCmdShow);
    initDiscord();
    RPThread = std::thread(loopDiscord);

    // message loop
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}



LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{

    switch (uMsg)
    {
    case WM_CREATE:
    {
        HWND btn1 = CreateWindow(L"button", L"Update", WS_CHILD | WS_VISIBLE | BS_FLAT, 10, 30, 80, 30, hwnd, (HMENU)0x1001, hInst, NULL);
        HWND btn2 = CreateWindow(L"button", L"Clear", WS_CHILD | WS_VISIBLE | BS_FLAT, 100, 30, 80, 30, hwnd, (HMENU)0x1002, hInst, NULL);
        HWND btn3 = CreateWindow(L"button", L"[NOW]", WS_CHILD | WS_VISIBLE | BS_FLAT, 190, 30, 70, 30, hwnd, (HMENU)0x1003, hInst, NULL);
        HWND btn4 = CreateWindow(L"button", L"?", WS_CHILD | WS_VISIBLE | BS_FLAT, 270, 10, 20, 20, hwnd, (HMENU)0x1004, hInst, NULL);
        hwndEdit1 = CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 110, 80, 170, 20, hwnd, (HMENU)0x2001, hInst, NULL);
        hwndEdit2 = CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 110, 110, 80, 20, hwnd, (HMENU)0x2002, hInst, NULL);
        hwndEdit3 = CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 200, 110, 80, 20, hwnd, (HMENU)0x2003, hInst, NULL);
        hwndEdit4 = CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 110, 140, 80, 20, hwnd, (HMENU)0x2004, hInst, NULL);
        hwndEdit5 = CreateWindow(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 200, 140, 80, 20, hwnd, (HMENU)0x2005, hInst, NULL);
        SendMessage(hwndEdit1, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(5, 5));
        SendMessage(hwndEdit2, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(5, 5));
        SendMessage(hwndEdit3, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(5, 5));
        SendMessage(hwndEdit4, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(5, 5));
        SendMessage(hwndEdit5, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(5, 5));
        SendMessage(hwndEdit1, EM_SETLIMITTEXT, (WPARAM)PROPERTY_LENGTH, NULL);

    }
    return 0;
    case WM_DESTROY:
        interrupted = true;
        RPThread.join();
        PostQuitMessage(0);
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        SetBkMode(hdc, TRANSPARENT);
        FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
        RECT input_rect = RECT({ 5, 75, 285, 165 });

        HBRUSH frameBrush = CreateSolidBrush(RGB(204, 255, 255));
        FillRect(hdc, &input_rect, frameBrush);

        if (running) {
            LPCWSTR szMessage = L"Showing!";
            TextOut(hdc, 10, 10, szMessage, wcslen(szMessage));
        }
        else {
            LPCWSTR szMessage = L"Default";
            TextOut(hdc, 10, 10, szMessage, wcslen(szMessage));
        }

        TextOut(hdc, 10, 80, L"Detail", 6);
        TextOut(hdc, 10, 110, L"Large Image", 11);
        TextOut(hdc, 10, 140, L"Small Image", 11);
        EndPaint(hwnd, &ps);

    }
    return 0;

    case WM_COMMAND:
    {
        switch (LOWORD(wParam)) {
        case 0x1001:
            if (HIWORD(wParam) == BN_CLICKED) {
                GetWindowTextW(hwndEdit1, detail_buf, PROPERTY_LENGTH);
                GetWindowTextW(hwndEdit2, largeImage_buf, PROPERTY_LENGTH);
                GetWindowTextW(hwndEdit3, largeText_buf, PROPERTY_LENGTH);
                GetWindowTextW(hwndEdit4, smallImage_buf, PROPERTY_LENGTH);
                GetWindowTextW(hwndEdit5, smallText_buf, PROPERTY_LENGTH);
                
                running = true;
                hasToUpdate.store(true);

                InvalidateRect(hwnd, 0, TRUE);
                UpdateWindow(hwnd);
            }
            break;

        case 0x1002:
        {
            if (HIWORD(wParam) == BN_CLICKED) {
                if (running) {
                    // clear running
                    running = false;
                    hasToUpdate.store(true);

                    InvalidateRect(hwnd, 0, TRUE);
                    UpdateWindow(hwnd);
                }
            }
        }
        break;
        case 0x1003:
        {
            if (HIWORD(wParam) == BN_CLICKED) {
                setTimerStartNow();
                hasToUpdate.store(true);
                std::cout << "Set started time to now." << std::endl;
            }
        }
        break;
        case 0x1004:
        {
            if (HIWORD(wParam) == BN_CLICKED) {
                ShowWindow(helphwnd, SW_SHOWDEFAULT);
                
            }
        }
        break;

        }
        return 0;
    }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


LRESULT CALLBACK helpWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

    switch (uMsg)
    {
    case WM_CREATE:
    {
        HWND btn1 = CreateWindow(L"button", L"Close", WS_CHILD | WS_VISIBLE | BS_FLAT, 10, 220, 60, 20, hwnd, (HMENU)0x3001, hInst, NULL);

    }
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        SetBkMode(hdc, TRANSPARENT);
        FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
        const WCHAR * texts[] = {
            L"Help",
            L"Discord Rich Presence Controller (Made by JJT)",
            L"[Details] shows the detail to your status.",
            L"[Large/Small Image] sets which image to use. It is provided by this software.",
            L"If you want to add image, contact the developer.",
            L"[Large/Small Text] sets which text to show when hovering the image.",
            L"Press [Update] button to apply changes to discord.",
            L"Press [Clear] button to set it to the default status(provided by this software).",
            L"Press [NOW] button to reset 'since' time to now.",
            L"If you close this program, discord will detect it and will remove the status."
        };
        int x = 15;
        int y = 10;
        for (int i = 0; i < 10; i++) {
            TextOut(hdc, x, y, texts[i], wcslen(texts[i]));
            y += 20;
        }
        EndPaint(hwnd, &ps);
    }
        return 0;
    case WM_COMMAND:
    {
        switch (LOWORD(wParam)) {
        case 0x3001:
        {
            ShowWindow(hwnd, SW_HIDE);
        }
            break;
        }
    }
        return 0;


    }


    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}



void initDiscord() {
    discord::Core::Create(1102256789178826855, DiscordCreateFlags_Default, &core);
    core->SetLogHook(discord::LogLevel::Debug, handleLogHook);
}

void loopDiscord() {
    activity.GetAssets().SetLargeImage("e11");
    activity.GetAssets().SetLargeText("KAIST");
    setTimerStartNow();
    
    std::signal(SIGINT, [](int) { interrupted = true; });
    while (!interrupted) {
        if (running) {
            if (hasToUpdate.load()) {
                activity.SetDetails(utf8_conv.to_bytes(std::wstring(detail_buf)).c_str());
                activity.GetTimestamps().SetStart(startTime);
                activity.GetAssets().SetLargeImage(utf8_conv.to_bytes(std::wstring(largeImage_buf)).c_str());
                activity.GetAssets().SetLargeText(utf8_conv.to_bytes(std::wstring(largeText_buf)).c_str());
                activity.GetAssets().SetSmallImage(utf8_conv.to_bytes(std::wstring(smallImage_buf)).c_str());
                activity.GetAssets().SetSmallText(utf8_conv.to_bytes(std::wstring(smallText_buf)).c_str());
                core->ActivityManager().UpdateActivity(activity, handleUpdate);
                hasToUpdate.store(false);
            }
        }
        else {
            if (hasToUpdate.load()) {
                core->ActivityManager().ClearActivity(handleClear);
                hasToUpdate.store(false);
            }
        }

        discord::Result result = core->RunCallbacks();
        if (result == discord::Result::Ok) {

        }
        else {
            std::cerr << "something wrong. (" << (int)result << ")" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}


uint64_t getEpochSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void setTimerStartNow() {
    startTime = getEpochSeconds();
}
// discord sdk callback functions
void handleUpdate(const discord::Result& result) {
    if (result == discord::Result::Ok) {
        std::cout << "UpdateActivity success." << std::endl;
    }
    else {
        std::cout << "UpdateActivity failed. (" << (int)result << ")" << std::endl;
    }
}
void handleClear(const discord::Result& result) {
    if (result == discord::Result::Ok) {
        std::cout << "ClearActivity success." << std::endl;
    }
    else {
        std::cout << "ClearActivity failed. (" << (int)result << ")" << std::endl;
    }
}

void handleLogHook(discord::LogLevel level, const char* message) {
    std::cout << "Log [" << int(level) << "] " << message << std::endl;
}