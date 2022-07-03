#ifndef UNICODE
#define UNICODE
#endif // UNICODE

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif // MOD_NOREPEAT

#include <windows.h>

// for GET_X_LPARAM
#include <windowsx.h>

#include <iostream>


typedef enum MOUSE_DIRECTION {
    MOUSE_UP        = 1 << 0,
    MOUSE_LEFT      = 1 << 1,
    MOUSE_DOWN      = 1 << 2,
    MOUSE_RIGHT     = 1 << 3,

    MOUSE_DIR_NULL  = 0,
} MOUSE_DIRECTION;

typedef enum MOUSE_SPEED {
    // a movement of 0-2 pixels
    MOUSE_SLOW    = 1 << 0,
    // a movement of 3-5 pixels
    MOUSE_MED     = 1 << 1,
    // a movement of 5-10 pixels
    MOUSE_FAST    = 1 << 2
} MOUSE_SPEED;

typedef struct MOUSE_INFORMATION {
    MOUSE_DIRECTION direction;
    MOUSE_SPEED speed;
} MOUSE_INFORMATION;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Global Keyboard Hook
LRESULT CALLBACK Keyboard_Proc(int nCode, WPARAM wParam, LPARAM lParam);
HHOOK keyboard_hook;

// state variable to treat numpad inputs as mouse movement
bool mouse_mode_active = false;

// state variable to not send multiple mouse press inputs while the user is holding down the same key
bool mouse_held = false;

#define CURRENT_MOUSE_SPEED MOUSE_FAST;

