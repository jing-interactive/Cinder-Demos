#include "pxcvideomodule.h"
#include "pxcsensemanager.h"
#include "pxccapture.h"
#include "pxcmetadata.h"
#include "pxcfacemodule.h"

#include "opencv2/core.hpp"
#include "opencv2/imgcodecs.hpp"

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/params/params.h"
#include "cinder/Log.h"

#include "Cinder-VNM/include/AssetManager.h"
#include "Cinder-VNM/include/MiniConfig.h"
#include "Cinder-VNM/include/TextureHelper.h"
#include "Cinder-VNM/include/StateMachine.h"

#include "OSC/src/cinder/osc/Osc.h"

#include "cinder/ImageIo.h"
#include "cinder/Utilities.h"
#include "cinder/Timeline.h"

#include "CinderOpenCV.h"

#include "ciAnimatedGif.h"

namespace stb
{
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
}

#ifdef _DEBUG
#pragma comment(lib, "libpxc_d.lib")
#else
#pragma comment(lib, "libpxc.lib")
#endif

using namespace ci;
using namespace ci::app;

using namespace cv;
using namespace std;

struct FaceDetector
{
    void setup()
    {
        mFaceCascade.load(getAssetPath("haarcascade_frontalface_alt.xml").string());
    }

    void updateFaces(const Surface& cameraImage)
    {
        const int calcScale = 2; // calculate the image at half scale

        // create a grayscale copy of the input image
        cv::Mat grayCameraImage(toOcv(cameraImage, CV_8UC1));

        // scale it to half size, as dictated by the calcScale constant
        int scaledWidth = cameraImage.getWidth() / calcScale;
        int scaledHeight = cameraImage.getHeight() / calcScale;
        cv::Mat smallImg(scaledHeight, scaledWidth, CV_8UC1);
        cv::resize(grayCameraImage, smallImg, smallImg.size(), 0, 0, cv::INTER_LINEAR);

        // equalize the histogram
        cv::equalizeHist(smallImg, smallImg);

        // clear out the previously deteced faces & eyes
        mFaces.clear();

        // detect the faces and iterate them, appending them to mFaces
        vector<cv::Rect> faces;
        mFaceCascade.detectMultiScale(smallImg, faces);
        for (auto faceIter = faces.begin(); faceIter != faces.end(); ++faceIter) {
            Rectf faceRect(fromOcv(*faceIter));
            faceRect *= calcScale;
            mFaces.push_back(faceRect);
        }
    }

    cv::CascadeClassifier mFaceCascade;
    vector<Rectf> mFaces;
};

class FaceAndColorApp;
FaceAndColorApp* theApp;
#define SAFE_RELEASE(ptr) if (ptr) ptr->Release(); ptr = nullptr;

struct RealSenseDevice
{
    PXCSenseManager* pSenseManager = nullptr;

    PXCFaceModule* pFace = nullptr;
    PXCRectI32 faceRect;

    PXCFaceData* pFaceData = nullptr;
    PXCFaceConfiguration* pFaceConfig = nullptr;

    PXCCapture::Device::MirrorMode prevMirrorMode;
    PXCCapture::Device* device = nullptr;

    cv::Mat4b frameBGRA;
    Surface surface;
    FaceDetector mFaceDetector;

