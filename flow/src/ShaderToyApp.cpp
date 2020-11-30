#include "cinder/FileWatcher.h"
#include "cinder/Log.h"
#include "cinder/Utilities.h"
#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/params/Params.h"

#include "AssetManager.h"
#include "FontHelper.h"
#include "MiniConfig.h"
#include "NvOptimusEnablement.h"

#include <time.h>

using namespace ci;
using namespace ci::app;
using namespace std;

class FlowApp : public App
{
  public:
    vector<string> listShaderFiles()
    {
        vector<string> files;
        auto assetRoot = getAssetPath("");
        for (auto& p : fs::directory_iterator(assetRoot))
        {
            auto ext = p.path().extension();
            if (ext == ".frag" || ext == ".fs" || ext == ".fragment")
            {
                auto filename = p.path().generic_string();
                filename.replace(filename.find(assetRoot.generic_string()),
                                 assetRoot.generic_string().size(),
                                 ""); // Left trim the assets prefix

                files.push_back(filename);
            }
        }

        return files;
    }

    void setup() override
    {
        log::makeLogger<log::LoggerFile>();
        _RENDERER = (char*)glGetString(GL_RENDERER);
        am::addAssetDirectory("../assets");

        texFont = FontHelper::createTextureFont("Helvetica", 24);

        gl::enableDepth();

        getWindow()->getSignalMove().connect([&] {
            _APP_X = getWindowPosX();
            _APP_Y = getWindowPosY();
        });

        getWindow()->getSignalResize().connect([&] {
            _APP_WIDTH = getWindowWidth();
            _APP_HEIGHT = getWindowHeight();
        });

        getWindow()->getSignalMouseDown().connect([&](MouseEvent event) {
            mMouse.x = (float)event.getPos().x;
            mMouse.y = _APP_HEIGHT - (float)event.getPos().y;
            mMouse.z = mMouse.x;
            mMouse.w = mMouse.y;
        });

        getWindow()->getSignalMouseDrag().connect([&](MouseEvent event) {
            mMouse.x = (float)event.getPos().x;
            mMouse.y = _APP_HEIGHT - (float)event.getPos().y;
        });

        getSignalCleanup().connect([&] { writeConfig(); });

        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            auto key = event.getCode();
            if (key == KeyEvent::KEY_ESCAPE) quit();
            if (key == KeyEvent::KEY_f) setFullScreen(!isFullScreen());
            if (key == KeyEvent::KEY_g) GUI_VISIBLE = !GUI_VISIBLE;
            if (key == KeyEvent::KEY_F4)
                launchWebBrowser(Url(getAssetPath(mShaderNames[SHADER_ID]).string(), true));
        });

        mLoadingContext = gl::env()->createSharedContext(gl::context());

        mShaderNames = listShaderFiles();
#if defined(CINDER_MSW_DESKTOP) || defined(CINDER_LINUX) || defined(CINDER_MAC)
        auto params = createConfigUI({300, 500});
        params->addText("Press F4 to edit current shader");
        ADD_ENUM_TO_INT(params.get(), SHADER_ID, mShaderNames);
        params->addParam("TEST_VEC4.x", &TEST_VEC4.x).group("TEST_VEC4").label("x").step(0.01f);
        params->addParam("TEST_VEC4.y", &TEST_VEC4.y).group("TEST_VEC4").label("y").step(0.02f);
        params->addParam("TEST_VEC4.z", &TEST_VEC4.z).group("TEST_VEC4").label("z").step(0.04f);
        params->addParam("TEST_VEC4.w", &TEST_VEC4.w).group("TEST_VEC4").label("w").step(0.08f);
        params->addParam("TEST_COLOR", &TEST_COLOR);
        params->addParam("TEST_ANGLES", &TEST_ANGLES);
        mParams = params;
