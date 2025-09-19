#include <Windows.h>
#include <d3d9.h>
#include "include/imgui/imgui.h"
#include "include/imgui/imgui_impl_dx9.h"
#include "include/imgui/imgui_impl_win32.h"

#pragma comment(lib, "d3d9.lib")

// Указатели на оригинальные функции
static HMODULE originalD3D9 = nullptr;
decltype(Direct3DCreate9)* originalDirect3DCreate9 = nullptr;

// Типы для оригинальных функций
typedef HRESULT(STDMETHODCALLTYPE* Present_t)(IDirect3DDevice9*, CONST RECT*, CONST RECT*, HWND, CONST RGNDATA*);
typedef HRESULT(STDMETHODCALLTYPE* Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
typedef HRESULT(STDMETHODCALLTYPE* CreateDevice_t)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);

// Оригинальные функции
Present_t OriginalPresent = nullptr;
Reset_t OriginalReset = nullptr;
CreateDevice_t OriginalCreateDevice = nullptr;

// Для перехвата оконных сообщений
WNDPROC OriginalWndProc = nullptr;
HWND g_hWindow = nullptr;

//Моё
bool Show_Window = false;

// Наша обработка оконных сообщений
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK Hooked_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // Всегда передаем сообщения в ImGui для обработки
    ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

    // Получаем состояние ImGui
    ImGuiIO& io = ImGui::GetIO();
    // Обрабатываем нажатие Insert 
    if (uMsg == WM_KEYDOWN && wParam == VK_INSERT)
    {        
            Show_Window = !Show_Window;
            return 0;       
    }

    // Блокируем сообщения мыши, если ImGui хочет их обработать
    if (((uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) && io.WantCaptureMouse) && Show_Window)
    {
        return true;
    }

    // Блокируем сообщения клавиатуры, если ImGui хочет их обработать
    if (((uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST) && io.WantCaptureKeyboard) && Show_Window)
    {
        return true;
    }
    
    return CallWindowProc(OriginalWndProc, hWnd, uMsg, wParam, lParam);
}

// Наши функции-перехватчики
HRESULT STDMETHODCALLTYPE Hooked_Present(IDirect3DDevice9* pDevice, CONST RECT* pSourceRect,
    CONST RECT* pDestRect, HWND hDestWindowOverride,
    CONST RGNDATA* pDirtyRegion)
{
    // Рендеринг ImGui
    if (Show_Window) {
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();

        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));

        // Ваш ImGui код здесь
        ImGui::Begin("Test Window!", &Show_Window, ImGuiWindowFlags_NoMove);
        ImGui::Text("Text!");
        ImGui::Text("Some Text!");
        ImGui::End();
        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }
    return OriginalPresent(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

HRESULT STDMETHODCALLTYPE Hooked_Reset(IDirect3DDevice9* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
    ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = OriginalReset(pDevice, pPresentationParameters);
    if (SUCCEEDED(hr))
    {
        ImGui_ImplDX9_CreateDeviceObjects();
    }
    return hr;
}

// Функция для перехвата методов устройства
void HookDevice(IDirect3DDevice9* pDevice)
{
    // Получаем указатель на таблицу виртуальных методов
    void** vTable = *((void***)pDevice);

    // Сохраняем оригинальные указатели
    OriginalPresent = (Present_t)vTable[17]; // Present обычно имеет индекс 17
    OriginalReset = (Reset_t)vTable[16];     // Reset обычно имеет индекс 16

    // Меняем защиту памяти для записи
    DWORD oldProtect;
    VirtualProtect(&vTable[17], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);

    // Заменяем указатели на наши функции
    vTable[17] = (void*)Hooked_Present;
    vTable[16] = (void*)Hooked_Reset;

    // Восстанавливаем защиту
    VirtualProtect(&vTable[17], sizeof(void*), oldProtect, &oldProtect);
}

// Перехваченный CreateDevice
HRESULT STDMETHODCALLTYPE Hooked_CreateDevice(IDirect3D9* pD3D, UINT Adapter, D3DDEVTYPE DeviceType,
    HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS* pPresentationParameters,
    IDirect3DDevice9** ppReturnedDeviceInterface)
{
    // Вызываем оригинальный CreateDevice
    HRESULT hr = OriginalCreateDevice(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags,
        pPresentationParameters, ppReturnedDeviceInterface);

    if (SUCCEEDED(hr))
    {
        // Сохраняем handle окна
        g_hWindow = hFocusWindow;

        // Инициализируем ImGui
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = nullptr;

        ImGui_ImplWin32_Init(hFocusWindow);
        ImGui_ImplDX9_Init(*ppReturnedDeviceInterface);

        // Перехватываем оконную процедуру
        OriginalWndProc = (WNDPROC)SetWindowLongPtr(hFocusWindow, GWLP_WNDPROC, (LONG_PTR)Hooked_WndProc);

        // Перехватываем методы устройства
        HookDevice(*ppReturnedDeviceInterface);
    }

    return hr;
}

// Перехваченная Direct3DCreate9
IDirect3D9* WINAPI Direct3DCreate9(UINT SDKVersion)
{
    if (!originalDirect3DCreate9)
    {
        return nullptr;
    }

    // Создаем оригинальный объект IDirect3D9
    IDirect3D9* pD3D = originalDirect3DCreate9(SDKVersion);
    if (!pD3D)
        return nullptr;

    // Перехватываем виртуальную таблицу IDirect3D9
    void** vTable = *((void***)pD3D);
    OriginalCreateDevice = (CreateDevice_t)vTable[16]; // CreateDevice имеет индекс 16

    // Заменяем CreateDevice на наш перехватчик
    DWORD oldProtect;
    VirtualProtect(&vTable[16], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
    vTable[16] = (void*)Hooked_CreateDevice;
    VirtualProtect(&vTable[16], sizeof(void*), oldProtect, &oldProtect);

    return pD3D;
}

// Точка входа DLL
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        // Загружаем оригинальную d3d9.dll
        char systemPath[MAX_PATH];
        GetSystemWow64DirectoryA(systemPath, MAX_PATH);
        strcat_s(systemPath, "\\d3d9.dll");

        originalD3D9 = LoadLibraryA(systemPath);
        if (!originalD3D9)
        {
            return FALSE;
        }

        originalDirect3DCreate9 = reinterpret_cast<decltype(Direct3DCreate9)*>(
            GetProcAddress(originalD3D9, "Direct3DCreate9"));

        if (!originalDirect3DCreate9)
        {
            FreeLibrary(originalD3D9);
            return FALSE;
        }
    }
    //else if (ul_reason_for_call == DLL_PROCESS_DETACH)
    //{


    //    // Освобождаем ресурсы ImGui
    //    ImGui_ImplDX9_Shutdown();
    //    ImGui_ImplWin32_Shutdown();
    //    ImGui::DestroyContext();

    //    // Восстанавливаем оригинальную оконную процедуру
    //    if (g_hWindow && OriginalWndProc)
    //    {
    //        SetWindowLongPtr(g_hWindow, GWLP_WNDPROC, (LONG_PTR)OriginalWndProc);
    //    }

    //    if (originalD3D9)
    //    {
    //        FreeLibrary(originalD3D9);
    //    }
    //}
    return TRUE;
}