#define VULKAN_RENDERER 0

#include "cinder/app/App.h"
#include "cinder/ObjLoader.h"
#include "cinder/CameraUI.h"
#include "cinder/Log.h"

using namespace ci;
using namespace ci::app;
using namespace std;

#if VULKAN_RENDERER
#include "cinder/app/RendererVk.h"
#include "SanctumCathedralVk.h"
#else
#include "cinder/app/RendererGl.h"
#include "SanctumCathedralGl.h"
#endif

#if defined CINDER_MSW_OCULUS_RIFT
    #include "CinderOculus.h"
using namespace hmd;
#endif


class SanctumVRApp : public App {
  public:
    SanctumVRApp();
    
	void update() override;
	void draw() override;
  private:
    void drawScene();
    
    CameraPersp         mCam;
    CameraUi            mCamUi;
    SanctumCathedral    mSanctum;
    
#if defined CINDER_MSW_OCULUS_RIFT
    OculusRiftRef		mRift;
#endif
    
    vec3                mViewerPosition;
};

SanctumVRApp::SanctumVRApp()
    : mViewerPosition( 0.0f, 1.0f, 0.0f )
{
    mCamUi = CameraUi( &mCam );
    mCamUi.connect( getWindow() );
    mCam.setPerspective( 65.0f, getWindowAspectRatio(), 0.1, 10000 );
    mCam.setEyePoint( vec3( 5.0f, 0.0f, -25.0f ) );
    mCam.lookAt( vec3( 0.0f, 8.0f, 0.0f ) );

    try
    {
        mSanctum.setupModel();
    }
    catch (const std::exception &e) {
        CI_LOG_EXCEPTION("Error loading asset", e);
        quit();
    }

    gfx::enableDepthWrite();
    gfx::enableDepthRead();
    gfx::enableAlphaBlending();

#if defined CINDER_MSW_OCULUS_RIFT
	try {
		mRift = OculusRift::create();
	}
	catch (const RiftExeption& exc) {
		CI_LOG_EXCEPTION("Failed rift initialization.", exc);
	}
#endif
}


#if defined CINDER_MSW_OCULUS_RIFT

void SanctumVRApp::update()
{
    // Move head location
    if( mRift ) {
        auto host = mRift->getHostCamera();
        host.setEyePoint( mViewerPosition );
        host.lookAt( vec3( 0 ) );
        mRift->setHostCamera( host );
    }
    
    // Draw from update due to conflicting WM_PAINT signal emitted by ovr_submitFrame (0.7 SDK).
    gfx::clear( Color( 1.0f, 1.0f, 1.0f ) );

    if( mRift && ! mRift->isFrameSkipped() ) {
        ScopedRiftBuffer bind{ mRift };
        
        for( auto eye : mRift->getEyes() ) {
            mRift->enableEye( eye );
            
            drawScene();
            
            // Draw positional tracking camera frustum
            CameraPersp positional;
            if( mRift->getPositionalTrackingCamera( &positional ) ) {
                gfx::setModelMatrix( mat4() );
                gfx::lineWidth( 1.0f );
                gfx::drawFrustum( positional );
            }
        }
    }
}

void SanctumVRApp::drawScene()
{
	gfx::pushMatrices();
	gfx::rotate( -M_PI * 0.5f, vec3(1.0f, 0.0f, 0.0f ) );
	//gfx::scale( vec3( 0.1f ); // this doesnt affect the perception of scale
	gfx::translate( vec3( 8.0f *cos( getElapsedSeconds()*0.1f ), 2.0f, 15.0f * sin( getElapsedSeconds()*0.1f ) ) );
    mSanctum.draw();
	gfx::popMatrices();
}

void SanctumVRApp::draw()
{
    if( ! mRift ) {
        gfx::viewport( getWindowSize() );
        gfx::setMatrices( mCam );
        
        drawScene();
    }
}



#else // is OSX

void SanctumVRApp::update()
{
    gfx::setMatrices(mCam);
    mSanctum.update();
}

void SanctumVRApp::drawScene(){}

void SanctumVRApp::draw()
{
    gfx::clear();
    
    mSanctum.draw();
}
#endif

void prepareSettings(App::Settings *settings)
{
#if defined CINDER_MSW_OCULUS_RIFT
	try{
		RiftManager::initialize();
	}
	catch (const RiftExeption& exc) {
		CI_LOG_EXCEPTION("Failed ovr initialization", exc);
	}
#endif
	settings->setTitle("Sanctum VR");
	settings->setWindowSize(1920 / 2, 1080 / 2);

#ifdef _DEBUG
    settings->setConsoleWindowEnabled();
#endif
}

#if VULKAN_RENDERER

VkBool32 debugReportVk(
    VkDebugReportFlagsEXT      flags,
    VkDebugReportObjectTypeEXT objectType,
    uint64_t                   object,
    size_t                     location,
    int32_t                    messageCode,
    const char*                pLayerPrefix,
    const char*                pMessage,
    void*                      pUserData
    )
{
    if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
        CI_LOG_I( "[" << pLayerPrefix << "] : " << pMessage << " (" << messageCode << ")" );
    }
    else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
        CI_LOG_W( "[" << pLayerPrefix << "] : " << pMessage << " (" << messageCode << ")" );
    }
    else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
        CI_LOG_I( "[" << pLayerPrefix << "] : " << pMessage << " (" << messageCode << ")" );
    }
    else if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
        CI_LOG_E("[" << pLayerPrefix << "] : " << pMessage << " (" << messageCode << ")");
    }
    else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
        CI_LOG_D( "[" << pLayerPrefix << "] : " << pMessage << " (" << messageCode << ")" );
    }
    return VK_FALSE;
}

const std::vector<std::string> gLayers = {
    //"VK_LAYER_LUNARG_api_dump",
    //"VK_LAYER_LUNARG_standard_validation",
    //"VK_LAYER_LUNARG_threading",
    //"VK_LAYER_LUNARG_mem_tracker",
    //"VK_LAYER_LUNARG_object_tracker",
    //"VK_LAYER_LUNARG_draw_state",
    //"VK_LAYER_LUNARG_param_checker",
    //"VK_LAYER_LUNARG_swapchain",
    //"VK_LAYER_LUNARG_device_limits",
    //"VK_LAYER_LUNARG_image",
    //"VK_LAYER_GOOGLE_unique_objects",
};

CINDER_APP(SanctumVRApp, 
    RendererVk(RendererVk::Options()
        .setSamples(VK_SAMPLE_COUNT_1_BIT)
        .setLayers(gLayers)
        .setDebugReportCallbackFn(debugReportVk)
    ),
    prepareSettings)

#else

CINDER_APP(SanctumVRApp, 
    RendererGl(RendererGl::Options().msaa(4)),
    prepareSettings)

#endif