    bool setup()
    {
        if (FAKE_FACE_MODULE)
        {
            mFaceDetector.setup();
        }
        if (FAKE_DEVICE_MODE)
        {
            surface = *am::surface("face.tga");
            frameBGRA = toOcv(surface);
            return true;
        }

        // Create a pipeline construct
        pSenseManager = PXCSenseManager::CreateInstance();
        if (!pSenseManager)
        {
            wprintf_s(L"Error: SenseManager is unavailable/n");
            return false;
        }

        pxcStatus result;

        // Enable face module
        if (!FAKE_FACE_MODULE)
        {
            //FakeActivate<PXCFaceModule>(pSenseManager);
            result = pSenseManager->EnableFace();
            pFace = pSenseManager->QueryFace();
            if (!pFace)
            {
                CI_LOG_E("pSenseManager->QueryFace() fails.");
                return false;
            }
            pFaceData = pFace->CreateOutput();
            if (!pFaceData)
            {
                CI_LOG_E("pFace->CreateOutput() fails.");
                return false;
            }
            pFaceConfig = pFace->CreateActiveConfiguration();
            if (!pFaceConfig)
            {
                CI_LOG_E("pFace->CreateActiveConfiguration() fails.");
                return false;
            }
            pFaceConfig->detection.isEnabled = true;
            pFaceConfig->detection.maxTrackedFaces = 1;
            pFaceConfig->pose.isEnabled = false;
            pFaceConfig->landmarks.isEnabled = false;
            pFaceConfig->ApplyChanges();
        }

        // Initialize and report the resulting stream source and profile to stdout
        result = pSenseManager->Init();
        if (result < PXC_STATUS_NO_ERROR)
        {
            if (result == PXC_STATUS_ITEM_UNAVAILABLE)
            {
                CI_LOG_E("pSenseManager->Init() fails: STATUS_ITEM_UNAVAILABLE.");
            }
            else
            {
                CI_LOG_E("pSenseManager->Init() fails.");
            }
            return false;
        }
#if 0
        {
            DeviceInfo device_info;
            pSenseManager->QueryCaptureManager()->QueryCapture()->QueryDeviceInfo(0, &device_info);
            wprintf_s(L"Streaming from %s/nFirmware: %d.%d.%d.%d/n",
                device_info.name,
                device_info.firmware[0], device_info.firmware[1],
                device_info.firmware[2], device_info.firmware[3]);
        }
#endif

        // Get pointer to active device
        device = pSenseManager->QueryCaptureManager()->QueryDevice();
        if (!device)
        {
            CI_LOG_E("QueryDevice() fails.");
            return false;
        }

#if 0
        // Report the resulting profile
        {
            StreamProfileSet active_profile;
            result = device->QueryStreamProfileSet(&active_profile);
            if (result < STATUS_NO_ERROR)
            {
                CI_LOG_E("QueryStreamProfileSet() fails.");
                return false;
            }
            else // Report the profiles to stdout
            {
                wprintf_s(L"Color: %dx%dx%0.f /nDepth: %dx%dx%0.f /n",
                    active_profile.color.imageInfo.width, active_profile.color.imageInfo.height,
                    active_profile.color.frameRate.max,
                    active_profile.depth.imageInfo.width, active_profile.depth.imageInfo.height,
                    active_profile.depth.frameRate.max);
            }
        }
#endif

        // Mirror mode
        prevMirrorMode = device->QueryMirrorMode();
        device->SetMirrorMode(PXCCapture::Device::MirrorMode::MIRROR_MODE_HORIZONTAL);

        return true;
    }

    void update();
    void writeImage();

    virtual ~RealSenseDevice()
    {
        if (FAKE_DEVICE_MODE)
        {
            return;
        }

        if (device)
        {
            SAFE_RELEASE(pFaceConfig);
            SAFE_RELEASE(pFaceData);

            // Restore the mirror mode
            device->SetMirrorMode(prevMirrorMode);
            SAFE_RELEASE(pSenseManager);
        }
    }
};

const int kColorCount = 5;
Color8u palette[kColorCount];

