#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#include <wrl/client.h>
#ifdef _DEBUG
#include <iostream>
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace std;
using Microsoft::WRL::ComPtr;
// DirectX�̏������ɕK�v�ȃO���[�o���ϐ�
ID3D12Device* device = nullptr;
IDXGIFactory6* dxgiFactory = nullptr;
IDXGISwapChain4* swapChain = nullptr;
ID3D12GraphicsCommandList* commandList = nullptr;
ID3D12CommandAllocator* commandAllocator = nullptr;
ID3D12DescriptorHeap* rtvHeap = nullptr;
ID3D12CommandQueue* commandQueue = nullptr;

ComPtr<ID3D12DebugDevice> debugDevice;

// @brief �R���\�[����ʂɃt�H�[�}�b�g�t����������o�͂���֐�
void DebugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
	va_list args;
	va_start(args, format);
	vprintf(format, args);
	va_end(args);
#endif
}

void EnableDebugLayer()
{
#ifdef _DEBUG
	ID3D12Debug* debugLayer = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer))))
	{
		debugLayer->EnableDebugLayer();
		debugLayer->Release();
	}
	else
	{
		DebugOutputFormatString("Debug Layer�̗L�����Ɏ��s\n");
	}
#endif
}

// @brief DirectX�̏���������
bool InitializeDirectX(HWND hwnd)
{
	DebugOutputFormatString("DirectX�̏��������J�n\n");

	EnableDebugLayer(); // �f�o�b�O���C���[��L����

	// DXGI�t�@�N�g���̍쐬
#ifdef _DEBUG
	if (CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&dxgiFactory)) != S_OK)
	{
		DebugOutputFormatString("DXGI�t�@�N�g���̍쐬�Ɏ��s\n");
		return false;
	}
#else
	if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK)
	{
		DebugOutputFormatString("DXGI�t�@�N�g���̍쐬�Ɏ��s\n");
		return false;
	}
#endif
	// �A�_�v�^�̎擾
	std::vector<IDXGIAdapter*> adapters;

	IDXGIAdapter* tempAdapter = nullptr;

	for (UINT i = 0; dxgiFactory->EnumAdapters(i, &tempAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tempAdapter);
	}
	// �A�_�v�^�̏����擾
	for (size_t i = 0; i < adapters.size(); ++i)
	{
		DXGI_ADAPTER_DESC desc;
		adapters[i]->GetDesc(&desc);
		DebugOutputFormatString("Adapter %d: %ls\n", i, desc.Description);

		// �����ŃA�_�v�^�̏����g���ĉ����������s�����Ƃ��ł��܂�
		if (desc.VendorId == 0x10DE) // NVIDIA�̃A�_�v�^��T��
		{
			DebugOutputFormatString("NVIDIA Adapter found: %ls\n", desc.Description);
			tempAdapter = adapters[i];
			break;
		}
		else if (desc.VendorId == 0x1002) // AMD�̃A�_�v�^��T��
		{
			DebugOutputFormatString("AMD Adapter found: %ls\n", desc.Description);
		}
		else if (desc.VendorId == 0x8086) // Intel�̃A�_�v�^��T��
		{
			DebugOutputFormatString("Intel Adapter found: %ls\n", desc.Description);
		}
	}


	// Device�̍쐬
	D3D_FEATURE_LEVEL levels[] = 
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	D3D_FEATURE_LEVEL featureLevel;

	for (auto level : levels)
	{
		if (D3D12CreateDevice(tempAdapter, level, IID_PPV_ARGS(&device)) == S_OK)
		{
			featureLevel = level;
			break;
		}
	}

	if (device == nullptr)
	{
		DebugOutputFormatString("DirectX�̏������Ɏ��s\n");
		return false;
	}
#ifdef _DEBUG
	device->QueryInterface(IID_PPV_ARGS(&debugDevice));
