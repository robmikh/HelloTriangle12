#include "pch.h"
#include "Renderer.h"
#include "d3d12Helpers.h"
#include <d3dcompiler.h>

namespace shaders
{
    namespace pixel
    {
#include "PixelShader.h"
    }
    namespace vertex
    {
#include "VertexShader.h"
    }
}

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Foundation::Numerics;
}

namespace util
{
    using namespace robmikh::common::desktop;
    using namespace robmikh::common::uwp;
}

struct Vertex
{
    winrt::float3 pos;
    winrt::float3 color;
};

winrt::com_ptr<ID3DBlob> CreateBlobFromBytes(uint8_t const* bytes, size_t length)
{
    winrt::com_ptr<ID3DBlob> blob;
    winrt::check_hresult(D3DCreateBlob(length, blob.put()));
    memcpy_s(blob->GetBufferPointer(), blob->GetBufferSize(), bytes, length);
    return blob;
}

Renderer::Renderer(uint32_t width, uint32_t height) :
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height))
{
    m_width = width;
    m_height = height;

    LoadPipeline();
    LoadAssets();


}

void Renderer::Render()
{
    // Record all the commands we need to render the scene into the command list
    PopulateCommandList();

    // Execute the command list
    ID3D12CommandList* ppCommandLists[] = { m_commandList.get() };
    m_d3d12Queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame
    winrt::check_hresult(m_swapChain->Present(1, 0));

    WaitForPreviousFrame();
}

void Renderer::LoadPipeline()
{
    // Init D3D12
    m_dxgiFactory = util::CreateDXGIFactory();
    m_d3d12Device = util::CreateD3D12Device(m_dxgiFactory);
    m_d3d12Queue = util::CreateD3D12CommandQueue(m_d3d12Device);

    // Create our swap chain
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = m_width;
    desc.Height = m_height;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    winrt::com_ptr<IDXGISwapChain1> swapChain1;
    winrt::check_hresult(m_dxgiFactory->CreateSwapChainForComposition(m_d3d12Queue.get(), &desc, nullptr, swapChain1.put()));
    m_swapChain = swapChain1.as<IDXGISwapChain4>();

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps
    {
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = desc.BufferCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        winrt::check_hresult(m_d3d12Device->CreateDescriptorHeap(&rtvHeapDesc, winrt::guid_of<ID3D12DescriptorHeap>(), m_rtvHeap.put_void()));
        m_rtvDescriptorSize = m_d3d12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // Create frame resources
    m_renderTargets = std::vector<winrt::com_ptr<ID3D12Resource>>(desc.BufferCount, nullptr);
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
        // Create a RTV for each frame.
        for (uint32_t n = 0; n < m_renderTargets.size(); n++)
        {
            winrt::check_hresult(m_swapChain->GetBuffer(n, winrt::guid_of<ID3D12Resource>(), m_renderTargets[n].put_void()));
            m_d3d12Device->CreateRenderTargetView(m_renderTargets[n].get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, m_rtvDescriptorSize);
        }
    }

    winrt::check_hresult(m_d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, winrt::guid_of<ID3D12CommandAllocator>(), m_commandAllocator.put_void()));
}

void Renderer::LoadAssets()
{
    // Create an empty root signature
    {
        CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
        rootSignatureDesc.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        winrt::com_ptr<ID3DBlob> signature;
        winrt::com_ptr<ID3DBlob> error;
        winrt::check_hresult(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, signature.put(), error.put()));
        winrt::check_hresult(m_d3d12Device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), winrt::guid_of<ID3D12RootSignature>(), m_rootSignature.put_void()));
    }

    // Create the pipeline state, which includes compiling and loading shaders
    {
        auto vertexShader = CreateBlobFromBytes(shaders::vertex::g_main, ARRAYSIZE(shaders::vertex::g_main));
        auto pixelShader = CreateBlobFromBytes(shaders::pixel::g_main, ARRAYSIZE(shaders::pixel::g_main));

        // Define the vertex input layout
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        // Describe and create the graphics pipeline state object (PSO)
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs.data(), static_cast<uint32_t>(inputElementDescs.size()) };
        psoDesc.pRootSignature = m_rootSignature.get();
        psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.get());
        psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.get());
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;
        winrt::check_hresult(m_d3d12Device->CreateGraphicsPipelineState(&psoDesc, winrt::guid_of<ID3D12PipelineState>(), m_pipelineState.put_void()));
    }

    // Create the command list
    winrt::check_hresult(m_d3d12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.get(), m_pipelineState.get(), winrt::guid_of<ID3D12CommandList>(), m_commandList.put_void()));

    // Command lists are created in the recording state, but there is nothing
    // to record yet. The main loop expects it to be closed, so close it now.
    winrt::check_hresult(m_commandList->Close());

    // Create the vertex buffer
    {
        std::vector<Vertex> vertices =
        {
            { { 0.0f,  0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
            { { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
            { {-0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
        };

        auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(Vertex) * vertices.size());
        winrt::check_hresult(m_d3d12Device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            winrt::guid_of<ID3D12Resource>(),
            m_vertexBuffer.put_void()));

        // Copy the triangle data to the vertex buffer
        uint8_t* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
        winrt::check_hresult(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, vertices.data(), sizeof(Vertex) * vertices.size());
        m_vertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = sizeof(Vertex) * vertices.size();
    }

    // Create synchronization objects and wait until assets have been uploaded to the GPU
    m_fenceEvent = wil::unique_event(wil::EventOptions::None);
    {
        winrt::check_hresult(m_d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, winrt::guid_of<ID3D12Fence>(), m_fence.put_void()));
        m_fenceValue = 1;

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitForPreviousFrame();
    }
}

void Renderer::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    winrt::check_hresult(m_commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    winrt::check_hresult(m_commandList->Reset(m_commandAllocator.get(), m_pipelineState.get()));

    // Set necessary state.
    m_commandList->SetGraphicsRootSignature(m_rootSignature.get());
    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Indicate that the back buffer will be used as a render target.
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        m_commandList->ResourceBarrier(1, &barrier);
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Record commands.
    const float clearColor[] = { 0.39f, 0.58f, 0.93f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->DrawInstanced(3, 1, 0, 0);

    // Indicate that the back buffer will now be used to present.
    {
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        m_commandList->ResourceBarrier(1, &barrier);
    }

    winrt::check_hresult(m_commandList->Close());
}

void Renderer::WaitForPreviousFrame()
{
    // Signal and increment the fence value.
    const uint64_t fence = m_fenceValue;
    winrt::check_hresult(m_d3d12Queue->Signal(m_fence.get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        winrt::check_hresult(m_fence->SetEventOnCompletion(fence, m_fenceEvent.get()));
        m_fenceEvent.wait();
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}
