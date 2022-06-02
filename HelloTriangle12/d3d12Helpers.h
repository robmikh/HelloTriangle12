#pragma once
#include <Unknwn.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <winrt/base.h>

namespace robmikh::common::uwp
{
    namespace impl
    {
        inline winrt::com_ptr<IDXGIAdapter1> GetHardwareAdapter(winrt::com_ptr<IDXGIFactory1> const& factory)
        {
            auto factory6 = factory.as<IDXGIFactory6>();
            winrt::com_ptr<IDXGIAdapter1> adapter;
            for (
                uint32_t adapterIndex = 0;
                SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                    adapterIndex,
                    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                    winrt::guid_of<IDXGIAdapter1>(),
                    adapter.put_void()));
                ++adapterIndex)
            {
                DXGI_ADAPTER_DESC1 desc = {};
                adapter->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Don't select the Basic Render Driver adapter.
                    continue;
                }

                // Check to see whether the adapter supports Direct3D 12, but don't create the
                // actual device yet.
                if (SUCCEEDED(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
                {
                    break;
                }
            }
            return adapter;
        }
    }

    inline winrt::com_ptr<IDXGIFactory4> CreateDXGIFactory()
    {
        winrt::com_ptr<IDXGIFactory4> dxgiFactory;
        winrt::check_hresult(CreateDXGIFactory1(winrt::guid_of<IDXGIFactory4>(), dxgiFactory.put_void()));
        return dxgiFactory;
    }

    inline winrt::com_ptr<ID3D12Device> CreateD3D12Device(winrt::com_ptr<IDXGIFactory4> const& dxgiFactory)
    {
        auto adapter = impl::GetHardwareAdapter(dxgiFactory);
        if (adapter.get() == nullptr)
        {
            winrt::check_hresult(dxgiFactory->EnumWarpAdapter(winrt::guid_of<IDXGIAdapter1>(), adapter.put_void()));
        }
        winrt::com_ptr<ID3D12Device> d3d12Device;
        winrt::check_hresult(D3D12CreateDevice(
            adapter.get(),
            D3D_FEATURE_LEVEL_11_0,
            winrt::guid_of<ID3D12Device>(),
            d3d12Device.put_void()));
        return d3d12Device;
    }

    inline winrt::com_ptr<ID3D12CommandQueue> CreateD3D12CommandQueue(winrt::com_ptr<ID3D12Device> const& d3d12Device)
    {
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        winrt::com_ptr<ID3D12CommandQueue> d3d12Queue;
        winrt::check_hresult(d3d12Device->CreateCommandQueue(&queueDesc, winrt::guid_of<ID3D12CommandQueue>(), d3d12Queue.put_void()));
        return d3d12Queue;
    }
}
