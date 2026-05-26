#include "gui/GuiOverlay.h"

#if AIM_BUILD_GUI
#include <windows.h>
#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>
#include <d3d11.h>
#include <shellapi.h>
#include <tchar.h>

#pragma comment(lib, "d3d11.lib")

namespace {
ID3D11Device* gDevice = nullptr;
ID3D11DeviceContext* gContext = nullptr;
IDXGISwapChain* gSwapChain = nullptr;
ID3D11RenderTargetView* gRenderTargetView = nullptr;
HWND gHwnd = nullptr;
constexpr UINT WM_TRAYICON = WM_APP + 42;
NOTIFYICONDATA gTrayIcon{};
bool gTrayAdded = false;

void addTrayIcon(HWND hwnd) {
    if (gTrayAdded) return;
    gTrayIcon = {};
    gTrayIcon.cbSize = sizeof(NOTIFYICONDATA);
    gTrayIcon.hWnd = hwnd;
    gTrayIcon.uID = 1;
    gTrayIcon.uFlags = NIF_MESSAGE | NIF_TIP | NIF_ICON;
    gTrayIcon.uCallbackMessage = WM_TRAYICON;
    gTrayIcon.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    lstrcpy(gTrayIcon.szTip, _T("CppAimMinimal"));
    gTrayAdded = Shell_NotifyIcon(NIM_ADD, &gTrayIcon) == TRUE;
}

void removeTrayIcon() {
    if (gTrayAdded) {
        Shell_NotifyIcon(NIM_DELETE, &gTrayIcon);
        gTrayAdded = false;
    }
}

void restoreFromTray() {
    if (!gHwnd) return;
    ShowWindow(gHwnd, SW_SHOW);
    SetForegroundWindow(gHwnd);
}

void createRenderTarget() {
    ID3D11Texture2D* backBuffer = nullptr;
    gSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    gDevice->CreateRenderTargetView(backBuffer, nullptr, &gRenderTargetView);
    backBuffer->Release();
}

void cleanupRenderTarget() {
    if (gRenderTargetView) {
        gRenderTargetView->Release();
        gRenderTargetView = nullptr;
    }
}

bool createDeviceD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT flags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL levels[2] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, 2,
                                      D3D11_SDK_VERSION, &sd, &gSwapChain, &gDevice,
                                      &featureLevel, &gContext) != S_OK) {
        return false;
    }
    createRenderTarget();
    return true;
}

void cleanupDeviceD3D() {
    cleanupRenderTarget();
    if (gSwapChain) { gSwapChain->Release(); gSwapChain = nullptr; }
    if (gContext) { gContext->Release(); gContext = nullptr; }
    if (gDevice) { gDevice->Release(); gDevice = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
        return true;
    }
    switch (msg) {
    case WM_SIZE:
        if (gDevice != nullptr && wParam != SIZE_MINIMIZED) {
            cleanupRenderTarget();
            gSwapChain->ResizeBuffers(0, LOWORD(lParam), HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            createRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) {
            return 0;
        }
        break;
    case WM_CLOSE:
        addTrayIcon(hwnd);
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    case WM_TRAYICON:
        if (lParam == WM_LBUTTONDBLCLK || lParam == WM_RBUTTONUP) {
            restoreFromTray();
        }
        return 0;
    case WM_DESTROY:
        removeTrayIcon();
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
}
#endif

bool GuiOverlay::init(AppConfig* cfg, bool* reloadModelRequested, bool* saveRequested, bool* exitRequested) {
    cfg_ = cfg;
    reloadModelRequested_ = reloadModelRequested;
    saveRequested_ = saveRequested;
    exitRequested_ = exitRequested;
#if AIM_BUILD_GUI
    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, wndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, _T("CppAimMinimalGui"), nullptr };
    RegisterClassEx(&wc);
    gHwnd = CreateWindow(wc.lpszClassName, _T("Aim Settings"), WS_OVERLAPPEDWINDOW, 100, 100, 430, 420, nullptr, nullptr, wc.hInstance, nullptr);
    if (!createDeviceD3D(gHwnd)) {
        cleanupDeviceD3D();
        UnregisterClass(wc.lpszClassName, wc.hInstance);
        return false;
    }
    ShowWindow(gHwnd, SW_SHOWDEFAULT);
    UpdateWindow(gHwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(gHwnd);
    ImGui_ImplDX11_Init(gDevice, gContext);
#endif
    return true;
}

void GuiOverlay::shutdown() {
#if AIM_BUILD_GUI
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanupDeviceD3D();
    removeTrayIcon();
    if (gHwnd) {
        DestroyWindow(gHwnd);
        gHwnd = nullptr;
    }
#endif
}

void GuiOverlay::newFrame() {
#if AIM_BUILD_GUI
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (msg.message == WM_QUIT) {
            wantsExit_ = true;
            if (exitRequested_) *exitRequested_ = true;
        }
    }
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
#endif
}

void GuiOverlay::render() {
#if AIM_BUILD_GUI
    if (!cfg_) return;
    ImGui::SetNextWindowSize(ImVec2(400, 360), ImGuiCond_FirstUseEver);
    ImGui::Begin("Aim Settings", nullptr, ImGuiWindowFlags_NoCollapse);

    char modelBuf[260]{};
    strncpy_s(modelBuf, cfg_->modelPath.c_str(), sizeof(modelBuf) - 1);
    if (ImGui::InputText("Model", modelBuf, sizeof(modelBuf))) {
        cfg_->modelPath = modelBuf;
    }
    if (ImGui::Button("Reload model") && reloadModelRequested_) {
        *reloadModelRequested_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save") && saveRequested_) {
        *saveRequested_ = true;
    }

    ImGui::Separator();
    ImGui::SliderFloat("Confidence", &cfg_->confThreshold, 0.05f, 0.95f, "%.2f");
    ImGui::SliderFloat("Sensitivity X", &cfg_->sensitivityX, 0.05f, 10.0f, "%.2f");
    ImGui::SliderFloat("Sensitivity Y", &cfg_->sensitivityY, 0.05f, 10.0f, "%.2f");
    ImGui::SliderFloat("Smoothing", &cfg_->smoothing, 0.0f, 10.0f, "%.2f");
    ImGui::SliderFloat("Max speed", &cfg_->maxSpeed, 1.0f, 200.0f, "%.1f");
    ImGui::SliderFloat("Deadzone", &cfg_->deadzone, 0.0f, 5.0f, "%.1f");
    ImGui::Checkbox("Always on", &cfg_->alwaysOn);

    ImGui::Separator();
    ImGui::Text("Capture: %s  %dx%d", cfg_->captureMode.c_str(), cfg_->frameWidth, cfg_->frameHeight);
    if (ImGui::Button("Hide to tray")) {
        addTrayIcon(gHwnd);
        ShowWindow(gHwnd, SW_HIDE);
    }
    ImGui::SameLine();
    if (ImGui::Button("Exit") && exitRequested_) {
        *exitRequested_ = true;
    }
    ImGui::End();

    ImGui::Render();
    const float clearColor[4] = {0.08f, 0.08f, 0.08f, 1.0f};
    gContext->OMSetRenderTargets(1, &gRenderTargetView, nullptr);
    gContext->ClearRenderTargetView(gRenderTargetView, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    gSwapChain->Present(1, 0);
#endif
}
