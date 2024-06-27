#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Graphics.Display.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Input.h>

#include <d3d11.h>
#include <dxgi1_4.h>

using namespace winrt;

using namespace Windows;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::Foundation::Numerics;
using namespace Windows::Graphics::Display;
using namespace Windows::UI;
using namespace Windows::UI::Core;

struct App : implements<App, IFrameworkViewSource, IFrameworkView>
{
    bool m_windowClosed = false;
    bool m_windowVisible = true;

    com_ptr<ID3D11Device> m_pd3dDevice;
    com_ptr<ID3D11DeviceContext> m_pd3dDeviceContext;
    com_ptr<IDXGISwapChain1> m_pSwapChain;
    com_ptr<ID3D11RenderTargetView> m_mainRenderTargetView;

    UINT m_ResizeWidth = 0, m_ResizeHeight = 0;
    Windows::Foundation::Size m_OldSize;

    IFrameworkView CreateView()
    {
        return *this;
    }

    void Initialize(CoreApplicationView const &)
    {
    }

    void Load(hstring const&)
    {
    }

    void Uninitialize()
    {
    }

    void Run()
    {
        CoreWindow window = CoreWindow::GetForCurrentThread();
        CoreDispatcher dispatcher = window.Dispatcher();
        window.Activate();

      

        // High DPI scaling
        DisplayInformation currentDisplayInformation = DisplayInformation::GetForCurrentView();
        float dpi = currentDisplayInformation.LogicalDpi() / 96.0f;
       
      
        while (!m_windowClosed)
        {
            if (m_windowVisible)
            {
                dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

                // Handle window resize (we don't resize directly in the SizeChanged handler)
                if (m_ResizeWidth != 0 && m_ResizeHeight != 0)
                {
                    CleanupRenderTarget();
                    HRESULT hr = m_pSwapChain->ResizeBuffers(2, lround(m_ResizeWidth), lround(m_ResizeHeight), DXGI_FORMAT_UNKNOWN, 0);
                    m_ResizeWidth = m_ResizeHeight = 0;
                    CreateRenderTarget();
                }
                
                auto renderTargetView = m_mainRenderTargetView.get();
                const float clear_color_with_alpha[4] = { 1, 1, 1, 1 };

                m_pd3dDeviceContext->OMSetRenderTargets(1, &renderTargetView, nullptr);
                m_pd3dDeviceContext->ClearRenderTargetView(renderTargetView, clear_color_with_alpha);

                m_pSwapChain->Present(1, 0);
            }
            else
            {
                dispatcher.ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
            }
        }
    }

    void SetWindow(CoreWindow const & window)
    {
        window.Closed({ this, &App::OnWindowClosed });
        window.SizeChanged({ this, &App::OnSizeChanged });
        window.VisibilityChanged({ this, &App::OnVisibiltyChanged });

        D3D_FEATURE_LEVEL featureLevels[] =
        {
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_0,
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
            D3D_FEATURE_LEVEL_9_3,
            D3D_FEATURE_LEVEL_9_2,
            D3D_FEATURE_LEVEL_9_1
        };

        D3D_FEATURE_LEVEL featureLevel;

        HRESULT hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            0,
            0,
            featureLevels,
            ARRAYSIZE(featureLevels),
            D3D11_SDK_VERSION,
            m_pd3dDevice.put(),
            &featureLevel,
            m_pd3dDeviceContext.put()
        ); 

        if (FAILED(hr))
        {
            winrt::check_hresult(
                D3D11CreateDevice(
                    nullptr,
                    D3D_DRIVER_TYPE_WARP,
                    0,
                    0,
                    featureLevels,
                    ARRAYSIZE(featureLevels),
                    D3D11_SDK_VERSION,
                    m_pd3dDevice.put(),
                    &featureLevel,
                    m_pd3dDeviceContext.put()
                )
            );
        }

        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };

        swapChainDesc.Width = 0;
        swapChainDesc.Height = 0;
        swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.Stereo = false;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = 2;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

        // This sequence obtains the DXGI factory that was used to create the Direct3D device above.
        com_ptr<IDXGIDevice3> dxgiDevice;
        m_pd3dDevice.as(dxgiDevice);

        com_ptr<IDXGIAdapter> dxgiAdapter;
        winrt::check_hresult(
            dxgiDevice->GetAdapter(dxgiAdapter.put())
        );

        com_ptr<IDXGIFactory4> dxgiFactory;
        winrt::check_hresult(
            dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory))
        );

        winrt::check_hresult(
            dxgiFactory->CreateSwapChainForCoreWindow(
                m_pd3dDevice.get(),
                winrt::get_unknown(window),
                &swapChainDesc,
                nullptr,
                m_pSwapChain.put()
            )
        );

        winrt::check_hresult(
            dxgiDevice->SetMaximumFrameLatency(1)
        );

        CreateRenderTarget();
    }

    void OnSizeChanged(IInspectable const &, WindowSizeChangedEventArgs const & args)
    {
        auto size = args.Size();

        // Prevent unnecessary resize
        if (size != m_OldSize)
        {
            m_ResizeWidth = size.Width;
            m_ResizeHeight = size.Height;
        }

        m_OldSize = size;
    }

    void OnVisibiltyChanged(IInspectable const&, VisibilityChangedEventArgs const& args)
    {
        m_windowVisible = args.Visible();
    }

    void OnWindowClosed(IInspectable const&, CoreWindowEventArgs const& args)
    {
        m_windowClosed = true;
    }

    void CreateRenderTarget()
    {
        com_ptr<ID3D11Texture2D> pBackBuffer;
        m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(pBackBuffer.put()));
        m_pd3dDevice->CreateRenderTargetView(pBackBuffer.get(), nullptr, m_mainRenderTargetView.put());
    }

    void CleanupRenderTarget()
    {
        m_mainRenderTargetView = nullptr;
    }
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    CoreApplication::Run(make<App>());
}