void RealSenseDevice::writeImage()
{
    const int32_t height = frameBGRA.rows;
    const int32_t width = frameBGRA.cols;

    faceRect.x -= EXTRA_FACE_PIXELS;
    faceRect.y -= EXTRA_FACE_PIXELS;
    faceRect.w += EXTRA_FACE_PIXELS * 2;
    faceRect.h += EXTRA_FACE_PIXELS * 2;
    if (faceRect.x < 0) faceRect.x = 0;
    if (faceRect.y < 0) faceRect.y = 0;
    if (faceRect.x + faceRect.w > width - 1) faceRect.w = width - 1 - faceRect.x;
    if (faceRect.y + faceRect.h > height - 1) faceRect.h = height - 1 - faceRect.y;

    cv::Mat4b frameRGBA = frameBGRA.clone();
    cv::cvtColor(frameBGRA, frameRGBA, CV_BGRA2RGBA);
    cv::Mat4b faceOnly = frameRGBA({ faceRect.x, faceRect.y, faceRect.w, faceRect.h }).clone();
    stb::stbi_write_tga(FACE_FILENAME.c_str(), faceRect.w, faceRect.h, 4, faceOnly.data);
    stb::stbi_write_tga(IMAGE_FILENAME.c_str(), width, height, 4, frameRGBA.data);
    vector<Vec3f> colorSamples;

    cv::Mat4b blurred;
#if 0
    cv::medianBlur(frameBGRA(cv::Rect(0, height*FACE_RATIO, width, height*(1 - FACE_RATIO))), blurred, 3);
#else
    cv::medianBlur(frameBGRA, blurred, 3);
#endif

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
        {
            if (x < faceRect.x || x > faceRect.x + faceRect.w) continue;
            if (y < faceRect.y + faceRect.h) continue;
            auto& pixel = blurred(y, x);
            //if (pixel[2] > 0)
            {
                colorSamples.push_back(Vec3f(pixel[0], pixel[1], pixel[2]));
            }
        }

    // call kmeans	
    cv::Mat clusters;
    {
        cv::Mat labels;
        if (colorSamples.size() > kColorCount)
        {
            cv::kmeans(colorSamples, kColorCount, labels, cv::TermCriteria(cv::TermCriteria::COUNT, 8, 0), 2, cv::KMEANS_PP_CENTERS, clusters);
            struct ClusterVote
            {

                int cluster;
                int vote;
            };
            vector<ClusterVote> clusterVotes;
            for (int i = 0; i < kColorCount; i++)
            {
                clusterVotes.push_back({ i, 0 });
            }
            for (auto& label : cv::Mat1i(labels))
            {
                clusterVotes[label].vote++;
            }
            sort(clusterVotes.begin(), clusterVotes.end(), [](ClusterVote& lhs, ClusterVote& rhs) -> bool {
                return lhs.vote > rhs.vote;
            });

            FILE* fp = fopen(COLOR_FILENAME.c_str(), "w");
            if (fp)
            {
                for (int i = 0; i < kColorCount; ++i)
                {
                    int cluster = clusterVotes[i].cluster;
                    palette[i] = Color8u((int)clusters.at<cv::Vec3f>(cluster, 0)[2], (int)clusters.at<cv::Vec3f>(cluster, 0)[1], (int)clusters.at<cv::Vec3f>(cluster, 0)[0]);
                    fprintf(fp, "%d %d %d\n", palette[i].r, palette[i].g, palette[i].b);
                }
                fclose(fp);
            }
        }
    }
}

struct StateIdle : public State<FaceAndColorApp>
{
    GET_SINGLETON_IMPL(StateIdle);

    void enter(FaceAndColorApp* app);
    //void update(FaceAndColorApp* app);
    //void draw(FaceAndColorApp* app);
    //void exit(FaceAndColorApp* app);
    void sendMessage(FaceAndColorApp* app, const string& msg);
};

struct StateBeforeCapture : public State<FaceAndColorApp>
{
    GET_SINGLETON_IMPL(StateBeforeCapture);

    void enter(FaceAndColorApp* app);
    void update(FaceAndColorApp* app);
    //void draw(FaceAndColorApp* app);
    void exit(FaceAndColorApp* app);
    void sendMessage(FaceAndColorApp* app, const string& msg);
};

struct StateCapture : public State<FaceAndColorApp>
{
    GET_SINGLETON_IMPL(StateCapture);

    void enter(FaceAndColorApp* app);
    void update(FaceAndColorApp* app);
    //void draw(FaceAndColorApp* app);
    //void exit(FaceAndColorApp* app);
    void sendMessage(FaceAndColorApp* app, const string& msg);
};

