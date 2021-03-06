#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/Log.h"

#include "AssetManager.h"
#include "MiniConfigImgui.h"
#include "CinderRemoteImgui.h"
#include "CinderEnkiTS.h"

#include "../../blocks/tracy/Tracy.hpp"
#include "../../blocks/tracy/TracyOpenGL.hpp"

#define TRACY_EVENT_ENABLED 1

using namespace ci;
using namespace ci::app;
using namespace std;

struct enkiTSTestApp : public App
{
    void testEnki()
    {
        enki::TaskScheduler g_TS;

        g_TS.Initialize();

        enki::TaskSet task(1024, [](enki::TaskSetPartition range, uint32_t threadnum) { printf("Thread %d, start %d, end %d\n", threadnum, range.start, range.end); });

        g_TS.AddTaskSetToPipe(&task);
        g_TS.WaitforTask(&task);
    }

    void setup() override
    {
        log::makeLogger<log::LoggerFileRotating>(fs::path(), "IG.%Y.%m.%d.log");
     
#if TRACY_EVENT_ENABLED
        TracyGpuContext;
#endif
        if (ENKI_ENABLED)
            testEnki();

        auto aabb = am::triMesh(MESH_NAME)->calcBoundingBox();
        mCam.lookAt(aabb.getMax() * 2.0f, aabb.getCenter());
        mCamUi = CameraUi( &mCam, getWindow(), -1 );
        
        createConfigImgui(getWindow(), false, false);
        createRemoteImgui(REMOTE_GUI_IP.c_str());

        gl::enableDepth();

        getWindow()->getSignalResize().connect([&] {
            APP_WIDTH = getWindowWidth();
            APP_HEIGHT = getWindowHeight();
            mCam.setAspectRatio( getWindowAspectRatio() );
        });

        getSignalCleanup().connect([&] { writeConfig(); });

        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE) quit();
        });
        
        mGlslProg = am::glslProg(VS_NAME, FS_NAME);
        mGlslProg->uniform("uTex0", 0);
        mGlslProg->uniform("uTex1", 1);
        mGlslProg->uniform("uTex2", 2);
        mGlslProg->uniform("uTex3", 3);

        getSignalUpdate().connect([&] {
#if TRACY_EVENT_ENABLED
            TracyGpuCollect;
#endif
            updateRemoteImgui(ENABLE_REMOTE_GUI);
            ImGui_ImplCinder_NewFrameGuard(getWindow());
            vnm::drawMinicofigImgui(true);
        });

        getWindow()->getSignalDraw().connect([&] {
#if TRACY_EVENT_ENABLED
            ZoneScopedN("frame");
            TracyGpuZone("frame");
#endif
            gl::setMatrices( mCam );
            gl::clear();
        
            gl::ScopedTextureBind tex0(am::texture2d(TEX0_NAME), 0);
            gl::ScopedTextureBind tex1(am::texture2d(TEX1_NAME), 1);
            gl::ScopedTextureBind tex2(am::texture2d(TEX2_NAME), 2);
            gl::ScopedTextureBind tex3(am::texture2d(TEX3_NAME), 3);
            gl::ScopedGlslProg glsl(mGlslProg);

            {
#if TRACY_EVENT_ENABLED
                ZoneScopedN("draw");
                TracyGpuZone("draw");
#endif
                gl::draw(am::vboMesh(MESH_NAME));
            }

            ImGui_ImplCinder_PostDraw(ENABLE_REMOTE_GUI, ENABLE_LOCAL_GUI);

#if TRACY_EVENT_ENABLED
            FrameMark;
#endif
        });
    }
    
    CameraPersp         mCam;
    CameraUi            mCamUi;
    gl::GlslProgRef     mGlslProg;
};

CINDER_APP( enkiTSTestApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
    settings->setConsoleWindowEnabled();
} )
