/* D3D_Plugin - for licensing and copyright see license.txt */

#include <StdAfx.h>
#include "CD3DSystemDX10.h"
#include "d3d10hook.h"
#include "../dxgi/dxgihook.h"

#include <CPluginD3D.h>

//#pragma comment(lib, "dxerr.lib") // not needed atm

D3DPlugin::CD3DSystem10* D3DPlugin::gD3DSystem10 = NULL;

namespace dxgi_unique1 // need to make it unique since dx11 is using dxgi too
{
#undef METHOD
#undef INTERFACE

#define INTERFACE IDXGISwapChain

#define METHOD Present
    GEN_HOOK( UINT SyncInterval, UINT Flags )
    {
        if ( !D3DPlugin::gD3DSystem10->m_pSwapChain && D3DPlugin::gD3DSystem10->m_pDevice )
        {
            ID3D10Device* pSwapChainDevice = NULL;
            HRESULT hrg = This->GetDevice( __uuidof( ID3D10Device ), ( void** )&pSwapChainDevice );

            if ( pSwapChainDevice == D3DPlugin::gD3DSystem10->m_pDevice )
            {
                D3DPlugin::gD3DSystem10->m_pSwapChain = This;

                D3DPlugin::gPlugin->LogAlways( "DXGI swap chain retrieved" );
            }
        }

        bool bEngineSwapChain = D3DPlugin::gD3DSystem10->m_pSwapChain == This;

        if ( bEngineSwapChain )
        {
            D3DPlugin::gD3DSystem10->OnPrePresent();
        }

        CALL_ORGINAL( SyncInterval, Flags );

        if ( bEngineSwapChain )
        {
            D3DPlugin::gD3DSystem10->OnPostPresent();
            D3DPlugin::gD3DSystem10->OnPostBeginScene(); // doesn't exist in dx10, but keep for compatibility
        }

        rehookVT( This, IDXGISwapChain, Present );

        return hr;
    }
#undef METHOD
}

using namespace dxgi_unique1;

#undef INTERFACE
#define INTERFACE ID3D10Device

#define METHOD CreateTexture2D
GEN_HOOK(  __in  const D3D10_TEXTURE2D_DESC* pDesc, __in_xcount_opt( pDesc->MipLevels * pDesc->ArraySize )  const D3D10_SUBRESOURCE_DATA* pInitialData, __out  ID3D10Texture2D** ppTexture2D )
{
    if ( D3DPlugin::gD3DSystem10->m_nTextureMode == HTM_INJECT )
    {
        goto INJECTTEXTURE;
    }

    CALL_ORGINAL( pDesc, pInitialData, ppTexture2D );

    if ( SUCCEEDED( hr ) )
    {
        switch ( D3DPlugin::gD3DSystem10->m_nTextureMode )
        {
            case HTM_INJECT:
INJECTTEXTURE:
                if ( pDesc && pDesc->ArraySize == 1 && ppTexture2D )
                {
                    ppTexture2D[0] = ( ID3D10Texture2D* )( D3DPlugin::gD3DSystem10->m_pTempTex );
                    ppTexture2D[0]->AddRef();
                }

                hr = S_OK;
                break;

            case HTM_CREATE:
                D3DPlugin::gD3DSystem10->m_pTempTex = NULL;

                if ( pDesc && pDesc->ArraySize == 1 && ppTexture2D )
                {
                    D3DPlugin::gD3DSystem10->m_pTempTex = ( void* )( ppTexture2D[0] );
                }

                break;
        }

        D3DPlugin::gD3DSystem10->m_nTextureMode = HTM_NONE;
    }

    return hr;
}
#undef METHOD
#undef INTERFACE

