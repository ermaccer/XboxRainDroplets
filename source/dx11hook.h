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

class DX11Hook 
{
public:
	static inline uintptr_t presentPtr = 0;
	static inline uintptr_t resizeBuffersPtr = 0;

	static inline uintptr_t presentOriginalPtr = 0;
	static inline uintptr_t resizeBuffersOriginalPtr = 0;

	static inline ID3D11Device* dev = nullptr;
	static inline ID3D11DeviceContext* devcon;

	static inline Event<IDXGISwapChain*> onInitEvent = {};
	static inline Event<> onShutdownEvent = {};
	static inline Event<IDXGISwapChain*> onPresentEvent = {};
	static inline Event<IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT> onBeforeResizeEvent = {};
	static inline Event<IDXGISwapChain*, UINT, UINT, UINT, DXGI_FORMAT, UINT> onAfterResizeEvent = {};

	~DX11Hook() {
		if (devcon) {
			devcon->Release();
			devcon = nullptr;
		}

		if (dev) {
			dev->Release();
			dev = nullptr;
		}

		MH_DisableHook((void*)presentPtr);
		MH_DisableHook((void*)resizeBuffersPtr);
		onShutdownEvent();
	}

	static inline void Init() {
		HWND hWnd = GetForegroundWindow();
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
		swapChainDescription.SampleDesc.Quality = 0;

		HRESULT hResult = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_REFERENCE, nullptr, 0, nullptr, 0,
			D3D11_SDK_VERSION, &swapChainDescription, &swapChain, &dev, nullptr, &devcon);

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

struct ConstantBuffer
{
	DirectX::XMMATRIX matrix;
};

struct Vertex 
{
	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 col;
	DirectX::XMFLOAT2 uv0;
	DirectX::XMFLOAT2 uv1;
};

class ImRenderer
{
public:
	inline enum
	{
		PROJECTION_ORTHO,
		PROJECTION_PERSPECTIVE,
	};

private:
	static inline ID3D11Device* dev = nullptr;
	static inline ID3D11DeviceContext* devcon = nullptr;

	static inline ID3D11Buffer* vb = nullptr;
	static inline ID3D11Buffer* ib = nullptr;
	static inline ID3D11VertexShader* vs = nullptr;
	static inline ID3D11PixelShader* ps = nullptr;
	static inline ID3D11InputLayout* layout = nullptr;
	static inline ID3D11RenderTargetView* target = nullptr;

	static inline std::vector<Vertex> vertices = {};
	static inline DirectX::XMFLOAT4 color = {};
	static inline DirectX::XMFLOAT2 uv0 = {};
	static inline DirectX::XMFLOAT2 uv1 = {};

	static inline std::vector<uint16_t> indices = {};
	static inline uint32_t numIndices;

	static inline ConstantBuffer cb = {};
	static inline ID3D11Buffer* pb = nullptr;

	static inline uint8_t projectionMode = PROJECTION_ORTHO;

	static inline D3D11_VIEWPORT viewport = {};

	static inline float fov = 60.0f;
	static inline float nearPlane = 0.0f;
	static inline float farPlane = 1.0f;

	static inline ID3D11ShaderResourceView* tex;
	static inline ID3D11ShaderResourceView* mask;

	static inline ID3D11SamplerState* samplerState = nullptr;

public:
	static inline void Begin()
	{
		vertices.clear();
		indices.clear();
		numIndices = 0;

		color = { 1.0f, 1.0f, 1.0f, 1.0f };
		uv0 = { 0.0f, 0.0f };
		uv1 = { 0.0f, 0.0f };
	}

	static inline void End()
	{
		Update();
		Render();
	}

	static inline void SetColor4f(float r, float g, float b, float a)
	{
		color = { r, g, b, a };
	}

	static inline void SetColor3f(float r, float g, float b)
	{
		SetColor4f(r, g, b, 1.0f);
	}

	static inline void SetTexCoords4f(float x1, float y1, float x2, float y2) 
	{
		uv0 = { x1, y1 };
		uv1 = { x2, y2 };
	}

	static inline void SetTexCoords2f(float x, float y) {
		SetTexCoords4f(x, y, x, y);
	}

	static inline void SetVertex3f(float x, float y, float z)
	{
		Vertex v;
		v.pos.x = x;
		v.pos.y = y;
		v.pos.z = z;
		v.col = color;
		v.uv0 = uv0;
		v.uv1 = uv1;

		vertices.push_back(v);
	}

	static inline void SetIndex1i(uint16_t i) 
	{
		indices.push_back(i);
	}

	static inline void SetIndices(std::vector<uint16_t> const& i, int32_t n) 
	{
		indices = i;
		numIndices = n;
	}

	static inline void SetVertex2f(float x, float y)
	{
		SetVertex3f(x, y, 0.0f);
	}

