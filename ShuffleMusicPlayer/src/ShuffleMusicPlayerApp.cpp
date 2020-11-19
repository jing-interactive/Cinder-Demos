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
    float mProgress = -1;
    audio::VoiceSamplePlayerNodeRef mCurrentVoice;

    void playNextMusic()
    {
        mCurrentVoice = am::voice(MUSIC_FILE);
        mCurrentVoice->start();
    }

    void setup() override
    {
        log::makeLogger<log::LoggerFile>();
        createConfigImgui();

        playNextMusic();
    
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
                auto numSeconds = node->getNumSeconds();
                if (abs(mProgress - PROGRESS) * numSeconds > 1)
                {
                    node->seekToTime(PROGRESS * numSeconds);
                    mProgress = PROGRESS;
                }
                else
                {
                    mProgress = PROGRESS = node->getReadPositionTime() / numSeconds;
                }
            }

            mCurrentVoice->setPan(PAN);
            mCurrentVoice->setVolume(VOLUME);

            gl::clear();

            if (node->isEof())
            {
                playNextMusic();
            }
        });
    }
};

CINDER_APP( RPlayer, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
} )
