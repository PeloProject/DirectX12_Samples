#include "pch.h"
#include "PolygonTest.h"
#include "DirectXDevice.h"
#include <array>
#include <filesystem>

namespace
{
std::filesystem::path GetModuleDirectory()
{
	wchar_t modulePath[MAX_PATH] = {};
	HMODULE hModule = GetModuleHandleW(L"ApplicationDLL.dll");
	if (hModule != nullptr && GetModuleFileNameW(hModule, modulePath, MAX_PATH) > 0)
	{
		return std::filesystem::path(modulePath).parent_path();
	}

	wchar_t currentDir[MAX_PATH] = {};
	GetCurrentDirectoryW(MAX_PATH, currentDir);
	return std::filesystem::path(currentDir);
}

std::filesystem::path ResolveShaderPath(const wchar_t* shaderFileName)
{
	const std::filesystem::path moduleDir = GetModuleDirectory();
	const std::array<std::filesystem::path, 4> candidates = {
		moduleDir / L".." / L".." / L".." / L".." / L"ApplicationDLL" / L"Shader" / shaderFileName,
		moduleDir / L"ApplicationDLL" / L"Shader" / shaderFileName,
		moduleDir / L"Shader" / shaderFileName,
		std::filesystem::path(L"Shader") / shaderFileName
	};

	for (const auto& path : candidates)
	{
		std::error_code ec;
		if (std::filesystem::exists(path, ec))
		{
			return std::filesystem::weakly_canonical(path, ec);
		}
	}

	return candidates.back();
}
}

//=========================================================================================
// 
// @brief コンストラクタ
// 
//=========================================================================================
PolygonTest::PolygonTest()
{
	m_Vertices[0] = { { -0.4f, -0.7f, 0.0f }, { 0.0f, 1.0f } };
	m_Vertices[1] = { { -0.4f,  0.7f, 0.0f }, { 0.0f, 0.0f } };
	m_Vertices[2] = { {  0.4f, -0.7f, 0.0f }, { 1.0f, 1.0f } };
	m_Vertices[3] = { {  0.4f,  0.7f, 0.0f }, { 1.0f, 0.0f } };

	m_TextureData.resize(256 * 256);
	for (auto& tex : m_TextureData)
	{
		tex.R = rand() % 256;
		tex.G = rand() % 256;
		tex.B = rand() % 256;
		tex.A = 255;
	}

	if (CreateGpuResources() != S_OK)
	{ 
		throw std::runtime_error("CreateGpuResources is failed.");
	}
}

//=========================================================================================
// 
// @brief シェーダーのコンパイル
// 
//=========================================================================================
HRESULT PolygonTest::CompileShaders()
{
	const std::filesystem::path vertexShaderPath = ResolveShaderPath(L"BasicVertexShader.hlsl");
	const std::filesystem::path pixelShaderPath = ResolveShaderPath(L"BasicPixelShader.hlsl");

	ID3DBlob* errorBlob = nullptr;
	HRESULT hr = D3DCompileFromFile(
		vertexShaderPath.c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVS",
		"vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&m_pVertexShaderBlob,
		&errorBlob
	);

	if (!SUCCEEDED(hr)) {
		if (errorBlob) {
			LOG_DEBUG("Vertex Shader Compile Error: %s", (char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}
		LOG_DEBUG("Vertex Shader Path: %ls", vertexShaderPath.c_str());
		return E_FAIL;
	}

	hr = D3DCompileFromFile(
		pixelShaderPath.c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicPS",
		"ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&m_pPixelShaderBlob,
		&errorBlob
	);

	if (!SUCCEEDED(hr)) {
		if (errorBlob) {
			LOG_DEBUG("Pixcel Shader Compile Error: %s", (char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}
		LOG_DEBUG("Pixel Shader Path: %ls", pixelShaderPath.c_str());
		return E_FAIL;
	}

	return S_OK;
}

/// <summary>
/// 頂点バッファ用のコミット済みGPUリソースを作成し、クラス内の頂点データを転送して頂点バッファビューを設定します。
/// </summary>
/// <param name="heapProps">D3D12_HEAP_PROPERTIES 構造体。頂点バッファのためのヒープ割り当てのプロパティ（メモリ種類や作成方法など）を指定します。</param>
/// <param name="resourceDesc">D3D12_RESOURCE_DESC 構造体。作成するリソースのサイズ、フォーマット、使用方法などの記述を指定します。</param>
void PolygonTest::CreateVertexBuffer(const D3D12_HEAP_PROPERTIES& heapProps, const D3D12_RESOURCE_DESC& resourceDesc)
{
	// Gpuメモリの確保
	HRESULT hr = DirectXDevice::GetDevice()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_pVertexBuffer)
	);
	if (!SUCCEEDED(hr)) {
		return;
	}

	// 頂点データの転送
	Vertex* vertexMap = nullptr;
	hr = m_pVertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&vertexMap));
	if (SUCCEEDED(hr)) {
		memcpy(vertexMap, m_Vertices, sizeof(m_Vertices));
		D3D12_RANGE writtenRange = { 0, sizeof(m_Vertices) }; // 書き込み範囲
		m_pVertexBuffer->Unmap(0, &writtenRange);
	}

	// 頂点バッファビューの設定
	m_VertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
	m_VertexBufferView.SizeInBytes = sizeof(m_Vertices);		// 全バイト数
	m_VertexBufferView.StrideInBytes = sizeof(m_Vertices[0]);	// 1頂点あたりのサイズ
}

