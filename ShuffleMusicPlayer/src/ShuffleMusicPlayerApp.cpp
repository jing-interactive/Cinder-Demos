#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"

#include "AssetManager.h"
#include "MiniConfig.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class ShuffleMusicPlayerApp : public App
{
  public:
    void setup() override
    {
        log::makeLogger<log::LoggerFile>();
        createConfigUI({200, 200});

        mCurrentVoice = am::voice(MUSIC_FILE);
        mCurrentVoice->start();
    
        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE) quit();
        });
        
        getWindow()->getSignalDraw().connect([&] {
            gl::clear();

            mCurrentVoice->setPan(PAN);
            mCurrentVoice->setVolume(VOLUME);
        });
    }
    
private:
    audio::VoiceRef mCurrentVoice;
};

CINDER_APP( ShuffleMusicPlayerApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
} )
