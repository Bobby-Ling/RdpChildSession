#include "pch.h"
#include "framework.h"
#include "resource.h"
#include "RdpChildSessionATL_i.h"
#include "xdlldata.h"

// for WTSEnableChildSessions
#pragma comment(lib, "wtsapi32.lib")
#include <wtsapi32.h>


#include <atlbase.h>
#include <atlcom.h>
#include <atlctl.h>
#include <atlwin.h>
#include <atltypes.h>
#include <atlhost.h>

#include <comdef.h>
// #define CHECKHR(hr) if (FAILED(hr)) { MessageBoxW(NULL, _com_error(hr, nullptr).ErrorMessage(), L"错误", MB_ICONERROR); return 1; }
inline bool CHECKHR(HRESULT hr, LPCWSTR msg = L"") {
    if (FAILED(hr)) {
        MessageBoxW(NULL, msg, _com_error(hr, nullptr).ErrorMessage(), MB_ICONERROR);
        return false;
    }
    return true;
}

using namespace ATL;

// 窗口类名
const TCHAR* g_szWindowClass = TEXT("RdpChildSessionATLWindowClass");
const TCHAR* g_szTitle = TEXT("RDP子会话");

// 窗口句柄
HWND g_hWnd = NULL;

// RDP控件指针
CComPtr<IMsRdpClient10> g_pRdpClient = NULL;

// ATL宿主窗口
CAxWindow* g_pAxWindow = NULL;

// 窗口过程函数
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

// 创建RDP控件
HRESULT CreateRdpControl(HWND hWnd);

//
HRESULT EnableChildSession();

// 连接到RDP子会话
HRESULT ConnectToChildSession();

class CRdpChildSessionATLModule : public ATL::CAtlExeModuleT< CRdpChildSessionATLModule >
{
public:
    DECLARE_LIBID(LIBID_RdpChildSessionATLLib)
    DECLARE_REGISTRY_APPID_RESOURCEID(IDR_RDPCHILDSESSIONATL, "{3b23976a-dd44-4e89-8bb7-a5ba20bad4b8}")
};

CRdpChildSessionATLModule _AtlModule;

