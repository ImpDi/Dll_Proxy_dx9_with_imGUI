#include <Windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <math.h>
#include "include/imgui/imgui.h"
#include "include/imgui/imgui_impl_dx9.h"
#include "include/imgui/imgui_impl_win32.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

// ��������� �� ������������ �������
static HMODULE originalD3D9 = nullptr;
decltype(Direct3DCreate9)* originalDirect3DCreate9 = nullptr;

// ���� ��� ������������ �������
typedef HRESULT(STDMETHODCALLTYPE* Present_t)(IDirect3DDevice9*, CONST RECT*, CONST RECT*, HWND, CONST RGNDATA*);
typedef HRESULT(STDMETHODCALLTYPE* Reset_t)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
typedef HRESULT(STDMETHODCALLTYPE* CreateDevice_t)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
typedef HRESULT(STDMETHODCALLTYPE* SetVertexShaderConstantF_t)(IDirect3DDevice9*, UINT, CONST float*, UINT);

// ������������ �������
Present_t OriginalPresent = nullptr;
Reset_t OriginalReset = nullptr;
CreateDevice_t OriginalCreateDevice = nullptr;
SetVertexShaderConstantF_t OriginalSetVertexShaderConstantF = nullptr;

// ��� ��������� ������� ���������
WNDPROC OriginalWndProc = nullptr;
HWND g_hWindow = nullptr;

// ��������� ������
float CameraOffset[3] = { 0.0f, 0.0f, 0.0f };
float CameraRotation[2] = { 0.0f, 0.0f };

// ���������� ��� ��������� ��������
bool ModifyViewMatrix = true;
UINT ViewMatrixRegister = 0;

//��
bool Show_Window = false;

// ���� ��������� ������� ���������
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK Hooked_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // ������ �������� ��������� � ImGui ��� ���������
    ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);

    // �������� ��������� ImGui
    ImGuiIO& io = ImGui::GetIO();
    // ������������ ������� Insert 
    if (uMsg == WM_KEYDOWN && wParam == VK_INSERT)
    {        
            Show_Window = !Show_Window;
            return 0;       
    }

    // ��������� ��������� ����, ���� ImGui ����� �� ����������
    if (((uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) && io.WantCaptureMouse) && Show_Window)
    {
        return true;
    }

    // ��������� ��������� ����������, ���� ImGui ����� �� ����������
    if (((uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST) && io.WantCaptureKeyboard) && Show_Window)
    {
        return true;
    }
    
    return CallWindowProc(OriginalWndProc, hWnd, uMsg, wParam, lParam);
}

D3DXMATRIX CreateRotationMatrix(float pitch, float yaw) {
    D3DXMATRIX rotationMatrix;
    D3DXMatrixRotationYawPitchRoll(&rotationMatrix, yaw, pitch, 0.0f);
    return rotationMatrix;
}

// ����������� SetVertexShaderConstantF
HRESULT STDMETHODCALLTYPE Hooked_SetVertexShaderConstantF(IDirect3DDevice9* pDevice, UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) {
    // ���� ��� ������� ���� � �� ����� � ��������������
    if (ModifyViewMatrix && StartRegister == ViewMatrixRegister && Vector4fCount >= 4) {
        // �������� ������������ ������
        float modifiedConstants[16];
        memcpy(modifiedConstants, pConstantData, sizeof(modifiedConstants));

        // ����������� � �������
        D3DXMATRIX viewMatrix(modifiedConstants);

        // ��������� ��������
        D3DXMATRIX translationMatrix;
        D3DXMatrixTranslation(&translationMatrix, CameraOffset[0], CameraOffset[1], CameraOffset[2]);
        viewMatrix = viewMatrix * translationMatrix;

        // ��������� ��������
        if (CameraRotation[0] != 0.0f || CameraRotation[1] != 0.0f) {
            D3DXMATRIX rotationMatrix = CreateRotationMatrix(CameraRotation[0], CameraRotation[1]);
            viewMatrix = viewMatrix * rotationMatrix;
        }

        // �������� ���������������� ������� ������� � ���������
        memcpy(modifiedConstants, &viewMatrix, sizeof(modifiedConstants));

        return OriginalSetVertexShaderConstantF(pDevice, StartRegister, modifiedConstants, Vector4fCount);
    }

    return OriginalSetVertexShaderConstantF(pDevice, StartRegister, pConstantData, Vector4fCount);
}
// ���� �������-������������
HRESULT STDMETHODCALLTYPE Hooked_Present(IDirect3DDevice9* pDevice, CONST RECT* pSourceRect,
    CONST RECT* pDestRect, HWND hDestWindowOverride,
    CONST RGNDATA* pDirtyRegion)
{
    // ��������� ImGui
    if (Show_Window) {
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();

        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));

        // ���� ���������� �������
        ImGui::Begin("Camera Control", &Show_Window, ImGuiWindowFlags_NoMove);
        ImGui::Text("Camera Position");
        ImGui::SliderFloat("X", &CameraOffset[0], -100.0f, 100.0f);
        ImGui::SliderFloat("Y", &CameraOffset[1], -100.0f, 100.0f);
        ImGui::SliderFloat("Z", &CameraOffset[2], -100.0f, 100.0f);
        ImGui::Text("Camera Rotation");
        ImGui::SliderFloat("Pitch", &CameraRotation[0], -3.14f, 3.14f);
        ImGui::SliderFloat("Yaw", &CameraRotation[1], -3.14f, 3.14f);
        ImGui::Checkbox("Modify View Matrix", &ModifyViewMatrix);
        ImGui::InputInt("View Matrix Register", (int*)&ViewMatrixRegister);
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