bool BootstrapD3D10SwapChainHook( )
{
    bool bRet = false;
    HWND hWndDummy = CreateWindowEx( NULL, TEXT( "Message" ), TEXT( "DummyWindow" ), WS_MINIMIZE, 0, 0, 8, 8, HWND_MESSAGE, NULL, 0, NULL );

    HMODULE hModule = NULL;
    hModule = GetModuleHandle( "d3d10.dll" );

    typedef HRESULT ( WINAPI * fD3D10CreateDeviceAndSwapChain )(
        _In_   IDXGIAdapter * pAdapter,
        _In_   D3D10_DRIVER_TYPE DriverType,
        _In_   HMODULE Software,
        _In_   UINT Flags,
        _In_   UINT SDKVersion,
        _In_   DXGI_SWAP_CHAIN_DESC * pSwapChainDesc,
        _Out_  IDXGISwapChain** ppSwapChain,
        _Out_  ID3D10Device** ppDevice
    );

    fD3D10CreateDeviceAndSwapChain D3D10CreateDeviceAndSwapChain = ( fD3D10CreateDeviceAndSwapChain )GetProcAddress( hModule, "D3D10CreateDeviceAndSwapChain" );

    IDXGISwapChain* pSwapChain = NULL;
    ID3D10Device* pDevice = NULL;

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
    HRESULT hr = D3D10CreateDeviceAndSwapChain(
                     NULL,
                     D3D10_DRIVER_TYPE_HARDWARE,
                     NULL,
                     0,
                     D3D10_SDK_VERSION,
                     &sSwapChainDesc,
                     &pSwapChain,
                     &pDevice );

    if ( SUCCEEDED( hr ) )
    {
        bRet = true;
        hookVT( pSwapChain, IDXGISwapChain, Present );
    }

    SAFE_RELEASE( pSwapChain );
    SAFE_RELEASE( pDevice );

    DestroyWindow( hWndDummy );

    return bRet;
}

namespace D3DPlugin
{
    namespace dxgi_unique1 // need to make it unique since dx11 is using dxgi too
    {
        IDXGISwapChain* FindDXGISwapChain( INT_PTR nRelativeBase )
        {
            INT_PTR nModuleOffset = ( INT_PTR )GetModuleHandle( TEXT( "dxgi.dll" ) );

            if ( !nModuleOffset )
            {
                gPlugin->LogWarning( "DX10/DXGI not active" );
                return NULL;
            }

            // atm search is not needed since we receive swap chain through present
            return NULL;
        }
    }
    using namespace dxgi_unique1;

    ID3D10Device* FindD3D10Device( INT_PTR nRelativeBase, void* pTrialDevice )
    {
        INT_PTR nModuleOffset = ( INT_PTR )GetModuleHandle( TEXT( "d3d10.dll" ) );

        if ( !nModuleOffset )
        {
            gPlugin->LogWarning( "DX10 not active" );
            return NULL;
        }

        return ( ID3D10Device* )pTrialDevice;

        /*
            // Not required for DX10, Device can be retrieved via SwapChain

            // Set Defaults
            static INT_PTR      dxoffset    = 0xC00 + sizeof(INT_PTR);
            static DWORD        dxoffsetlen = sizeof(dxoffset);

            static INT_PTR      dxdata[3];
            static DWORD        dxdatalen   = sizeof(dxdata);

            static bool bFirstCall = true;

        #ifdef _WIN64
            static LPCTSTR sSubKeyData = D3D_DATA SEP D3D_TARGETX64 SEP D3D_TARGETDX10;
            static LPCTSTR sSubKeyOffset = D3D_OFFSET SEP D3D_TARGETX64 SEP D3D_TARGETDX10;
            if(bFirstCall)
            {
                dxdata[0] = 0x000000000001ce3c;
                dxdata[1] = 0x0000000000002f30;
                dxdata[2] = 0x0000000000002f00;
            }
        #else
            static LPCTSTR sSubKeyData = D3D_DATA SEP D3D_TARGETX86 SEP D3D_TARGETDX10;
            static LPCTSTR sSubKeyOffset = D3D_OFFSET SEP D3D_TARGETX86 SEP D3D_TARGETDX10;
            if(bFirstCall)
            {
                dxdata[0] = 0x00006F19;
                dxdata[1] = 0x00006992;
                dxdata[2] = 0x00006969;
            }
        #endif
            bFirstCall = false;

            void* pInterfaceClass = (void*)(nRelativeBase + dxoffset);
            int nFunctioncount = 98 // dx10 device;

            // Calculate Offsets of IUnknown Interface VTable
            dxdata[0] += nModuleOffset;
            dxdata[1] += nModuleOffset;
            dxdata[2] += nModuleOffset;
            bool bInterfaceOk = CheckForInterface<IUnknown>(pInterfaceClass, dxdata, dxdatalen, __uuidof(ID3D11Device), nFunctioncount);
            dxdata[0] -= nModuleOffset;
            dxdata[1] -= nModuleOffset;
            dxdata[2] -= nModuleOffset;

            // Offset already found
            if(bInterfaceOk)
                return *(ID3D11Device**)pInterfaceClass;

            // Search for offset
            return FindInterface<ID3D10Device, IUnknown>(
                nModuleOffset,
                nRelativeBase,
                nFunctioncount,
                0xFFF,
                sSubKeyData,
                dxdata,
                dxdatalen,
                sSubKeyOffset,
                dxoffset,
                dxoffsetlen,
                &GetD3D11DeviceData);
        */
    }