extern "C" int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
    LPTSTR lpCmdLine, int nShowCmd)
{
    // 初始化COM
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr))
    {
        MessageBox(NULL, TEXT("COM初始化失败"), TEXT("错误"), MB_OK | MB_ICONERROR);
        return 1;
    }

    // 似乎不需要也能运行
    // hr = EnableChildSession();
    if (FAILED(hr))
    {
        CoUninitialize();
        return 1;
    }

    WNDCLASSEX wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = g_szWindowClass;

    if (!RegisterClassEx(&wcex))
    {
        MessageBox(NULL, TEXT("注册窗口类失败"), TEXT("错误"), MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    g_hWnd = CreateWindow(
        g_szWindowClass,
        g_szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        NULL,
        NULL,
        hInstance,
        NULL);

    if (!g_hWnd)
    {
        MessageBox(NULL, TEXT("创建窗口失败"), TEXT("错误"), MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    // 创建RDP控件
    hr = CreateRdpControl(g_hWnd);
    if (FAILED(hr))
    {
        MessageBox(NULL, TEXT("创建RDP控件失败"), TEXT("错误"), MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    ShowWindow(g_hWnd, nShowCmd);
    UpdateWindow(g_hWnd);

    hr = ConnectToChildSession();
    if (FAILED(hr))
    {
        MessageBox(NULL, TEXT("连接到子会话失败"), TEXT("错误"), MB_OK | MB_ICONERROR);
        CHECKHR(hr);
        CoUninitialize();
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_pRdpClient.Release();
    CoUninitialize();

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_SIZE:
        if (g_pAxWindow && g_pAxWindow->m_hWnd)
        {
            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            g_pAxWindow->MoveWindow(&rcClient);
        }
        break;

    case WM_DESTROY:
        if (g_pAxWindow)
        {
            if (g_pAxWindow->m_hWnd)
            {
                g_pAxWindow->DestroyWindow();
            }
            delete g_pAxWindow;
            g_pAxWindow = NULL;
        }

        g_pRdpClient.Release();

        WTSEnableChildSessions(FALSE);

        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

HRESULT CreateRdpControl(HWND hWnd)
{
    RECT rcClient;
    GetClientRect(hWnd, &rcClient);

    // 创建ATL宿主窗口
    g_pAxWindow = new CAxWindow();
    if (!g_pAxWindow)
    {
        return E_OUTOFMEMORY;
    }

    // 创建宿主窗口
    g_pAxWindow->Create(hWnd, rcClient, NULL, WS_CHILD | WS_VISIBLE);
    if (!g_pAxWindow->m_hWnd)
    {
        delete g_pAxWindow;
        g_pAxWindow = NULL;
        return E_FAIL;
    }

    // 创建RDP控件
    LPOLESTR clsidString = nullptr;
    HRESULT hr = NULL;

    CHECKHR(hr = StringFromCLSID(CLSID_MsRdpClient9NotSafeForScripting, &clsidString));
    // hr = g_pAxWindow->CreateControl(L"{7584c670-2274-4efb-b00b-d6aaba6d3850}"); // CLSID_MsRdpClient9
    CHECKHR(hr = g_pAxWindow->CreateControl(clsidString));
    CoTaskMemFree(clsidString);

    CHECKHR(hr = g_pAxWindow->QueryControl(IID_IMsRdpClient9, (void**)&g_pRdpClient));

    CComPtr<IMsRdpClientNonScriptable7> pNonScriptable;
    CHECKHR(hr = g_pRdpClient->QueryInterface(IID_IMsRdpClientNonScriptable, (void**)&pNonScriptable));
    CHECKHR(hr = pNonScriptable->put_UIParentWindowHandle((wireHWND)hWnd));

    // https://learn.microsoft.com/en-us/windows/win32/termserv/imsrdpclientadvancedsettings8
    CComPtr<IMsRdpClientAdvancedSettings8> pAdvancedSettings;
    CHECKHR(hr = g_pRdpClient->get_AdvancedSettings9(&pAdvancedSettings));

    CHECKHR(hr = pAdvancedSettings->put_AuthenticationLevel(0));
    CHECKHR(hr = pAdvancedSettings->put_EnableCredSspSupport(TRUE));
    CHECKHR(hr = pAdvancedSettings->put_RedirectDrives(FALSE));
    CHECKHR(hr = pAdvancedSettings->put_RedirectPrinters(FALSE));
    CHECKHR(hr = pAdvancedSettings->put_RedirectClipboard(TRUE));
    CHECKHR(hr = pAdvancedSettings->put_SmoothScroll(1));
    // CHECKHR(hr = pAdvancedSettings->put_allowBackgroundInput(1));

    // CComPtr<IMsRdpClientSecuredSettings2> pSecuredSettings;
    // CHECKHR(hr = g_pRdpClient->get_SecuredSettings3(&pSecuredSettings));

    // CHECKHR(pSecuredSettings->put_KeyboardHookMode(1));

    return S_OK;
}

HRESULT EnableChildSession()
{
    // 启用子会话功能
    if (!WTSEnableChildSessions(TRUE))
    {
        MessageBox(NULL, TEXT("启用子会话功能失败"), TEXT("错误"), MB_OK | MB_ICONERROR);
        return E_UNEXPECTED;
    }

    BOOL bEnabled = FALSE;
    if (!WTSIsChildSessionsEnabled(&bEnabled) || !bEnabled)
    {
        MessageBox(NULL, TEXT("子会话功能未启用"), TEXT("错误"), MB_OK | MB_ICONERROR);
        return E_UNEXPECTED;
    }
    return S_OK;
}

HRESULT ConnectToChildSession()
{
    if (!g_pRdpClient)
    {
        return E_POINTER;
    }

    CHECKHR(g_pRdpClient->put_Server(TEXT("localhost")));

    CComPtr<IMsRdpExtendedSettings> pExtendedSettings;
    HRESULT hr = g_pRdpClient->QueryInterface(IID_IMsRdpExtendedSettings, (void**)&pExtendedSettings);
    if (FAILED(hr))
    {
        return hr;
    }

    // see https://learn.microsoft.com/en-us/windows/win32/termserv/child-sessions
    CHECKHR(pExtendedSettings->put_Property(CComBSTR("ConnectToChildSession"), &CComVariant(true)));

    CHECKHR(pExtendedSettings->put_Property(CComBSTR("EnableFrameBufferRedirection"), &CComVariant(true)));
    CHECKHR(pExtendedSettings->put_Property(CComBSTR("EnableHardwareMode"), &CComVariant(true)));
    // 设置帧率: https://learn.microsoft.com/en-us/troubleshoot/windows-server/remote/frame-rate-limited-to-30-fps
    // 设置HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\Terminal Server\WinStations\DWMFRAMEINTERVAL为10~15可以极大提升流畅度


    hr = g_pRdpClient->Connect();
    if (FAILED(hr))
    {
        return hr;
    }

    return S_OK;
}

