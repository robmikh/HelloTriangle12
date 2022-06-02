#include "pch.h"
#include "MainWindow.h"
#include "Renderer.h"

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

    // Init our renderer
    auto renderer = Renderer(800, 600);
    
    // Attach our swap chain to the visual tree
    auto swapChain = renderer.SwapChain();
    auto surface = util::CreateCompositionSurfaceForSwapChain(compositor, swapChain.get());
    auto visual = compositor.CreateSpriteVisual();
    visual.RelativeSizeAdjustment({ 1.0f, 1.0f });
    visual.Brush(compositor.CreateSurfaceBrush(surface));
    root.Children().InsertAtTop(visual);

    // Render
    renderer.Render();

    // Message pump
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return util::ShutdownDispatcherQueueControllerAndWait(controller, static_cast<int>(msg.wParam));
}