/// <summary>
/// indexバッファ用のコミット済みGPUリソースを作成し、クラス内のインデックスデータを転送してインデックスバッファビューを設定します。
/// </summary>
/// <param name="heapProps"></param>
/// <param name="resourceDesc"></param>
void PolygonTest::CreateIndexBuffer(const D3D12_HEAP_PROPERTIES& heapProps, const D3D12_RESOURCE_DESC& resourceDesc)
{
	HRESULT hr = DirectXDevice::GetDeviceComPtr()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_pIndexBuffer)
	);

	if (!SUCCEEDED(hr)) {
		LOG_DEBUG("Index Buffer Create Error");
		return;
	}



	// インデックスバッファの作成
	unsigned short indices[] = {
	0, 1, 2,
	2, 1, 3
	//3, 2, 1
	};

	m_Indices = vector<short>(std::begin(indices), std::end(indices));

	// インデックスデータの転送
	unsigned short* indexMap = nullptr;
	m_pIndexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&indexMap));
	if (!SUCCEEDED(hr)) {
		return;
	}

	std::copy(std::begin(m_Indices), std::end(m_Indices), indexMap);
	m_pIndexBuffer->Unmap(0, nullptr);

	// インデックスバッファビューの設定
	m_IndexBufferView.BufferLocation = m_pIndexBuffer->GetGPUVirtualAddress();
	m_IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
	m_IndexBufferView.SizeInBytes = static_cast<UINT>(sizeof(m_Indices[0])* m_Indices.size());
}

