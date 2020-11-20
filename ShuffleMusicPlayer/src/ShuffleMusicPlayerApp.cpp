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
    vector<fs::path> mMusicPaths;
    vector<audio::VoiceSamplePlayerNodeRef> mVoices;

    bool addMusic(const fs::path& filePath)
    {
        mMusicPaths.clear();
        return addMusic(filePath, 0);
    }

    bool addMusic(const fs::path& filePath, int depth)
    {
        static auto& audioExts = audio::SourceFile::getSupportedExtensions();
        if (fs::is_regular_file(filePath))
        {
            auto ext = filePath.extension().string().substr(1);
            bool isSupported = std::find(audioExts.begin(), audioExts.end(), ext) != audioExts.end();
            if (isSupported)
            {
                mMusicPaths.push_back(filePath);
            }
        }
        else if (depth == 0 && fs::is_directory(filePath))
        {
            fs::directory_iterator kEnd;
            for (fs::directory_iterator it(filePath); it != kEnd; ++it)
            {
                addMusic(*it, depth + 1);
            }
        }

        return true;
    }

    void shuffleMusic()
    {
        for (auto& voice : mVoices)
        {
            voice->stop();
        }
        mVoices.clear();
        for (auto& path : mMusicPaths)
        {
            auto voice = am::voice(path.string());
            voice->getSamplePlayerNode()->setLoopEnabled(LOOP_MODE);
            voice->start();
            mVoices.emplace_back(voice);
        }
    }

    void setup() override
    {
        log::makeLogger<log::LoggerFile>();
        createConfigImgui();

        addMusic(getAssetPath(MUSIC_PATH));
        shuffleMusic();
    
        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE) quit();
        });

        getWindow()->getSignalResize().connect([&] {
            APP_WIDTH = getWindowWidth();
            APP_HEIGHT = getWindowHeight();
        });

        getWindow()->getSignalFileDrop().connect([&](FileDropEvent& event) {
            for (auto& filePath : event.getFiles())
            {
                if (addMusic(filePath))
                {
                    MUSIC_PATH = filePath.string();
                    break;
                }
            }
            shuffleMusic();
        });

        getSignalCleanup().connect([&] { writeConfig(); });
        
        getSignalUpdate().connect([&] {
            if (mVoices.size() > 1)
            {
                // TODO: inspect every voices, like a DJ software (XD)
                auto& voice = mVoices[0];
                auto& node = voice->getSamplePlayerNode();
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

            for (auto& voice : mVoices)
            {
                voice->setPan(PAN);
                voice->setVolume(VOLUME);
                auto& node = voice->getSamplePlayerNode();
                node->setLoopEnabled(LOOP_MODE);

#if 0
                auto& node = voice->getSamplePlayerNode();
                if (node->isEof())
                {
                    voice->stop();
                    voice->start();
                }
#endif
            }
        });
        
        getWindow()->getSignalDraw().connect([&] {
            gl::clear();
        });
    }
};

CINDER_APP( RPlayer, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
} )
