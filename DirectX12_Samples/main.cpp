#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <vector>
#ifdef _DEBUG
#include <iostream>
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace std;

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
// @brief DirectX�̏���������
bool InitializeDirectX(HWND hwnd)
{
	DebugOutputFormatString("DirectX�̏��������J�n\n");
	ID3D12Device* device = nullptr;
	IDXGIFactory6* dxgiFactory = nullptr;
	IDXGISwapChain4* swapChain = nullptr;

	// DXGI�t�@�N�g���̍쐬
	if (CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)) != S_OK)
	{
		DebugOutputFormatString("DXGI�t�@�N�g���̍쐬�Ɏ��s\n");
		return false;
	}
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

	DebugOutputFormatString("DirectX�̏�����������ɏI��\n");

	//-----
	// �R�}���h���X�g
	ID3D12CommandList* commandList = nullptr;
	ID3D12CommandAllocator* commandAllocator = nullptr;

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
	ID3D12CommandQueue* commandQueue = nullptr;
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

	return true;
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
	}

	UnregisterClassW(wc.lpszClassName, wc.hInstance); // �E�B���h�E�N���X�̓o�^����
	
	return 0;
}