HRESULT PolygonTest::CreateGpuResources()
{
	// ヒーププロパティの設定
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD; // アップロード可能なヒープタイプ
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = sizeof(m_Vertices);
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	CreateVertexBuffer(heapProps, resourceDesc);
	CreateIndexBuffer(heapProps, resourceDesc);

	// シェーダーのコンパイル
	HRESULT hr = CompileShaders();
	if (FAILED(hr)) {
		return hr;
	}

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* errorBlob = nullptr;
	ID3DBlob* rootSignatureBlob = nullptr;
	hr = D3D12SerializeRootSignature(
		&rootSignatureDesc, // ルートシグネチャの設定
		D3D_ROOT_SIGNATURE_VERSION_1, // ルートシグネチャのバージョン
		&rootSignatureBlob,	// シリアライズされたルートシグネチャの格納先
		&errorBlob				// エラーメッセージの格納先
	);
	if (!SUCCEEDED(hr)) {
		if (errorBlob) {
			LOG_DEBUG("Root Signature Serialize Error: %s", (char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}
		return hr;
	}

	// ルートシグネチャの生成
	hr = DirectXDevice::GetDevice()->CreateRootSignature(
		0,
		rootSignatureBlob->GetBufferPointer(),
		rootSignatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&m_pRootSignature)
	);
	rootSignatureBlob->Release();
	if (!SUCCEEDED(hr)) {
		return hr;
	}

	CreateTextureBuffer();

	// グラフィックスパイプラインステートの設定
	CreateGraphicsPipelineState();

	return hr;
}


/// <summary>
/// グラフィックスパイプラインステートを作成します。
/// </summary>
void PolygonTest::CreateGraphicsPipelineState()
{
	D3D12_INPUT_ELEMENT_DESC inputLayoutDesc[] = {
		{ // 座標乗法
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		{ //UV座標
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};


	// グラフィックスパイプラインステートの設定
	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipelineDesc = {};
	// パイプラインステートにルートシグネチャを設定
	gpipelineDesc.pRootSignature = m_pRootSignature.Get();

	gpipelineDesc.VS.BytecodeLength = m_pVertexShaderBlob->GetBufferSize();
	gpipelineDesc.VS.pShaderBytecode = m_pVertexShaderBlob->GetBufferPointer();
	gpipelineDesc.PS.BytecodeLength = m_pPixelShaderBlob->GetBufferSize();
	gpipelineDesc.PS.pShaderBytecode = m_pPixelShaderBlob->GetBufferPointer();
	gpipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gpipelineDesc.RasterizerState.MultisampleEnable = false;
	gpipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;	// 背面カリングしない
	gpipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gpipelineDesc.RasterizerState.DepthClipEnable = true;			// 深度クリップを有効にする


	// ブレンドステートの設定
	gpipelineDesc.BlendState.AlphaToCoverageEnable = false;		// マルチサンプル時のアルファトゥカバレッジを無効にする
	gpipelineDesc.BlendState.IndependentBlendEnable = false;	// 独立ブレンドを無効にする

	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
	renderTargetBlendDesc.BlendEnable = false;
	renderTargetBlendDesc.LogicOpEnable = false;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	gpipelineDesc.BlendState.RenderTarget[0] = renderTargetBlendDesc;

	// 入力レイアウトの設定
	gpipelineDesc.InputLayout.pInputElementDescs = inputLayoutDesc;		// 入力レイアウトの先頭アドレス
	gpipelineDesc.InputLayout.NumElements = _countof(inputLayoutDesc);	// 入力レイアウトの数

	// トラアングルストリップの場合に頂点を切り離す場合の設定
	gpipelineDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

	gpipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // プリミティブトポロジータイプを三角形に設定

	gpipelineDesc.NumRenderTargets = 1; // レンダーターゲットの数
	gpipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // レンダーターゲットのフォーマットを設定
	gpipelineDesc.SampleDesc.Count = 1;		// マルチサンプリングの設定
	gpipelineDesc.SampleDesc.Quality = 0;	// マルチサンプリングの品質レベル（最低）

	HRESULT hr = DirectXDevice::GetDevice()->CreateGraphicsPipelineState(&gpipelineDesc, IID_PPV_ARGS(&m_pPipelineState));
	if (!SUCCEEDED(hr)) {
		return;
	}
}

/// <summary>
/// Texture用のコミット済みGPUリソースを作成します。
/// </summary>
/// <returns></returns>
HRESULT PolygonTest::CreateTextureBuffer()
{
	// ヒーププロパティの設定
	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	heapProps.CreationNodeMask = 0;
	heapProps.VisibleNodeMask = 0;

	// テクスチャリソースの設定
	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	resourceDesc.Width = 256;
	resourceDesc.Height = 256;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.MipLevels = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	// テクスチャ用のGPUリソースを作成
	HRESULT hr = DirectXDevice::GetDevice()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&m_pTextureBuffer)
	);

	if (!SUCCEEDED(hr)) {
		LOG_DEBUG("Texture Buffer Create Error");
		return hr;
	}

	// テクスチャデータの転送
	hr = m_pTextureBuffer->WriteToSubresource(
		0,
		nullptr,
		m_TextureData.data(),
		256 * sizeof(TextureRGBA),
		256 * 256 * sizeof(TextureRGBA)
	);
	if (!SUCCEEDED(hr)) {
		LOG_DEBUG("Texture Data Write Error");
		return hr;
	}

	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	// シェーダーリソースビューの設定
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // シェーダーから見えるようにする
	descriptorHeapDesc.NodeMask = 0;
	descriptorHeapDesc.NumDescriptors = 1; // ディスクリプタの数
	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; // シェーダーリソースビュー用のヒープタイプ

	hr = DirectXDevice::GetDevice()->CreateDescriptorHeap(
		&descriptorHeapDesc,
		IID_PPV_ARGS(&descriptorHeap)
	);

	return hr;
}

void PolygonTest::ApplyTransformations()
{
	DirectX::XMFLOAT3* vertexMap = nullptr;

}

void PolygonTest::Render()
{
	ID3D12GraphicsCommandList* commandList = DirectXDevice::GetCommandList();
	if (commandList == nullptr)
	{
		return;
	}

	D3D12_VIEWPORT viewport = {};
	viewport.Width = static_cast<FLOAT>(Application::GetWindowWidth());
	viewport.Height = static_cast<FLOAT>(Application::GetWindowHeight());
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	ComPtr<ID3D12GraphicsCommandList> m_pCommandList = commandList;
	m_pCommandList->SetPipelineState(m_pPipelineState.Get());
	m_pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
	m_pCommandList->RSSetViewports(1, &viewport);
	D3D12_RECT scissorRect = { 0, 0, static_cast<LONG>(viewport.Width), static_cast<LONG>(viewport.Height) };
	m_pCommandList->RSSetScissorRects(1, &scissorRect);
	m_pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	m_pCommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
	m_pCommandList->IASetIndexBuffer(&m_IndexBufferView);
	//m_pCommandList.Get()->DrawInstanced(6, 1, 0, 0);
	m_pCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}
