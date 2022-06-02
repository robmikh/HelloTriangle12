#pragma once

class Renderer
{
public:
	Renderer(uint32_t width, uint32_t height);

	void Render();

	winrt::com_ptr<IDXGISwapChain4> const& SwapChain() { return m_swapChain; }

private:
	void LoadPipeline();
	void LoadAssets();
	void PopulateCommandList();
	void WaitForPreviousFrame();
	
private:
	winrt::com_ptr<IDXGIFactory4> m_dxgiFactory;
	winrt::com_ptr<ID3D12Device> m_d3d12Device;
	winrt::com_ptr<ID3D12CommandQueue> m_d3d12Queue;
	winrt::com_ptr<IDXGISwapChain4> m_swapChain;

	winrt::com_ptr<ID3D12DescriptorHeap> m_rtvHeap;
	uint32_t m_rtvDescriptorSize = 0;
	std::vector<winrt::com_ptr<ID3D12Resource>> m_renderTargets;
	winrt::com_ptr<ID3D12CommandAllocator> m_commandAllocator;
	winrt::com_ptr<ID3D12RootSignature> m_rootSignature;
	winrt::com_ptr<ID3D12PipelineState> m_pipelineState;
	winrt::com_ptr<ID3D12GraphicsCommandList> m_commandList;

	winrt::com_ptr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView = {};

	uint32_t m_frameIndex = 0;
	winrt::com_ptr<ID3D12Fence> m_fence;
	uint64_t m_fenceValue = 0;
	wil::unique_event m_fenceEvent{ nullptr };

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;

	uint32_t m_width = 0;
	uint32_t m_height = 0;
};