    CD3DSystem10::CD3DSystem10()
    {
        gD3DSystem10 = this;

        m_bD3DHookInstalled = false;
        m_nTextureMode = HTM_NONE;
        m_pTempTex = NULL;
        m_pSwapChain = NULL;

        void* pTrialDevice = NULL;
#if CDK_VERSION < 350
        pTrialDevice = gEnv->pRenderer->EF_Query( EFQ_D3DDevice );
#elif CDK_VERSION > 354
        pTrialDevice = gEnv->pRenderer->EF_Query( EFQ_D3DDevice, pTrialDevice );
#endif
        m_pDevice = FindD3D10Device( ( INT_PTR )gEnv->pRenderer, pTrialDevice );

        // Hook Swap Chain
        if ( m_pDevice )
        {
            if ( BootstrapD3D10SwapChainHook() )
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
            gPlugin->LogAlways( "DX10 device found" );
        }

        else
        {
            gPlugin->LogWarning( "DX10 device not found" );
        }
    }

    CD3DSystem10::~CD3DSystem10()
    {
        hookD3D( false );

        gD3DSystem10 = NULL;
    }

    void CD3DSystem10::hookD3D( bool bHook )
    {
#if defined(D3D_DISABLE_HOOK)
        return;
#endif

        if ( m_pDevice )
        {
            if ( bHook && m_bD3DHookInstalled )
            {
                if ( m_pSwapChain )
                {
                    rehookVT( m_pSwapChain, IDXGISwapChain, Present );
                }

                rehookVT( m_pDevice, ID3D10Device, CreateTexture2D );
                //rehookVT( m_pDevice, ID3D10Device, GetImmediateContext );
                //rehookVT( m_pDeviceCtx, ID3D11DeviceContext, ClearRenderTargetView );
                //rehookVT( m_pDeviceCtx, ID3D11DeviceContext, End );
                //rehookVT( m_pDeviceCtx, ID3D11DeviceContext, GSSetShader );
            }

            else if ( bHook )
            {
                hookVT( m_pDevice, ID3D10Device, CreateTexture2D );
                //hookVT( m_pDevice, ID3D10Device, GetImmediateContext );
                //hookVT( m_pDeviceCtx, ID3D11DeviceContext, ClearRenderTargetView );
                //hookVT( m_pDeviceCtx, ID3D11DeviceContext, End );
                //hookVT( m_pDeviceCtx, ID3D11DeviceContext, GSSetShader );
            }

            else if ( m_bD3DHookInstalled )
            {
                // For Unloading
                if ( m_pSwapChain )
                {
                    unhookVT( m_pSwapChain, IDXGISwapChain, Present, false );
                }

                unhookVT( m_pDevice, ID3D10Device, CreateTexture2D, false  );
                //unhookVT( m_pDevice, ID3D10Device, GetImmediateContext, false  );
                //unhookVT( m_pDeviceCtx, ID3D11DeviceContext, ClearRenderTargetView, false  );
                //unhookVT( m_pDeviceCtx, ID3D11DeviceContext, End, false  );
                //unhookVT( m_pDeviceCtx, ID3D11DeviceContext, GSSetShader, false );
            }

            m_bD3DHookInstalled = bHook;
        }
    }

    ITexture* CD3DSystem10::CreateTexture( void** pD3DTextureDst, int width, int height, int numMips, ETEX_Format eTF, int flags )
    {
        gD3DSystem10->m_nTextureMode = HTM_CREATE;
        int iTex = gEnv->pRenderer->SF_CreateTexture( width, height, numMips, NULL, eTF, flags ); // Create Texture
        *pD3DTextureDst = m_pTempTex;
        m_pTempTex = NULL;

        return gEnv->pRenderer->EF_GetTextureByID( iTex );
    }

    ITexture* CD3DSystem10::InjectTexture( void* pD3DTextureSrc, int nWidth, int nHeight, ETEX_Format eTF, int flags )
    {
        gD3DSystem10->m_nTextureMode = HTM_INJECT;
        m_pTempTex = pD3DTextureSrc;
        int iTex = gEnv->pRenderer->SF_CreateTexture( nWidth, nHeight, 1, NULL, eTF, flags ); // Create Dummytexture and replace it to trick CE3 into using our texture.
        m_pTempTex = NULL;

        return gEnv->pRenderer->EF_GetTextureByID( iTex );
    }

}