// ������� ��� ��������� ������� ����������
void HookDevice(IDirect3DDevice9* pDevice)
{
    // �������� ��������� �� ������� ����������� �������
    void** vTable = *((void***)pDevice);

    // ��������� ������������ ���������
    OriginalPresent = (Present_t)vTable[17]; // Present ������ ����� ������ 17
    OriginalReset = (Reset_t)vTable[16];     // Reset ������ ����� ������ 16
    OriginalSetVertexShaderConstantF = (SetVertexShaderConstantF_t)vTable[94];

    // ������ ������ ������ ��� ������
    DWORD oldProtect;
    VirtualProtect(&vTable[17], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
    VirtualProtect(&vTable[16], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
    VirtualProtect(&vTable[94], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
    
    // �������� ��������� �� ���� �������
    vTable[17] = (void*)Hooked_Present;
    vTable[16] = (void*)Hooked_Reset;
    vTable[94] = (void*)Hooked_SetVertexShaderConstantF;

    // ��������������� ������
    VirtualProtect(&vTable[17], sizeof(void*), oldProtect, &oldProtect);
    VirtualProtect(&vTable[16], sizeof(void*), oldProtect, &oldProtect);
    VirtualProtect(&vTable[94], sizeof(void*), oldProtect, &oldProtect);
}

// ������������� CreateDevice
HRESULT STDMETHODCALLTYPE Hooked_CreateDevice(IDirect3D9* pD3D, UINT Adapter, D3DDEVTYPE DeviceType,
    HWND hFocusWindow, DWORD BehaviorFlags,
    D3DPRESENT_PARAMETERS* pPresentationParameters,
    IDirect3DDevice9** ppReturnedDeviceInterface)
{
    // �������� ������������ CreateDevice
    HRESULT hr = OriginalCreateDevice(pD3D, Adapter, DeviceType, hFocusWindow, BehaviorFlags,
        pPresentationParameters, ppReturnedDeviceInterface);

    if (SUCCEEDED(hr))
    {
        // ��������� handle ����
        g_hWindow = hFocusWindow;

        // �������������� ImGui
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.IniFilename = nullptr;

        ImGui_ImplWin32_Init(hFocusWindow);
        ImGui_ImplDX9_Init(*ppReturnedDeviceInterface);

        // ������������� ������� ���������
        OriginalWndProc = (WNDPROC)SetWindowLongPtr(hFocusWindow, GWLP_WNDPROC, (LONG_PTR)Hooked_WndProc);

        // ������������� ������ ����������
        HookDevice(*ppReturnedDeviceInterface);
    }

    return hr;
}

// ������������� Direct3DCreate9
IDirect3D9* WINAPI Direct3DCreate9(UINT SDKVersion)
{
    if (!originalDirect3DCreate9)
    {
        return nullptr;
    }

    // ������� ������������ ������ IDirect3D9
    IDirect3D9* pD3D = originalDirect3DCreate9(SDKVersion);
    if (!pD3D)
        return nullptr;

    // ������������� ����������� ������� IDirect3D9
    void** vTable = *((void***)pD3D);
    OriginalCreateDevice = (CreateDevice_t)vTable[16]; // CreateDevice ����� ������ 16

    // �������� CreateDevice �� ��� �����������
    DWORD oldProtect;
    VirtualProtect(&vTable[16], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);
    vTable[16] = (void*)Hooked_CreateDevice;
    VirtualProtect(&vTable[16], sizeof(void*), oldProtect, &oldProtect);

    return pD3D;
}

// ����� ����� DLL
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        // ��������� ������������ d3d9.dll
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


    //    // ����������� ������� ImGui
    //    ImGui_ImplDX9_Shutdown();
    //    ImGui_ImplWin32_Shutdown();
    //    ImGui::DestroyContext();

    //    // ��������������� ������������ ������� ���������
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