	static inline void Init(ID3D11Device* device, ID3D11DeviceContext* context)
	{
		dev = device;
		devcon = context;

		D3D11_BUFFER_DESC bufferDesc;
		ZeroMemory(&bufferDesc, sizeof(bufferDesc));

		bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bufferDesc.MiscFlags = 0;
		bufferDesc.ByteWidth = sizeof(Vertex) * 65536;

		// Vertex buffer
		dev->CreateBuffer(&bufferDesc, nullptr, &vb);

		// Index buffer
		bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		dev->CreateBuffer(&bufferDesc, nullptr, &ib);

		// Constant buffer
		bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bufferDesc.ByteWidth = sizeof(ConstantBuffer);

		D3D11_SUBRESOURCE_DATA initData;
		ZeroMemory(&initData, sizeof(initData));
		initData.pSysMem = &cb;
		dev->CreateBuffer(&bufferDesc, &initData, &pb);

		D3D11_SAMPLER_DESC samplerDesc;
		ZeroMemory(&samplerDesc, sizeof(samplerDesc));
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		samplerDesc.BorderColor[0] = 0.0f;
		samplerDesc.BorderColor[1] = 0.0f;
		samplerDesc.BorderColor[2] = 0.0f;
		samplerDesc.BorderColor[3] = 0.0f;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
		device->CreateSamplerState(&samplerDesc, &samplerState);
	}

	static inline void Shutdown()
	{
		if (vb)
		{
			vb->Release();
			vb = nullptr;
		}

		if (ib)
		{
			ib->Release();
			ib = nullptr;
		}

		if (pb)
		{
			pb->Release();
			pb = nullptr;
		}

		dev = nullptr;
		devcon = nullptr;
	}

	static inline void SetInputLayout(ID3D11InputLayout* l)
	{
		layout = l;
	}

	static inline void SetRenderTarget(ID3D11RenderTargetView* t)
	{
		target = t;
	}

	static inline void SetShaders(ID3D11VertexShader* v, ID3D11PixelShader* p)
	{
		vs = v;
		ps = p;
	}

	static inline void SetProjectionMode(uint8_t p)
	{
		projectionMode = p;

		switch (p) {
		case PROJECTION_ORTHO:
			cb.matrix = DirectX::XMMatrixOrthographicOffCenterLH(0, viewport.Width, viewport.Height, 0, nearPlane, farPlane);
			break;
		case PROJECTION_PERSPECTIVE:
			cb.matrix = DirectX::XMMatrixPerspectiveFovLH(fov, viewport.Width / viewport.Height, nearPlane, farPlane);
			break;
		}
	}

	static inline void SetViewport(float x, float y, float w, float h) 
	{
		viewport.TopLeftX = x;
		viewport.TopLeftY = y;
		viewport.Width = w;
		viewport.Height = h;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
	}

	static inline void SetTexture(ID3D11ShaderResourceView* texture, ID3D11ShaderResourceView* textureMask = nullptr)
	{
		tex = texture;
		mask = textureMask;
	}

	static inline ID3D11Texture2D* CreateTexture(uint32_t width, uint32_t height, uint8_t channels, uint8_t* pixels)
	{
		ID3D11Texture2D* texture = nullptr;
		D3D11_TEXTURE2D_DESC desc;
		ZeroMemory(&desc, sizeof(desc));
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;

		//D3D11_SUBRESOURCE_DATA initialData;
		//ZeroMemory(&initialData, sizeof(initialData));
		//initialData.pSysMem = pixels;
		//initialData.SysMemPitch = width * sizeof(uint32_t);

		dev->CreateTexture2D(&desc, nullptr, &texture);

		if (pixels)
			UpdateTexture(texture, pixels);

		return texture;
	}

	static inline void UpdateTexture(ID3D11Texture2D* texture, uint8_t* pixels) {
		D3D11_TEXTURE2D_DESC desc;
		texture->GetDesc(&desc);
		devcon->UpdateSubresource(texture, 0, nullptr, pixels, desc.Width * sizeof(UINT), 0);
	}

private:
	static inline void Update()
	{
		// Deduce indices automatically if unset.
		if (indices.empty()) 
		{
			uint16_t i = 0;
			for (auto& it : vertices) 
			{
				indices.push_back(i++);
			}

			numIndices = indices.size();
		}

		// Update index/vertex buffers
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		devcon->Map(vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		memcpy(mappedResource.pData, vertices.data(), vertices.size() * sizeof(Vertex));
		devcon->Unmap(vb, 0);

		devcon->Map(ib, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		memcpy(mappedResource.pData, indices.data(), numIndices * sizeof(uint16_t));
		devcon->Unmap(ib, 0);

		// Update constant
		cb.matrix = DirectX::XMMatrixTranspose(cb.matrix);

		devcon->Map(pb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		memcpy(mappedResource.pData, &cb, sizeof(cb));
		devcon->Unmap(pb, 0);

		devcon->UpdateSubresource(pb, 0, nullptr, &cb, 0, 0);
	}

	static inline void Render()
	{
		// Set viewport
		devcon->RSSetViewports(1, &viewport);

		// Set matrix
		devcon->VSSetConstantBuffers(0, 1, &pb);

		// Set index/vertex buffers
		UINT stride = sizeof(Vertex);
		UINT offset = 0;
		devcon->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
		devcon->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);
		devcon->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Set vertex buffer layout
		devcon->IASetInputLayout(layout);

		// Set render target
		devcon->OMSetRenderTargets(1, &target, nullptr);

		// Set shaders
		devcon->VSSetShader(vs, nullptr, 0);
		devcon->PSSetShader(ps, nullptr, 0);

		// Set texture
		devcon->PSSetShaderResources(0, 1, &tex);
		devcon->PSSetShaderResources(1, 1, &mask);

		devcon->PSSetSamplers(0, 1, &samplerState);

		// Draw
		devcon->DrawIndexed(numIndices, 0, 0);
	}
};
