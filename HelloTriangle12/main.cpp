#include "pch.h"
#include "MainWindow.h"
#include "d3d12Helpers.h"

namespace winrt
{
    using namespace Windows::Foundation;
    using namespace Windows::Foundation::Numerics;
    using namespace Windows::UI;
    using namespace Windows::UI::Composition;
}

namespace util
{
    using namespace robmikh::common::desktop;
    using namespace robmikh::common::uwp;
}

int __stdcall WinMain(HINSTANCE, HINSTANCE, PSTR, int)
{
    // Initialize COM
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    // Create the DispatcherQueue that the compositor needs to run
    auto controller = util::CreateDispatcherQueueControllerForCurrentThread();

    // Create our window and visual tree
    auto window = MainWindow(L"HelloTriangle12", 800, 600);
    auto compositor = winrt::Compositor();
    auto target = window.CreateWindowTarget(compositor);
    auto root = compositor.CreateSpriteVisual();
    root.RelativeSizeAdjustment({ 1.0f, 1.0f });
    root.Brush(compositor.CreateColorBrush(winrt::Colors::White()));
    target.Root(root);

    // Init D3D12
    auto dxgiFactory = util::CreateDXGIFactory();
    auto d3d12Device = util::CreateD3D12Device(dxgiFactory);
    auto d3d12Queue = util::CreateD3D12CommandQueue(d3d12Device);

    // Create our swap chain
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = 800;
    desc.Height = 600;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    winrt::com_ptr<IDXGISwapChain1> swapChain;
    winrt::check_hresult(dxgiFactory->CreateSwapChainForComposition(d3d12Queue.get(), &desc, nullptr, swapChain.put()));
    auto surface = util::CreateCompositionSurfaceForSwapChain(compositor, swapChain.get());
    auto visual = compositor.CreateSpriteVisual();
    visual.RelativeSizeAdjustment({ 1.0f, 1.0f });
    visual.Brush(compositor.CreateSurfaceBrush(surface));
    root.Children().InsertAtTop(visual);

    // Setup our pipeline


    // Message pump
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return util::ShutdownDispatcherQueueControllerAndWait(controller, static_cast<int>(msg.wParam));
}
