#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/Log.h"

#include "AssetManager.h"
#include "MiniConfigImgui.h"
#include "CinderRemoteImgui.h"

#include "../../blocks/enkiTS/enkiTS/src/TaskScheduler.h"
#include "../../blocks/tracy/Tracy.hpp"
#include "../../blocks/tracy/TracyOpenGL.hpp"

using namespace ci;
using namespace ci::app;
using namespace std;

struct enkiTSTestApp : public App
{
    enki::TaskScheduler g_TS;

    void setup() override
    {
        log::makeLogger<log::LoggerFileRotating>(fs::path(), "IG.%Y.%m.%d.log");
     
        TracyGpuContext;

        g_TS.Initialize();

        enki::TaskSet task(1024, [](enki::TaskSetPartition range, uint32_t threadnum) { printf("Thread %d, start %d, end %d\n", threadnum, range.start, range.end); });

        g_TS.AddTaskSetToPipe(&task);
        g_TS.WaitforTask(&task);

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
            TracyGpuCollect;

            updateRemoteImgui(ENABLE_REMOTE_GUI);
            ImGui_ImplCinder_NewFrameGuard(getWindow());
            vnm::drawMinicofigImgui(true);
        });

        getWindow()->getSignalDraw().connect([&] {
            ZoneScopedN("frame");
            TracyGpuZone("frame");

            gl::setMatrices( mCam );
            gl::clear();
        
            gl::ScopedTextureBind tex0(am::texture2d(TEX0_NAME), 0);
            gl::ScopedTextureBind tex1(am::texture2d(TEX1_NAME), 1);
            gl::ScopedTextureBind tex2(am::texture2d(TEX2_NAME), 2);
            gl::ScopedTextureBind tex3(am::texture2d(TEX3_NAME), 3);
            gl::ScopedGlslProg glsl(mGlslProg);

            {
                ZoneScopedN("draw");
                TracyGpuZone("draw");
                gl::draw(am::vboMesh(MESH_NAME));
            }

            ImGui_ImplCinder_PostDraw(ENABLE_REMOTE_GUI, ENABLE_LOCAL_GUI);

            FrameMark;
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