#endif
        mChannel0 = am::texture2d(TEX0_NAME);
        mChannel1 = am::texture2d(TEX1_NAME);
        mChannel2 = am::texture2d(TEX2_NAME);
        mChannel3 = am::texture2d(TEX3_NAME);

        gl::enableVerticalSync(_V_SYNC);

        getSignalUpdate().connect([&] {
            _FPS = getAverageFps();
            // Calculate shader parameters.
            // https://www.shadertoy.com/howto
            vec3 iResolution(vec2(getWindowSize()), 1);
            float iGlobalTime = (float)getElapsedSeconds();
            float iTimeDelta = 1.0f / _FPS;
            float iFrame = (float)getElapsedFrames();
            float iChannelTime0 = (float)getElapsedSeconds();
            float iChannelTime1 = (float)getElapsedSeconds();
            float iChannelTime2 = (float)getElapsedSeconds();
            float iChannelTime3 = (float)getElapsedSeconds();
            vec3 iChannelResolution0 = mChannel0 ? vec3(mChannel0->getSize(), 1) : vec3(1);
            vec3 iChannelResolution1 = mChannel1 ? vec3(mChannel1->getSize(), 1) : vec3(1);
            vec3 iChannelResolution2 = mChannel2 ? vec3(mChannel2->getSize(), 1) : vec3(1);
            vec3 iChannelResolution3 = mChannel3 ? vec3(mChannel3->getSize(), 1) : vec3(1);
            float iSampleRate = 44100;

            time_t now = time(0);
            tm* t = gmtime(&now);
            vec4 iDate(float(t->tm_year + 1900), float(t->tm_mon), float(t->tm_mday),
                       float(t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec));

            if (mShaderID != SHADER_ID)
            {
                mShaderID = SHADER_ID;

                mWatchConnection.disconnect();
                mWatchConnection = mFileWatcher.watch(
                    getAssetPath(mShaderNames[SHADER_ID]), [&](const WatchEvent& event) {
                        try
                        {
                            mShaderError = "";
                            std::string vs = am::str("common/shadertoy.vert");
                            std::string fs = am::str("common/shadertoy.frag") +
                                             loadString(loadAsset(mShaderNames[SHADER_ID]));

                            mLoadingContext->makeCurrent();
                            auto format = gl::GlslProg::Format().vertex(vs).fragment(fs);
                        // https://github.com/mattdesl/lwjgl-basics/wiki/GLSL-Versions
#if defined(CINDER_GL_ES)
                            format.version(300); // es 3.0
#else
                            format.version(330); // gl 3.3
#endif
                            auto newGlslProg = gl::GlslProg::create(format);
                            dispatchAsync([&, newGlslProg] { mGlslProg = newGlslProg; });
                        }
                        catch (const std::exception& e)
                        {
                            // Uhoh, something went wrong, but it's not fatal.
                            CI_LOG_EXCEPTION("Failed to compile the shader: ", e);
                            mGlslProg.reset();
                            mShaderError = e.what();
                        }
                    });
            }
            if (mGlslProg)
            {
                // Set shader uniforms.
                mGlslProg->uniform("iResolution", iResolution);
                mGlslProg->uniform("iTime", iGlobalTime);
                mGlslProg->uniform("iTimeDelta", iTimeDelta);
                mGlslProg->uniform("iFrame", iFrame);
                mGlslProg->uniform("iChannelTime[0]", iChannelTime0);
                mGlslProg->uniform("iChannelTime[1]", iChannelTime1);
                mGlslProg->uniform("iChannelTime[2]", iChannelTime2);
                mGlslProg->uniform("iChannelTime[3]", iChannelTime3);
                mGlslProg->uniform("iChannelResolution[0]", iChannelResolution0);
                mGlslProg->uniform("iChannelResolution[1]", iChannelResolution1);
                mGlslProg->uniform("iChannelResolution[2]", iChannelResolution2);
                mGlslProg->uniform("iChannelResolution[3]", iChannelResolution3);
                mGlslProg->uniform("iMouse", mMouse);
                mGlslProg->uniform("iChannel0", 0);
                mGlslProg->uniform("iChannel1", 1);
                mGlslProg->uniform("iChannel2", 2);
                mGlslProg->uniform("iChannel3", 3);
                mGlslProg->uniform("iDate", iDate);
                mGlslProg->uniform("iSampleRate", iSampleRate);

                mGlslProg->uniform("TEST_VEC4", TEST_VEC4);
                mGlslProg->uniform("TEST_COLOR", TEST_COLOR);
                mGlslProg->uniform("TEST_ANGLES", glm::eulerAngles(TEST_ANGLES));
            }
        });

        getWindow()->getSignalDraw().connect([&] {
            gl::clear();
            mParams->show(GUI_VISIBLE);

            if (!mGlslProg)
            {
                gl::ScopedColor clr(Color(1, 0, 0));
                texFont->drawString(mShaderError, {10, _APP_HEIGHT - 150});
                return;
            }

            gl::ScopedTextureBind tex0(mChannel0, 0);
            gl::ScopedTextureBind tex1(mChannel1, 1);
            gl::ScopedTextureBind tex2(mChannel2, 2);
            gl::ScopedTextureBind tex3(mChannel3, 3);
            gl::ScopedGlslProg glsl(mGlslProg);

            gl::drawSolidRect(Rectf(0, (float)getWindowHeight(), (float)getWindowWidth(), 0));

#if 0
            gl::disableAlphaBlending();
            gl::setMatricesWindow( app::getWindowSize() ); 
            gl::drawString(mShaderError, { 10, 10 });
            gl::disableAlphaBlending();
#endif
        });
    }

  private:
    gl::GlslProgRef mGlslProg;
    gl::ContextRef mLoadingContext;
    vec4 TEST_VEC4 = {0.01, 0.01, 0.01, 0.01};
    ColorA TEST_COLOR;
    quat TEST_ANGLES;
    //! Texture slots for our shader, based on ShaderToy.
    gl::TextureRef mChannel0;
    gl::TextureRef mChannel1;
    gl::TextureRef mChannel2;
    gl::TextureRef mChannel3;
    //! Our mouse position: xy = current position while mouse down, zw = last click position.
    vec4 mMouse;
    int mShaderID = -1;
    vector<string> mShaderNames;
    FileWatcher mFileWatcher;
    signals::Connection mWatchConnection;

    gl::TextureFontRef texFont;
    string mShaderError;
    params::InterfaceGlRef mParams;
};

CINDER_APP(FlowApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowPos(_APP_X, _APP_Y);
    settings->setWindowSize(_APP_WIDTH, _APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
})
