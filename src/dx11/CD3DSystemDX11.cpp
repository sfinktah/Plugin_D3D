/* D3D_Plugin - for licensing and copyright see license.txt */

#include <StdAfx.h>
#include "CD3DSystemDX11.h"
#include "d3d11hook.h"
#include "../dxgi/dxgihook.h"
#include "../dxgi/dxgiutils.hpp"
#include <TwSimpleDX11.h>
#include <CPluginD3D.h>

//#pragma comment(lib, "dxerr.lib") // not needed atm

D3DPlugin::CD3DSystem11* D3DPlugin::gD3DSystem11 = NULL;

#undef METHOD
#undef INTERFACE

#define INTERFACE IDXGISwapChain
#define METHOD Present

GEN_HOOK( UINT SyncInterval, UINT Flags )
{
	// Have DEVICE, need SWAPCHAIN
    if ( !D3DPlugin::gD3DSystem11->m_pSwapChain && D3DPlugin::gD3DSystem11->m_pDevice )
    { 
		ID3D11Device* pSwapChainDevice = NULL; 
		HRESULT hrg = This->GetDevice( __uuidof( ID3D11Device ), ( void** )&pSwapChainDevice ); 
        if ( pSwapChainDevice == D3DPlugin::gD3DSystem11->m_pDevice )
        {
            D3DPlugin::gD3DSystem11->m_pSwapChain = This;
            D3DPlugin::gPlugin->LogAlways( "DXGI swap chain retrieved" );
        }
    }

    bool bEngineSwapChain = D3DPlugin::gD3DSystem11->m_pSwapChain == This;

    if ( bEngineSwapChain )
    {
        D3DPlugin::gD3DSystem11->OnPrePresent();
    }

    CALL_ORGINAL( SyncInterval, Flags );

    if ( bEngineSwapChain )
    {
        D3DPlugin::gD3DSystem11->OnPostPresent();
        D3DPlugin::gD3DSystem11->OnPostBeginScene(); // doesn't exist in dx11, but keep for compatibility
    }

    //rehookVT( This, IDXGISwapChain, Present );

    return hr;
}
#undef METHOD
#undef INTERFACE

bool GetD3D11DeviceData( INT_PTR* unkdata, int nDatalen, void* pParam )
{
    bool bRet = false;
    HWND hWndDummy = CreateWindowEx( NULL, TEXT( "Message" ), TEXT( "DummyWindow" ), WS_MINIMIZE, 0, 0, 8, 8, HWND_MESSAGE, NULL, 0, NULL );

    HMODULE hModule = NULL;
    hModule = GetModuleHandleA( "d3d11.dll" );

    typedef HRESULT( WINAPI * fD3D11CreateDeviceAndSwapChain )(
        _In_   IDXGIAdapter * pAdapter,
        _In_   D3D_DRIVER_TYPE DriverType,
        _In_   HMODULE Software,
        _In_   UINT Flags,
        _In_   const D3D_FEATURE_LEVEL * pFeatureLevels,
        _In_   UINT FeatureLevels,
        _In_   UINT SDKVersion,
        _In_   const DXGI_SWAP_CHAIN_DESC * pSwapChainDesc,
        _Out_  IDXGISwapChain** ppSwapChain,
        _Out_  ID3D11Device** ppDevice,
        _Out_  D3D_FEATURE_LEVEL * pFeatureLevel,
        _Out_  ID3D11DeviceContext** ppImmediateContext );

    fD3D11CreateDeviceAndSwapChain D3D11CreateDeviceAndSwapChain = ( fD3D11CreateDeviceAndSwapChain )GetProcAddress( hModule, "D3D11CreateDeviceAndSwapChain" );

    IDXGISwapChain* pSwapChain = NULL;
    ID3D11Device* pDevice = NULL;
    ID3D11DeviceContext* pDeviceContext = NULL;

	// sfink: why is it a dummy?
    // dummy swap chain description struct
    DXGI_SWAP_CHAIN_DESC sSwapChainDesc;
    ZeroMemory( &sSwapChainDesc, sizeof( DXGI_SWAP_CHAIN_DESC ) );
    sSwapChainDesc.Windowed = TRUE;
    sSwapChainDesc.OutputWindow = hWndDummy;
    sSwapChainDesc.BufferCount = 1;
    sSwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sSwapChainDesc.SampleDesc.Count = 1;

    // create a device, device context and swap chain using the information in the scd struct
	
	HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL,                     /* _In_opt_   IDXGIAdapter         *pAdapter,               */ 
        D3D_DRIVER_TYPE_HARDWARE, /*            D3D_DRIVER_TYPE      DriverType,              */ 
        NULL,                     /*            HMODULE              Software,                */ 
        NULL,                     /*            UINT                 Flags,                   */ 
        NULL,                     /* _In_opt_   D3D_FEATURE_LEVEL    *pFeatureLevels,         */ 
        NULL,                     /*            UINT                 FeatureLevels,           */ 
        D3D11_SDK_VERSION,        /*            UINT                 SDKVersion,              */ 
        &sSwapChainDesc,          /* _In_opt_   DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,         */ 
        &pSwapChain,              /* _Out_opt_  IDXGISwapChain       **ppSwapChain,           */ 
        &pDevice,                 /* _Out_opt_  ID3D11Device         **ppDevice,              */ 
        NULL,                     /* _Out_opt_  D3D_FEATURE_LEVEL    *pFeatureLevel,          */ 
        &pDeviceContext );        /* _Out_opt_  ID3D11DeviceContext  **ppImmediateContex      */ 

    if ( SUCCEEDED( hr ) )
    {
        bRet = true;

        if ( pParam && *( ( bool* )pParam ) )
        {
            hookVT( pSwapChain, IDXGISwapChain, Present );
        }
    }

    if ( bRet && unkdata )
    {
        void** vt = getVT( pDevice );

        if ( vt )
        {
            memcpy( unkdata, vt, nDatalen );
        }

        else
        {
            bRet = false;
        }
    }

    SAFE_RELEASE( pSwapChain );
    SAFE_RELEASE( pDevice );
    SAFE_RELEASE( pDeviceContext );

    DestroyWindow( hWndDummy );

    return bRet;
}