bool make_new_cursor_coords(MOUSE_INFORMATION *info, POINT *pt){
/*
    Modifies the input POINT to hold new X and Y coordinates
    for the mouse movement described by the input MOUSE_INFORMATION struct.

    Currently only processes one axis of movement at a time, if diagonal
    movement is desired, just call this function twice with a vertical and
    horizontal direction

    REMINDER: (0, 0) is the top left corner of the screen
*/

    // need to make sure that our new point is bounded by all screens
    int total_screen_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int total_screen_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // The number of pixels to move the point
    int delta = 0;

    // a percentage of the total dimensions to traverse per message
    switch (info->speed){
    case MOUSE_SLOW:
        // 0-2% per movement
        delta = rand() %3;
        break;
    case MOUSE_MED:
        // 3-5% per movement
        delta = 3 + rand() %3;
        break;
    case MOUSE_FAST:
        // 5-7% per movement
        delta = 5 + rand() %3;
        break;
    }

    switch(info->direction){
    case MOUSE_DOWN:
        pt->y = pt->y + (int)(total_screen_height * delta / 100);
        break;
    case MOUSE_LEFT:
        pt->x = pt->x - (int)(total_screen_width * delta / 100);
        break;
    case MOUSE_RIGHT:
        pt->x = pt->x + (int)(total_screen_width * delta / 100);
        break;
    case MOUSE_UP:
        pt->y = pt->y - (int)(total_screen_height * delta / 100);
        break;
    case MOUSE_DIR_NULL:
        return -1;
    }

    // screen bounding
    if (pt->x < 0) pt->x = 0;
    if (pt->y < 0) pt->y = 0;
    if (pt->x >= total_screen_width) pt->x = total_screen_width - 1;
    if (pt->y >= total_screen_height) pt->y = total_screen_height - 1;

    return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszArgument, int nCmdShow){
    const wchar_t CLASS_NAME[] = L"Windows Hook";

    WNDCLASS wc = {};

    /*  A pointer to an application-defined function called the window proc,
        that defines most behavior for the window
    */
    wc.lpfnWndProc = WindowProc;

    /* The handle to the application instance */
    wc.hInstance = hInstance;

    /* A string that defines the window class */
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    // create the window
    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Window Text",
        WS_OVERLAPPEDWINDOW,

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

        NULL,       // Parent Window
        NULL,      // Menu
        hInstance,   // Instance Handle
        NULL
    );

    if (hwnd == NULL){
        return 0;
    }

    std::cout <<"My window handle: " << hwnd << "\n";

    // Toggle displaying the empty window if I need to capture inputs for testing
    // ShowWindow(hwnd, nCmdShow);

    // Register the Windows Hooks
    keyboard_hook = SetWindowsHookExA(
        WH_KEYBOARD_LL,
        Keyboard_Proc,
        NULL,               // The DLL containing the Hook proc is in this code
        0                   // Associate this hook with all threads on the same desktop
    );
    if (keyboard_hook == NULL)
        return 0;
    // alt+e hotkey to toggle mouse mode
    bool alt_hk = RegisterHotKey(hwnd, 1, (MOD_ALT | MOD_NOREPEAT), 0x45);
    if (!alt_hk)
        return 0;

    // Run the message loop
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) > 0){
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam){

    switch(uMsg){
    case WM_HOTKEY:
        // system-define snapshot hotkeys
        if (wParam < 0)
            break;
        else if (wParam == 1){
            mouse_mode_active = !mouse_mode_active;
            return 0;
        }
        break;
    case WM_DESTROY:
        UnhookWindowsHookEx(keyboard_hook);
        UnregisterHotKey(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


// Hooks Definitions
LRESULT CALLBACK Keyboard_Proc(int nCode, WPARAM wParam, LPARAM lParam){
   /* determine if toggling mouse_mode on or off */
   if (nCode >=0){

        // common vars for the switch
        POINT pt;

        switch(wParam){

        case WM_KEYDOWN:{
            // everything else will need to have mouse_mode active
            if (!mouse_mode_active)
                return CallNextHookEx(0, nCode, wParam, lParam);

            /* All details about the specific keys pressed in lParam */
            LPKBDLLHOOKSTRUCT key_details = (LPKBDLLHOOKSTRUCT)lParam;

            HWND active_window = GetForegroundWindow();

            MOUSE_INFORMATION mouse_info = {};
            mouse_info.speed = CURRENT_MOUSE_SPEED;
            mouse_info.direction = MOUSE_DIR_NULL;

            switch (key_details->vkCode){
            case VK_NUMPAD5:
                // consume the numpad if we are holding the mouse
                if (mouse_held)
                    return -1;

                mouse_held = true;
                std::cout << "Mouse clicked \n";
                // Obtain cursor coords in active window
                GetCursorPos(&pt);

                //ScreenToClient(active_window, &pt);
                PostMessage(active_window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(pt.x, pt.y));

                // Return non-zero value to prevent default
                return -1;
            case VK_NUMPAD8:
                mouse_info.direction = MOUSE_UP;
            case VK_NUMPAD4:
                mouse_info.direction = mouse_info.direction == MOUSE_DIR_NULL? MOUSE_LEFT : mouse_info.direction;
            case VK_NUMPAD2:
                mouse_info.direction = mouse_info.direction == MOUSE_DIR_NULL ? MOUSE_DOWN : mouse_info.direction;
            case VK_NUMPAD6:
                mouse_info.direction = mouse_info.direction == MOUSE_DIR_NULL ? MOUSE_RIGHT : mouse_info.direction;

                // move the mouse - common code
                GetCursorPos(&pt);
                POINT initial_pos = pt;
                make_new_cursor_coords(&mouse_info, &pt);

                // global coordinates
                // SetCursorPos(pt.x, pt.y);

                // translating to local coords does not seem to register
                // in some apps either
                // ScreenToClient(active_window,&pt);
                // PostMessage(active_window, WM_MOUSEMOVE, 0, MAKELPARAM(pt.x, pt.y));

                INPUT mouse_input;
                mouse_input.type = INPUT_MOUSE;
                mouse_input.mi.dx = pt.x - initial_pos.x;
                mouse_input.mi.dy = pt.y - initial_pos.y;
                mouse_input.mi.mouseData = 0;
                mouse_input.mi.dwFlags = MOUSEEVENTF_MOVE;
                mouse_input.mi.time = 0;
                SendInput(1, &mouse_input, sizeof(INPUT));

                return -1;  // prevent initial numpad press from propagating
            } // end switch on key_details->vkCode

        } break;

        case WM_KEYUP: {
            if (!mouse_mode_active)
                return CallNextHookEx(0, nCode, wParam, lParam);

            /* All details about the specific keys pressed in lParam */
            LPKBDLLHOOKSTRUCT key_details = (LPKBDLLHOOKSTRUCT)lParam;

            HWND active_window = GetForegroundWindow();

            // mouse click
            if (key_details->vkCode == VK_NUMPAD5){
                mouse_held = false;
                // Obtain cursor coords in active window
                POINT pt;
                GetCursorPos(&pt);
                ScreenToClient(active_window, &pt);

                PostMessage(active_window, WM_LBUTTONUP, 0, MAKELPARAM(pt.x, pt.y));
                // Return non-zero value to prevent default event bubbling
                return -1;
            }
        } break;

        default: {
        }
        }  // end of message switch
}
    // else pass off processing to the next hook
    return CallNextHookEx(0, nCode, wParam, lParam);
}

