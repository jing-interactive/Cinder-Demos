#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/Log.h"

#include "AssetManager.h"
#include "MiniConfigImgui.h"

#include "UnrealTrace.h"

UE_TRACE_CHANNEL_DEFINE(VinjnChannel)

UE_TRACE_EVENT_BEGIN(VinjnLogger, Frame)
    UE_TRACE_EVENT_FIELD(uint64, TimestampBase)
    UE_TRACE_EVENT_FIELD(uint32, RenderingFrameNumber)
UE_TRACE_EVENT_END()

// C:\UE_4.26\Engine\Source\Runtime\Core\Private\ProfilingDebugging\TraceAuxiliary.cpp
UE_TRACE_EVENT_BEGIN(Diagnostics, Session2, Important)
    UE_TRACE_EVENT_FIELD(Trace::AnsiString, Platform)
    UE_TRACE_EVENT_FIELD(Trace::AnsiString, AppName)
    UE_TRACE_EVENT_FIELD(Trace::WideString, CommandLine)
    UE_TRACE_EVENT_FIELD(uint8, ConfigurationType)
    UE_TRACE_EVENT_FIELD(uint8, TargetType)
UE_TRACE_EVENT_END()

using namespace ci;
using namespace ci::app;
using namespace std;

struct UnrealInsightsTestApp : public App
{
    void setup() override
    {
        log::makeLogger<log::LoggerFileRotating>(fs::path(), "app.%Y.%m.%d.log");

        {
#if 0
            auto args = Trace::FChannel::InitArgs();
            DummyChannel.Setup("dummy", args);
            VinjnChannelObject.Setup("vinjn", args);
#endif
            // Trace out information about this session. This is done before initialisation
            // so that it is always sent (all channels are enabled prior to initialisation)
            UE_TRACE_LOG(Diagnostics, Session2, Trace::TraceLogChannel)
                << Session2.Platform("Win64")
                << Session2.AppName("UnrealInsightsTestApp")
                << Session2.CommandLine(L"")
                << Session2.ConfigurationType(1)
                << Session2.TargetType(1);

            auto desc = Trace::FInitializeDesc();
            desc.bUseWorkerThread = true;
            Trace::Initialize(desc);
            Trace::FChannel::ToggleAll(true);

            Trace::SendTo(L"127.0.0.1", HOST_PORT);
        }

        auto aabb = am::triMesh(MESH_NAME)->calcBoundingBox();
        mCam.lookAt(aabb.getMax() * 2.0f, aabb.getCenter());
        mCamUi = CameraUi( &mCam, getWindow(), -1 );

        createConfigImgui();
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

        getSignalUpdate().connect([&] {
            IS_TRACING = Trace::IsTracing();
            uint64 TimestampBase = getElapsedSeconds() * 1000000;
            uint32 RenderingFrameNumber = getElapsedFrames();
            int EventBufferSize = sizeof(TimestampBase) + sizeof(RenderingFrameNumber);
            UE_TRACE_LOG(VinjnLogger, Frame, Trace::TraceLogChannel)
                << Frame.TimestampBase(TimestampBase)
                << Frame.RenderingFrameNumber(RenderingFrameNumber)
                //<< Frame.Attachment(Frame.EventBuffer, Frame.EventBufferSize)
                ;

            Trace::Update();
        });

        mGlslProg = am::glslProg(VS_NAME, FS_NAME);
        mGlslProg->uniform("uTex0", 0);
        mGlslProg->uniform("uTex1", 1);
        mGlslProg->uniform("uTex2", 2);
        mGlslProg->uniform("uTex3", 3);

        getWindow()->getSignalDraw().connect([&] {
            gl::setMatrices( mCam );
            gl::clear();
        
            gl::ScopedTextureBind tex0(am::texture2d(TEX0_NAME), 0);
            gl::ScopedTextureBind tex1(am::texture2d(TEX1_NAME), 1);
            gl::ScopedTextureBind tex2(am::texture2d(TEX2_NAME), 2);
            gl::ScopedTextureBind tex3(am::texture2d(TEX3_NAME), 3);
            gl::ScopedGlslProg glsl(mGlslProg);

            gl::draw(am::vboMesh(MESH_NAME));
        });
    }
    
    CameraPersp         mCam;
    CameraUi            mCamUi;
    gl::GlslProgRef     mGlslProg;
};

CINDER_APP( UnrealInsightsTestApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
} )
