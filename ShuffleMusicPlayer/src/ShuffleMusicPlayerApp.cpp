#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"

#include "AssetManager.h"
#include "MiniConfigImgui.h"

using namespace ci;
using namespace ci::app;
using namespace std;

struct RPlayer : public App
{
    int mProgress = -1;
    audio::VoiceSamplePlayerNodeRef mCurrentVoice;

    void setup() override
    {
        log::makeLogger<log::LoggerFile>();
        createConfigImgui();

        mCurrentVoice = am::voice(MUSIC_FILE);
        mCurrentVoice->start();
    
        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE) quit();
        });

        getWindow()->getSignalResize().connect([&] {
            APP_WIDTH = getWindowWidth();
            APP_HEIGHT = getWindowHeight();
        });

        getSignalCleanup().connect([&] { writeConfig(); });
        
        getWindow()->getSignalDraw().connect([&] {
            auto node = mCurrentVoice->getSamplePlayerNode();
            {
                if (mProgress != PROGRESS)
                {
                    node->seek(PROGRESS);
                    mProgress = PROGRESS;
                }
                else
                {
                    mProgress = PROGRESS = node->getReadPosition();
                }
            }
            gl::clear();

            mCurrentVoice->setPan(PAN);
            mCurrentVoice->setVolume(VOLUME);
        });
    }
};

CINDER_APP( RPlayer, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
} )