struct FaceAndColorApp : public App, StateMachine<FaceAndColorApp>
{
public:
    void setup() override
    {
        theApp = (FaceAndColorApp*)get();
        setWindowPos(0, 0);

        log::makeLogger<log::LoggerFile>();

        mGif = ciAnimatedGif::create(loadAsset("gif.gif"));

        setOwner(this);

        createConfigUI({ 300, 250 });
        gl::disableDepthRead();
        gl::enableAlphaBlending();

        if (!mDevice.setup())
        {
            CI_LOG_E("Faile to setup RealSense device.");
            quit();
            return;
        }

        mOscSender = make_shared<osc::SenderUdp>(10000, OSC_IP, OSC_SEND_PORT);
        mOscSender->bind();

        mOscReceiver = make_shared<osc::ReceiverUdp>(OSC_RECV_PORT);
        mOscReceiver->setListener(OSC_RECV_MSG, [&](const osc::Message &msg){
            sendMessage(msg.getAddress());
        });

        try {
            // Bind the receiver to the endpoint. This function may throw.
            mOscReceiver->bind();
        }
        catch (const osc::Exception &ex) {
            CI_LOG_E("Error binding: " << ex.what() << " val: " << ex.value());
            quit();
        }
        // UDP opens the socket and "listens" accepting any message from any endpoint. The listen
        // function takes an error handler for the underlying socket. Any errors that would
        // call this function are because of problems with the socket or with the remote message.
        mOscReceiver->listen(
            [](asio::error_code error, asio::ip::udp::endpoint endpoint) -> bool {
            if (error) {
                CI_LOG_E("Error Listening: " << error.message() << " val: " << error.value() << " endpoint: " << endpoint);
                return false;
            }
            else
                return true;
        });

        changeToState(StateIdle::get());

        getWindow()->getSignalKeyUp().connect([&](KeyEvent& event) {
            if (event.getCode() == KeyEvent::KEY_ESCAPE)
            {
                quit();
                return;
            }
            if (event.getCode() == KeyEvent::KEY_SPACE)
            {
                theApp->dispatchAsync([&] {
                    theApp->changeToState(StateIdle::get());
                });
            }
        });

        getSignalUpdate().connect([&] {
            _FPS = getAverageFps();
            mDevice.update();
            updateFSM();
            updateTexture(mTexture, mDevice.surface);
        });

        getWindow()->getSignalDraw().connect([&] {
            gl::clear();
            gl::draw(mTexture, getWindowBounds());

            const auto kRadius = 20;
            for (int i = 0; i < kColorCount; i++)
            {
                gl::color(palette[i]);
                gl::drawSolidCircle({ 300 + i * kRadius * 2, 20 }, kRadius);
            }

#if 0
            gl::color(ColorA(1, 1, 1, 0.03f));
            gl::drawSolidRect({ 0.0f, FACE_RATIO * getWindowHeight(), (float)getWindowWidth(), (float)getWindowHeight() });
#endif
            gl::color(ColorA::black());
            gl::drawSolidRect({ 0, 0, 128, 16 });

            gl::color(mTintColor);

            if (mFullWhite)
            {
                gl::drawSolidRect({ 0, 0, 128, 16 });
            }
            else
            {
                mGif->draw();
            }
            gl::color(ColorA::white());

            drawFSM();
        });
    }

    RealSenseDevice mDevice;
    gl::TextureRef mTexture;

    shared_ptr<osc::SenderUdp> mOscSender;
    shared_ptr<osc::ReceiverUdp> mOscReceiver;
    float mStateStartSeconds;
    
    ciAnimatedGifRef mGif;
    bool mFullWhite = false;
    Anim<ColorA> mTintColor;
    Anim<float> mPaletteIdx;
};

void StateIdle::enter(FaceAndColorApp*)
{
    _APP_STATUS = "Idle";
    theApp->timeline().apply(&theApp->mTintColor, ColorA::black(), 1.0f);
}

void StateIdle::sendMessage(FaceAndColorApp*, const string& msg)
{
    if (msg == "user-enter")
    {
        theApp->dispatchAsync([] {
            theApp->changeToState(StateBeforeCapture::get());
        });
    }
}

void StateBeforeCapture::enter(FaceAndColorApp*)
{
    _APP_STATUS = "Before Capture";
    theApp->mStateStartSeconds = getElapsedSeconds();
    auto fn = []{theApp->mFullWhite = true; };
    theApp->timeline()
        .apply(&theApp->mTintColor, ColorA(1, 1, 1, 1.0f), COUNTDOWN_SECONDS - FULL_WHITE_SECONDS)
        .finishFn(fn)
    ;
}

void StateBeforeCapture::exit(FaceAndColorApp*)
{
    theApp->mFullWhite = false;
}

void StateBeforeCapture::update(FaceAndColorApp*)
{
    if (getElapsedSeconds() - theApp->mStateStartSeconds > COUNTDOWN_SECONDS)
    {
        theApp->dispatchAsync([]{
            theApp->changeToState(StateCapture::get());
        });
    }
}

void StateBeforeCapture::sendMessage(FaceAndColorApp*, const string& msg)
{
    if (msg == "user-leave")
    {
        theApp->dispatchAsync([&] {
            theApp->changeToState(StateIdle::get());
        });
    }
}