#endif // _DEBUG


	DebugOutputFormatString("DirectX�̏�����������ɏI��\n");

	//-----
	// �R�}���h���X�g


	if (device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)) != S_OK)
	{
		DebugOutputFormatString("�R�}���h�A���P�[�^�̍쐬�Ɏ��s\n");
		return false;
	}
	
	if (device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr, IID_PPV_ARGS(&commandList)) != S_OK)
	{
		DebugOutputFormatString("�R�}���h���X�g�̍쐬�Ɏ��s\n");
		return false;
	}

	// �R�}���h���X�g�̎��s

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	// �R�}���h���X�g�ƍ��킹�܂��B
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	// �^�C���A�E�g�Ȃ�
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	// �v���C�I���e�B�̎w��͖���
	queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	// �A�_�v�^�[���P�����g�p���Ȃ����͂O�ł悢
	queueDesc.NodeMask = 0;
	if (device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)) != S_OK)
	{
		DebugOutputFormatString("�R�}���h�L���[�̍쐬�Ɏ��s\n");
		return false;
	}

	// �X���b�v�`�F�C���̍쐬
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

	swapChainDesc.Width = 800; // �E�B���h�E�̕�
	swapChainDesc.Height = 600; // �E�B���h�E�̍���
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // �o�b�t�@�̃t�H�[�}�b�g
	swapChainDesc.Stereo = false; // �X�e���I�\���͂��Ȃ�
	
	
	swapChainDesc.SampleDesc.Count = 1; // �}���`�T���v�����O�̐�
	swapChainDesc.SampleDesc.Quality = 0; // �}���`�T���v�����O�̕i��
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER; // �o�b�t�@�̎g�p�@
	swapChainDesc.BufferCount = 2; // �o�b�t�@��

	
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH; // �o�b�N�o�b�t�@�͐L�яk�݉\
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // �t���b�v��͑��₩�ɔj��
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED; // �A���t�@���[�h�͎w�肵�Ȃ�
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH; // �E�B���h�E�A�t���X�N���[�����[�h�ύX������

	if (dxgiFactory->CreateSwapChainForHwnd(
		commandQueue,
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)&swapChain) != S_OK)
	{
		DebugOutputFormatString("�X���b�v�`�F�C���̍쐬�Ɏ��s\n");
		return false;
	}

	// �����_�[�^�[�^�[�Q�b�g�r���[�̍쐬
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = 2; // �o�b�N�o�b�t�@�̐�
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // �����_�[�^�[�Q�b�g�r���[
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // �t���O�͂Ȃ�
	rtvHeapDesc.NodeMask = 0; // �m�[�h�}�X�N��0�i�P��A�_�v�^�[�g�p���j

	if (device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap)) != S_OK)
	{
		DebugOutputFormatString("�����_�[�^�[�Q�b�g�r���[�̃q�[�v�̍쐬�Ɏ��s\n");
		return false;
	}

	// �X���b�v�`�F�C���̎擾
	DXGI_SWAP_CHAIN_DESC swapChainDesc2;
	if (swapChain->GetDesc(&swapChainDesc2) != S_OK)
	{
		DebugOutputFormatString("�X���b�v�`�F�C���̎擾�Ɏ��s\n");
		return false;
	}

	// �o�b�N�o�b�t�@�̎擾
	vector<ID3D12Resource*> backBuffers(swapChainDesc2.BufferCount);
	for (UINT i = 0; i < swapChainDesc2.BufferCount; ++i)
	{
		if (swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i])) != S_OK)
		{
			DebugOutputFormatString("�o�b�N�o�b�t�@�̎擾�Ɏ��s\n");
			return false;
		}
	}

	// �����_�[�^�[�Q�b�g�r���[�̍쐬
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < swapChainDesc2.BufferCount; ++i)
	{
		device->CreateRenderTargetView(backBuffers[i], nullptr, rtvHandle);
		rtvHandle.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}


	return true;
}

void UpdateDirectX(HWND hwnd)
{
	// DirectX�̍X�V�����������ɋL�q
	// �Ⴆ�΁A�����_�����O�⃊�\�[�X�̍X�V�Ȃ�

	// �R�}���h���X�g�̃��Z�b�g
	commandAllocator->Reset();

	auto bbidx = swapChain->GetCurrentBackBufferIndex(); // ���݂̃o�b�N�o�b�t�@�̃C���f�b�N�X���擾

	auto rtvH = rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbidx * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	commandList->OMSetRenderTargets(1, &rtvH, true, nullptr); // �����_�[�^�[�Q�b�g��ݒ�

	// ��ʂ��N���A
	const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f }; // ���F�ŃN���A
	commandList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);

	commandList->Close(); // �R�}���h���X�g�����

	ID3D12CommandList* commandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists); // �R�}���h���X�g�����s
	commandAllocator->Reset(); // �R�}���h�A���P�[�^�����Z�b�g
	commandList->Reset(commandAllocator, nullptr); // �R�}���h���X�g�����Z�b�g

	swapChain->Present(1, 0); // �X���b�v�`�F�C�����v���[���g�i��ʂɕ\���j
}

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0); // OS�ɏI����ʒm
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

// @brief �E�B���h�E�v���V�[�W��
#ifdef _DEBUG
int main()
{
#else
int WINAPI WINAPI WinMain(HINSTANCE hInstance, LPSTR lpCmdLine)
{

#endif
	// �E�B���h�E�N���X�̓o�^
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProcedure;
	wc.lpszClassName = L"DirectX12_Samples";
	wc.hInstance = GetModuleHandle(nullptr);
	RegisterClassEx(&wc); // �E�B���h�E�N���X�̓o�^

	RECT wrct = { 0, 0, 800, 600 }; // �E�B���h�E�̃T�C�Y

	AdjustWindowRect(&wrct, WS_OVERLAPPEDWINDOW, FALSE); // �E�B���h�E�̃T�C�Y�𒲐�

	// �E�B���h�E�̍쐬
	HWND hwnd = CreateWindow(
		wc.lpszClassName,
		L"DirectX12 Samples",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		wrct.right - wrct.left,
		wrct.bottom - wrct.top,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr
	);

	ShowWindow(hwnd, SW_SHOW); // �E�B���h�E��\��
	
	InitializeDirectX(hwnd); // DirectX�̏�����

	MSG msg = {};
	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (msg.message == WM_QUIT) break; // WM_QUIT���b�Z�[�W��������I��
		}

		UpdateDirectX(hwnd); // DirectX�̍X�V����
	}

	UnregisterClassW(wc.lpszClassName, wc.hInstance); // �E�B���h�E�N���X�̓o�^����

	debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
	debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_SUMMARY | D3D12_RLDO_IGNORE_INTERNAL);
	
	return 0;
}