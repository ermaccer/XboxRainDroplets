#pragma once
#include "minHook.h"

#pragma region Undefine Windows Macros
#ifndef __dxgitype_h__
#undef DXGI_STATUS_OCCLUDED
#undef DXGI_STATUS_CLIPPED
#undef DXGI_STATUS_NO_REDIRECTION
#undef DXGI_STATUS_NO_DESKTOP_ACCESS
#undef DXGI_STATUS_GRAPHICS_VIDPN_SOURCE_IN_USE
#undef DXGI_STATUS_MODE_CHANGED
#undef DXGI_STATUS_MODE_CHANGE_IN_PROGRESS
#undef DXGI_ERROR_INVALID_CALL
#undef DXGI_ERROR_NOT_FOUND
#undef DXGI_ERROR_MORE_DATA
#undef DXGI_ERROR_UNSUPPORTED
#undef DXGI_ERROR_DEVICE_REMOVED
#undef DXGI_ERROR_DEVICE_HUNG
#undef DXGI_ERROR_DEVICE_RESET
#undef DXGI_ERROR_WAS_STILL_DRAWING
#undef DXGI_ERROR_FRAME_STATISTICS_DISJOINT
#undef DXGI_ERROR_GRAPHICS_VIDPN_SOURCE_IN_USE
#undef DXGI_ERROR_DRIVER_INTERNAL_ERROR
#undef DXGI_ERROR_NONEXCLUSIVE
#undef DXGI_ERROR_NOT_CURRENTLY_AVAILABLE
#undef DXGI_ERROR_REMOTE_CLIENT_DISCONNECTED
#undef DXGI_ERROR_REMOTE_OUTOFMEMORY
#undef D3D11_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS
#undef D3D11_ERROR_FILE_NOT_FOUND
#undef D3D11_ERROR_TOO_MANY_UNIQUE_VIEW_OBJECTS
#undef D3D11_ERROR_DEFERRED_CONTEXT_MAP_WITHOUT_INITIAL_DISCARD
#undef D3D10_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS
#undef D3D10_ERROR_FILE_NOT_FOUND
#endif
#pragma endregion

#include <d3d11.h>
#include <D3DX11.h>
#include <d3dcompiler.h>
#include <functional>
#include <DirectXMath.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dx11.lib")
#pragma comment(lib, "d3dcompiler.lib")

template<typename... Args>
class Event : public std::function<void(Args...)> 
{
public:
	using std::function<void(Args...)>::function;

private:
	std::vector<std::function<void(Args...)>> handlers;

public:
	void operator+=(std::function<void(Args...)> handler)
	{
		handlers.push_back(handler);
	}

	void operator()(Args... args) const
	{
		if (!handlers.empty()) 
		{
			for (auto& handler : handlers)
			{
				handler(args...);
			}
		}
	}
};

struct ConstantBuffer
{
	DirectX::XMMATRIX proj;
};

class DX11Hook 
{
public:
	static inline uintptr_t presentPtr = 0;
	static inline uintptr_t resizeBuffersPtr = 0;

	static inline uintptr_t presentOriginalPtr = 0;
	static inline uintptr_t resizeBuffersOriginalPtr = 0;

	static inline Event<IDXGISwapChain*> onInitEvent = {};
	static inline Event<> onShutdownEvent = {};
	static inline Event<IDXGISwapChain*> onPresentEvent = {};
	static inline Event<IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT> onBeforeResizeEvent = {};
	static inline Event<IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT> onAfterResizeEvent = {};

	~DX11Hook() {
		MH_DisableHook((void*)presentPtr);
		MH_DisableHook((void*)resizeBuffersPtr);
		onShutdownEvent();
	}

	static inline void Init() {
		HWND hWnd = GetDesktopWindow();
		ID3D11Device* device = nullptr;
		ID3D11DeviceContext* context = nullptr;

		IDXGISwapChain* swapChain = nullptr;
		DXGI_SWAP_CHAIN_DESC swapChainDescription;
		ZeroMemory(&swapChainDescription, sizeof(DXGI_SWAP_CHAIN_DESC));

		D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

		swapChainDescription.OutputWindow = hWnd;
		swapChainDescription.Windowed = true;

		swapChainDescription.BufferCount = 1;
		swapChainDescription.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDescription.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		swapChainDescription.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		swapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

		swapChainDescription.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		swapChainDescription.SampleDesc.Count = 1;

		HRESULT hResult = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_NULL, nullptr, D3D11_CREATE_DEVICE_DEBUG, &featureLevel, 1,
			D3D11_SDK_VERSION, &swapChainDescription, &swapChain, &device, nullptr, &context);

		if (FAILED(hResult))
		{
			return;
		}

		uintptr_t* vTable = *(uintptr_t**)(swapChain);

		if (!vTable)
		{
			return;
		}

		presentPtr = vTable[8];
		resizeBuffersPtr = vTable[13];

		assert(presentPtr);
		assert(resizeBuffersPtr);

		MH_Initialize();
		onInitEvent(swapChain);

		swapChain->Release();
		swapChain = nullptr;

		context->Release();
		context = nullptr;

		device->Release();
		device = nullptr;

		if (MH_CreateHook((void*)presentPtr, Present, (void**)&presentOriginalPtr) != MH_OK)
			return;

		MH_EnableHook((void*)presentPtr);

		if (MH_CreateHook((void*)resizeBuffersPtr, ResizeBuffers, (void**)&resizeBuffersOriginalPtr) != MH_OK)
			return;

		MH_EnableHook((void*)resizeBuffersPtr);
	}

	static inline HRESULT WINAPI Present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
		uintptr_t addr = presentOriginalPtr;
		assert(addr != 0);

		onPresentEvent(pSwapChain);

		return ((HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT))addr)(pSwapChain, SyncInterval, Flags);
	}

	static inline HRESULT WINAPI ResizeBuffers(IDXGISwapChain* pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
		uintptr_t addr = resizeBuffersOriginalPtr;
		assert(addr != 0);

		onBeforeResizeEvent(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
		HRESULT result = ((HRESULT(WINAPI*)(IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT))addr)(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
		onAfterResizeEvent(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);

		return result;
	}
};