// TODO: move to FaceAndColorApp
static int idx;

void StateCapture::enter(FaceAndColorApp*)
{
    idx = -1;

    _APP_STATUS = "Capture";
    theApp->mDevice.writeImage();

    osc::Message msg(OSC_SEND_MSG);
    theApp->mOscSender->send(msg);

    theApp->mStateStartSeconds = getElapsedSeconds();
}

void StateCapture::update(FaceAndColorApp*)
{
    float elapsed = getElapsedSeconds() - theApp->mStateStartSeconds;
    int intElapsed = elapsed;
    if (intElapsed > idx)
    {
        idx = intElapsed;
#if 1
        theApp->timeline().apply(&theApp->mTintColor, ColorA(palette[idx % FLY_COLOR_COUNT]), 1.0f);
#else
        theApp->mTintColor = ColorA(palette[idx]);
#endif
    }
    if (elapsed > RECV_TIMEOUT_SECONDS)
    {
        theApp->dispatchAsync([&] {
            theApp->changeToState(StateIdle::get());
        });
    }
}

void StateCapture::sendMessage(FaceAndColorApp*, const string& msg)
{
    if (msg == OSC_RECV_MSG)
    {
        theApp->dispatchAsync([&] {
            theApp->changeToState(StateIdle::get());
        });
    }
}

void RealSenseDevice::update()
{
    if (!FAKE_DEVICE_MODE)
    {
        // Get the segmented image from the Seg3D video module
        pxcStatus result = pSenseManager->AcquireFrame(true);

        if (result < PXC_STATUS_NO_ERROR)
        {
            CI_LOG_E("Error: AcquireFrame failed " << result);
            return;
        }

        PXCCapture::Sample *sample = pSenseManager->QuerySample();
        PXCImage* segmented_image = sample->color;

        if (!segmented_image)
        {
            CI_LOG_E("Error: The segmentation image was not returned" << result);
            pSenseManager->ReleaseFrame();
            return;
        }
        // Iterate over the pixels in the image
        PXCImage::ImageData segmented_image_data;
        segmented_image->AcquireAccess(PXCImage::ACCESS_READ, PXCImage::PIXEL_FORMAT_RGB32, &segmented_image_data);
        const int32_t height = segmented_image->QueryInfo().height;
        const int32_t width = segmented_image->QueryInfo().width;

        frameBGRA = Mat4b(height, width, (Vec4b*)segmented_image_data.planes[0]);
        surface = ci::Surface(segmented_image_data.planes[0], width, height, segmented_image_data.pitches[0], SurfaceChannelOrder::BGRA);
        segmented_image->ReleaseAccess(&segmented_image_data);
        //SAFE_RELEASE(segmented_image);

        if (!FAKE_FACE_MODULE)
        {
            pFaceData->Update();
            if (pFaceData->QueryNumberOfDetectedFaces() == 1)
            {
                auto face = pFaceData->QueryFaceByIndex(0);
                if (!face)
                {
                    CI_LOG_E("QueryFaceByIndex() is NULL");
                }
                auto detection = face->QueryDetection();
                if (detection && detection->QueryBoundingRect(&faceRect))
                {
                    _FACE_STATUS = "Detected";
                    theApp->sendMessage("user-enter");
                }
            }
            else
            {
                _FACE_STATUS = "No";
                theApp->sendMessage("user-leave");
            }
        }

        pSenseManager->ReleaseFrame();
    }

    if (FAKE_FACE_MODULE)
    {
        mFaceDetector.updateFaces(surface);
        if (mFaceDetector.mFaces.empty())
        {
            _FACE_STATUS = "No";
            theApp->sendMessage("user-leave");
        }
        else
        {
            auto face = mFaceDetector.mFaces[0];
            faceRect = { (pxcI32)face.x1, (pxcI32)face.y1, (pxcI32)face.getWidth(), (pxcI32)face.getHeight() };
            _FACE_STATUS = "Detected";
            theApp->sendMessage("user-enter");
        }
    }
}

CINDER_APP(FaceAndColorApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    if (FAKE_DEVICE_MODE)
    {
        FAKE_FACE_MODULE = true;
    }

    settings->setBorderless();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
})