bool BootstrapD3D11SwapChainHook()
{
    bool bHook = true;
    return GetD3D11DeviceData( NULL, 0, &bHook );
}

namespace D3DPlugin
{
    CD3DSystem11::CD3DSystem11()
    {
        gD3DSystem11 = this;

        m_bD3DHookInstalled = false;
        m_nTextureMode = HTM_NONE;
        m_pTempTex = NULL;
        m_nFeatureLevel = 0;
        m_sGPUName = "";

		// Never tried disabling the internal hooking mechanism, but it might be the safer way
		// given the problems we had co-existing with ABT
#if !defined(D3D_DISABLE_HOOK)
        m_pDeviceCtx = NULL;
        m_pSwapChain = NULL;
#endif

        void* pTrialDevice = NULL;
#if CDK_VERSION < 350
//        pTrialDevice = gEnv->pRenderer->EF_Query( EFQ_D3DDevice );
//uncomment when it comes back
//#elif CDK_VERSION > ?
// gEnv->pRenderer->EF_Query( EFQ_D3DDevice, pTrialDevice );
#endif

		gPlugin->LogAlways( "(Disabled) Looking for m_pDevice" );
		// sfink
        // m_pDevice = FindD3D11Device( NULL, /* ( INT_PTR )gEnv->pRenderer, */ pTrialDevice );
		// Do we define g_D3DDev anywhere, why not use mP-pDevice and setdevice
		if (m_pDevice = g_D3DDev)
			gPlugin->LogAlways( "%s:%d Found m_pDevice: %p (Got it from Authority)", __FILE__, __LINE__, m_pDevice );

        // Hook Swap Chain
        if ( m_pDevice )
        {
            if ( BootstrapD3D11SwapChainHook() )
            {
                gPlugin->LogAlways( "DXGI swap chain hook set" );
            }

            else
            {
                gPlugin->LogAlways( "DXGI swap chain hook couldn't be installed" );
            }
        }

        if ( m_pDevice )
        {
            ID3D11Device* pDevice = ( ID3D11Device* )m_pDevice;
            pDevice->GetImmediateContext( ( ID3D11DeviceContext** )&m_pDeviceCtx );

            if ( m_pDeviceCtx )
            {
                ( ( ID3D11DeviceContext* )( m_pDeviceCtx ) )->Release();
                gPlugin->LogAlways( "DX11 device context found" );
            }

            else
            {
                gPlugin->LogWarning( "DX11 device context not found" );
            }

            m_nFeatureLevel = pDevice->GetFeatureLevel();
            m_sGPUName = getGPUName( pDevice );

            gPlugin->LogAlways( "DX11 device found: FeatureLevel(0x%05x) GPU(%s)", int( m_nFeatureLevel ), m_sGPUName.c_str() );
        }

        else
        {
            gPlugin->LogWarning( "DX11 device not found" );
        }
    }

    CD3DSystem11::~CD3DSystem11()
    {
        hookD3D( false );

        gD3DSystem11 = NULL;
    }

    void CD3DSystem11::hookD3D( bool bHook )
    {
#if defined(D3D_DISABLE_HOOK)
        return;
#endif

        if ( m_pDevice && m_pDeviceCtx )
        {
            if ( bHook && m_bD3DHookInstalled )
            {
                if ( m_pSwapChain )
                {
                    // rehookVT( m_pSwapChain, IDXGISwapChain, Present );
                }

            }

            else if ( bHook )
            {
            }

            else if ( m_bD3DHookInstalled )
            {
                // For Unloading
                if ( m_pSwapChain )
                {
                    unhookVT( m_pSwapChain, IDXGISwapChain, Present, false );
                }

            }

            m_bD3DHookInstalled = bHook;
        }
    }

    int CD3DSystem11::GetFeatureLevel()
    {
        return m_pDevice ? m_nFeatureLevel : 0;
    }

    const char* CD3DSystem11::GetGPUName()
    {
        return m_sGPUName.c_str();
    }

}
