#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Log.h"
#include "cinder/timeline.h"

#include "AssetManager.h"
#include "MiniConfig.h"

using namespace ci;
using namespace ci::app;
using namespace std;

const int SEQUENCE_COUNT = 9;
const int SOUND_COUNT = 4;

Anim<float> tick;

class BeatBoxerApp : public App
{
  public:
    void setup() override
    {
        log::makeLogger<log::LoggerFile>();
        createConfigUI({200, 200});
    
        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE) quit();
        });
        
        getSignalUpdate().connect([&] {
//            mSounds[0]->start();
        });
        
        getWindow()->getSignalDraw().connect([&] {
            gl::clear();
            gl::setMatricesWindow(getWindowSize());
            
            float x0 = getWindowWidth() * CANVAS_MARGIN;
            float y0 = getWindowHeight() * CANVAS_MARGIN;
            float dx =
            
            for (int k=0;k<SEQUENCE_COUNT;k++)
            {
                for (int i=0;i<SOUND_COUNT;i++)
                {
//                    mSounds[i][k] = am::voice(sounds[i]);
                }
            }
        });
        
        const char* sounds[SOUND_COUNT] = {
            "sounds/bass_drum.wav",
            "sounds/snare_drum.wav",
            "sounds/low_tom.wav",
            "sounds/mid_tom.wav",
            
            // 'sounds/hi_tom.wav',
            // 'sounds/rim_shot.wav',
            // 'sounds/hand_clap.wav',
            // 'sounds/cowbell.wav',
            // 'sounds/cymbal.wav',
            // 'sounds/o_hi_hat.wav',
            // 'sounds/cl_hi_hat.wav',
            
            // 'sounds/low_conga.wav',
            // 'sounds/mid_conga.wav',
            // 'sounds/hi_conga.wav',
            // 'sounds/claves.wav',
            // 'sounds/maracas.wav',
        };

        for (int i=0;i<SOUND_COUNT;i++)
        {
            auto source = audio::load(DataSourcePath::create(getAssetPath(sounds[i])));
            for (int k=0;k<SEQUENCE_COUNT;k++)
            {
                mSounds[i][k] = audio::Voice::create(source);
            }
        }
    }
    
private:
    bool mIsEnabled[SOUND_COUNT][SEQUENCE_COUNT];
    audio::VoiceSamplePlayerNodeRef mSounds[SOUND_COUNT][SEQUENCE_COUNT];
};

CINDER_APP( BeatBoxerApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
} )
