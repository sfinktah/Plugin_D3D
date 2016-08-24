/* D3D_Plugin - for licensing and copyright see license.txt */

#pragma once

#include <CPluginD3D.h>
#include <ITexture.h>

namespace D3DPlugin
{
    class CD3DSystem11 :
        public IPluginD3DEx,
        private ID3DEventListener
    {
        private:
            bool m_bD3DHookInstalled;
            std::vector<ID3DEventListener*> m_vecQueue;
            void hookD3D( bool bHook );
        public:
            int m_nTextureMode;
            int m_nFeatureLevel;
            void* m_pTempTex;
            void* m_pDevice;
            void* m_pDeviceCtx;
            void* m_pSwapChain;

            string m_sGPUName;

            CD3DSystem11();
            virtual ~CD3DSystem11();

            void Release()
            {
                delete this;
            }

            PluginManager::IPluginBase* GetBase()
            {
                return gPlugin->GetBase();
            };

            void ActivateEventDispatcher( bool bActivate )
            {
                hookD3D( bActivate );
            };

            eD3DType GetType()
            {
                return m_pDevice ? D3D_DX11 : D3D_NONE;
            };

            void* GetDevice()
            {
                return m_pDevice;
            }

            void* SetDevice(void *device)
            {
                return (m_pDevice = device);
            }

            void* GetDeviceContext()
            {
                return m_pDeviceCtx;
            };

            void* SetDeviceContext(void *context)
            {
                return (m_pDeviceCtx = context);
            };

            void* GetSwapChain()
            {
                return m_pSwapChain;
            };

            void* SetSwapChain(void *swapchain)
            {
                return (m_pSwapChain = swapchain);
            };

            ITexture* CreateTexture( void** pD3DTextureDst, int width, int height, int numMips, ETEX_Format eTF, int flags );
            ITexture* InjectTexture( void* pD3DTextureSrc, int nWidth, int nHeight, ETEX_Format eTF, int flags );

        public:
            DECLARE_REGISTER_LISTENER( m_vecQueue );
            DECLARE_UNREGISTER_LISTENER( m_vecQueue );
            DECLARE_BROADCAST_EVENT( m_vecQueue, OnPrePresent );
            DECLARE_BROADCAST_EVENT( m_vecQueue, OnPostPresent );
            DECLARE_BROADCAST_EVENT( m_vecQueue, OnPreReset );
            DECLARE_BROADCAST_EVENT( m_vecQueue, OnPostReset );
            DECLARE_BROADCAST_EVENT( m_vecQueue, OnPostBeginScene );

            virtual int GetFeatureLevel();

            virtual const char* GetGPUName();

    };

    extern CD3DSystem11* gD3DSystem11;
};