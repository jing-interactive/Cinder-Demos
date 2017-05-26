#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

#include "cinder/CameraUi.h"
#include "cinder/Log.h"

#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_math_stream_namespace.h>

#include "sutil/sutil.h"
#include "commonStructs.h"
#include "random.h"

#include "AssetManager.h"
#include "MiniConfig.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace optix;

#pragma comment(lib, "optix.1.lib")
#pragma comment(lib, "optix_prime.1.lib")
#pragma comment(lib, "optixu.1.lib")

static float rand_range(float min, float max)
{
    static unsigned int seed = 0u;
    return min + (max - min) * rnd(seed);
}

//------------------------------------------------------------------------------
//
// Globals
//
//------------------------------------------------------------------------------

Context      context;
bool         use_pbo = false;

std::string  texture_path = ci::app::getAssetDirectories()[0].string() + "/textures";
std::string  tutorial_ptx_path;
int          tutorial_number = 10;

//------------------------------------------------------------------------------
//
// Forward decls
//
//------------------------------------------------------------------------------

std::string ptxPath(const std::string& cuda_file);
optix::Buffer getOutputBuffer();
void destroyContext();
void createContext();
void createGeometry();
void setupLights();
void updateCamera(ci::Camera &cam);

//------------------------------------------------------------------------------
//
//  Helper functions
//
//------------------------------------------------------------------------------

std::string ptxPath(const std::string& cuda_file)
{
    return ci::app::getAssetDirectories()[0].string() + "/ptx/" + cuda_file + ".ptx";
}

optix::Buffer getOutputBuffer()
{
    return context["output_buffer"]->getBuffer();
}

void destroyContext()
{
    if (context) {
        context->destroy();
        context = 0;
    }
}

void createContext()
{
    // Set up context
    context = Context::create();
    context->setRayTypeCount(2);
    context->setEntryPointCount(1);
    context->setStackSize(4640);

    // Note: high max depth for reflection and refraction through glass
    context["max_depth"]->setInt(100);
    context["radiance_ray_type"]->setUint(0);
    context["shadow_ray_type"]->setUint(1);
    context["scene_epsilon"]->setFloat(1.e-4f);
    context["importance_cutoff"]->setFloat(0.01f);
    context["ambient_light_color"]->setFloat(0.31f, 0.33f, 0.28f);

    // Output buffer
    // First allocate the memory for the GL buffer, then attach it to OptiX.
    /*
    GLuint vbo = 0;
    glGenBuffers( 1, &vbo );
    glBindBuffer( GL_ARRAY_BUFFER, vbo );
    glBufferData( GL_ARRAY_BUFFER, 4 * APP_WIDTH * APP_HEIGHT, 0, GL_STREAM_DRAW);
    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    */

    optix::Buffer buffer = sutil::createOutputBuffer(context, RT_FORMAT_UNSIGNED_BYTE4, APP_WIDTH, APP_HEIGHT, use_pbo);
    context["output_buffer"]->set(buffer);


    // Ray generation program
    {
        const std::string camera_name = tutorial_number >= 11 ? "env_camera" : "pinhole_camera";
        Program ray_gen_program = context->createProgramFromPTXFile(tutorial_ptx_path, camera_name);
        context->setRayGenerationProgram(0, ray_gen_program);
    }

    // Exception program
    Program exception_program = context->createProgramFromPTXFile(tutorial_ptx_path, "exception");
    context->setExceptionProgram(0, exception_program);
    context["bad_color"]->setFloat(1.0f, 0.0f, 1.0f);

    // Miss program
    {
        const std::string miss_name = tutorial_number >= 5 ? "envmap_miss" : "miss";
        context->setMissProgram(0, context->createProgramFromPTXFile(tutorial_ptx_path, miss_name));
        const float3 default_color = make_float3(1.0f, 1.0f, 1.0f);
        const std::string texpath = texture_path + "/" + std::string("CedarCity.hdr");
        context["envmap"]->setTextureSampler(sutil::loadTexture(context, texpath, default_color));
        context["bg_color"]->setFloat(make_float3(0.34f, 0.55f, 0.85f));
    }

    // 3D solid noise buffer, 1 float channel, all entries in the range [0.0, 1.0].

    const int tex_APP_WIDTH = 64;
    const int tex_APP_HEIGHT = 64;
    const int tex_depth = 64;
    optix::Buffer noiseBuffer = context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT, tex_APP_WIDTH, tex_APP_HEIGHT, tex_depth);
    float *tex_data = (float *)noiseBuffer->map();

    // Random noise in range [0, 1]
    for (int i = tex_APP_WIDTH * tex_APP_HEIGHT * tex_depth; i > 0; i--) {
        // One channel 3D noise in [0.0, 1.0] range.
        *tex_data++ = rand_range(0.0f, 1.0f);
    }
    noiseBuffer->unmap();


    // Noise texture sampler
    TextureSampler noiseSampler = context->createTextureSampler();

    noiseSampler->setWrapMode(0, RT_WRAP_REPEAT);
    noiseSampler->setWrapMode(1, RT_WRAP_REPEAT);
    noiseSampler->setFilteringModes(RT_FILTER_LINEAR, RT_FILTER_LINEAR, RT_FILTER_NONE);
    noiseSampler->setIndexingMode(RT_TEXTURE_INDEX_NORMALIZED_COORDINATES);
    noiseSampler->setReadMode(RT_TEXTURE_READ_NORMALIZED_FLOAT);
    noiseSampler->setMaxAnisotropy(1.0f);
    noiseSampler->setMipLevelCount(1);
    noiseSampler->setArraySize(1);
    noiseSampler->setBuffer(0, 0, noiseBuffer);

    context["noise_texture"]->setTextureSampler(noiseSampler);
}

float4 make_plane(float3 n, float3 p)
{
    n = normalize(n);
    float d = -dot(n, p);
    return make_float4(n, d);
}

void createGeometry()
{
    const std::string box_ptx(ptxPath("box.cu"));
    Program box_bounds = context->createProgramFromPTXFile(box_ptx, "box_bounds");
    Program box_intersect = context->createProgramFromPTXFile(box_ptx, "box_intersect");

    // Create box
    Geometry box = context->createGeometry();
    box->setPrimitiveCount(1u);
    box->setBoundingBoxProgram(box_bounds);
    box->setIntersectionProgram(box_intersect);
    box["boxmin"]->setFloat(-2.0f, 0.0f, -2.0f);
    box["boxmax"]->setFloat(2.0f, 7.0f, 2.0f);

    // Create chull
    Geometry chull = 0;
    if (tutorial_number >= 9){
        chull = context->createGeometry();
        chull->setPrimitiveCount(1u);
        chull->setBoundingBoxProgram(context->createProgramFromPTXFile(tutorial_ptx_path, "chull_bounds"));
        chull->setIntersectionProgram(context->createProgramFromPTXFile(tutorial_ptx_path, "chull_intersect"));
        optix::Buffer plane_buffer = context->createBuffer(RT_BUFFER_INPUT);
        plane_buffer->setFormat(RT_FORMAT_FLOAT4);
        int nsides = 6;
        plane_buffer->setSize(nsides + 2);
        float4* chplane = (float4*)plane_buffer->map();
        float radius = 1;
        float3 xlate = make_float3(-1.4f, 0, -3.7f);

        for (int i = 0; i < nsides; i++){
            float angle = float(i) / float(nsides) * M_PIf * 2.0f;
            float x = cos(angle);
            float y = sin(angle);
            chplane[i] = make_plane(make_float3(x, 0, y), make_float3(x*radius, 0, y*radius) + xlate);
        }
        float min = 0.02f;
        float max = 3.5f;
        chplane[nsides + 0] = make_plane(make_float3(0, -1, 0), make_float3(0, min, 0) + xlate);
        float angle = 5.f / nsides * M_PIf * 2;
        chplane[nsides + 1] = make_plane(make_float3(cos(angle), .7f, sin(angle)), make_float3(0, max, 0) + xlate);
        plane_buffer->unmap();
        chull["planes"]->setBuffer(plane_buffer);
        chull["chull_bbmin"]->setFloat(-radius + xlate.x, min + xlate.y, -radius + xlate.z);
        chull["chull_bbmax"]->setFloat(radius + xlate.x, max + xlate.y, radius + xlate.z);
    }

    // Floor geometry
    const std::string floor_ptx(ptxPath("parallelogram.cu"));
    Geometry parallelogram = context->createGeometry();
    parallelogram->setPrimitiveCount(1u);
    parallelogram->setBoundingBoxProgram(context->createProgramFromPTXFile(floor_ptx, "bounds"));
    parallelogram->setIntersectionProgram(context->createProgramFromPTXFile(floor_ptx, "intersect"));
    float3 anchor = make_float3(-64.0f, 0.01f, -64.0f);
    float3 v1 = make_float3(128.0f, 0.0f, 0.0f);
    float3 v2 = make_float3(0.0f, 0.0f, 128.0f);
    float3 normal = cross(v2, v1);
    normal = normalize(normal);
    float d = dot(normal, anchor);
    v1 *= 1.0f / dot(v1, v1);
    v2 *= 1.0f / dot(v2, v2);
    float4 plane = make_float4(normal, d);
    parallelogram["plane"]->setFloat(plane);
    parallelogram["v1"]->setFloat(v1);
    parallelogram["v2"]->setFloat(v2);
    parallelogram["anchor"]->setFloat(anchor);

    // Materials
    std::string box_chname;
    if (tutorial_number >= 8){
        box_chname = "box_closest_hit_radiance";
    }
    else if (tutorial_number >= 3){
        box_chname = "closest_hit_radiance3";
    }
    else if (tutorial_number >= 2){
        box_chname = "closest_hit_radiance2";
    }
    else if (tutorial_number >= 1){
        box_chname = "closest_hit_radiance1";
    }
    else {
        box_chname = "closest_hit_radiance0";
    }

    Material box_matl = context->createMaterial();
    Program box_ch = context->createProgramFromPTXFile(tutorial_ptx_path, box_chname);
    box_matl->setClosestHitProgram(0, box_ch);
    if (tutorial_number >= 3) {
        Program box_ah = context->createProgramFromPTXFile(tutorial_ptx_path, "any_hit_shadow");
        box_matl->setAnyHitProgram(1, box_ah);
    }
    box_matl["Ka"]->setFloat(0.3f, 0.3f, 0.3f);
    box_matl["Kd"]->setFloat(0.6f, 0.7f, 0.8f);
    box_matl["Ks"]->setFloat(0.8f, 0.9f, 0.8f);
    box_matl["phong_exp"]->setFloat(88);
    box_matl["reflectivity_n"]->setFloat(0.2f, 0.2f, 0.2f);

    std::string floor_chname;
    if (tutorial_number >= 7){
        floor_chname = "floor_closest_hit_radiance";
    }
    else if (tutorial_number >= 6){
        floor_chname = "floor_closest_hit_radiance5";
    }
    else if (tutorial_number >= 4){
        floor_chname = "floor_closest_hit_radiance4";
    }
    else if (tutorial_number >= 3){
        floor_chname = "closest_hit_radiance3";
    }
    else if (tutorial_number >= 2){
        floor_chname = "closest_hit_radiance2";
    }
    else if (tutorial_number >= 1){
        floor_chname = "closest_hit_radiance1";
    }
    else {
        floor_chname = "closest_hit_radiance0";
    }

    Material floor_matl = context->createMaterial();
    Program floor_ch = context->createProgramFromPTXFile(tutorial_ptx_path, floor_chname);
    floor_matl->setClosestHitProgram(0, floor_ch);
    if (tutorial_number >= 3) {
        Program floor_ah = context->createProgramFromPTXFile(tutorial_ptx_path, "any_hit_shadow");
        floor_matl->setAnyHitProgram(1, floor_ah);
    }
    floor_matl["Ka"]->setFloat(0.3f, 0.3f, 0.1f);
    floor_matl["Kd"]->setFloat(194 / 255.f*.6f, 186 / 255.f*.6f, 151 / 255.f*.6f);
    floor_matl["Ks"]->setFloat(0.4f, 0.4f, 0.4f);
    floor_matl["reflectivity"]->setFloat(0.1f, 0.1f, 0.1f);
    floor_matl["reflectivity_n"]->setFloat(0.05f, 0.05f, 0.05f);
    floor_matl["phong_exp"]->setFloat(88);
    floor_matl["tile_v0"]->setFloat(0.25f, 0, .15f);
    floor_matl["tile_v1"]->setFloat(-.15f, 0, 0.25f);
    floor_matl["crack_color"]->setFloat(0.1f, 0.1f, 0.1f);
    floor_matl["crack_APP_WIDTH"]->setFloat(0.02f);

    // Glass material
    Material glass_matl;
    if (chull.get()) {
        Program glass_ch = context->createProgramFromPTXFile(tutorial_ptx_path, "glass_closest_hit_radiance");
        const std::string glass_ahname = tutorial_number >= 10 ? "glass_any_hit_shadow" : "any_hit_shadow";
        Program glass_ah = context->createProgramFromPTXFile(tutorial_ptx_path, glass_ahname);
        glass_matl = context->createMaterial();
        glass_matl->setClosestHitProgram(0, glass_ch);
        glass_matl->setAnyHitProgram(1, glass_ah);

        glass_matl["importance_cutoff"]->setFloat(1e-2f);
        glass_matl["cutoff_color"]->setFloat(0.34f, 0.55f, 0.85f);
        glass_matl["fresnel_exponent"]->setFloat(3.0f);
        glass_matl["fresnel_minimum"]->setFloat(0.1f);
        glass_matl["fresnel_maximum"]->setFloat(1.0f);
        glass_matl["refraction_index"]->setFloat(1.4f);
        glass_matl["refraction_color"]->setFloat(1.0f, 1.0f, 1.0f);
        glass_matl["reflection_color"]->setFloat(1.0f, 1.0f, 1.0f);
        glass_matl["refraction_maxdepth"]->setInt(100);
        glass_matl["reflection_maxdepth"]->setInt(100);
        float3 extinction = make_float3(.80f, .89f, .75f);
        glass_matl["extinction_constant"]->setFloat(::log(extinction.x), ::log(extinction.y), ::log(extinction.z));
        glass_matl["shadow_attenuation"]->setFloat(0.4f, 0.7f, 0.4f);
    }

    // Create GIs for each piece of geometry
    std::vector<GeometryInstance> gis;
    gis.push_back(context->createGeometryInstance(box, &box_matl, &box_matl + 1));
    gis.push_back(context->createGeometryInstance(parallelogram, &floor_matl, &floor_matl + 1));
    if (chull.get())
        gis.push_back(context->createGeometryInstance(chull, &glass_matl, &glass_matl + 1));

    // Place all in group
    GeometryGroup geometrygroup = context->createGeometryGroup();
    geometrygroup->setChildCount(static_cast<unsigned int>(gis.size()));
    geometrygroup->setChild(0, gis[0]);
    geometrygroup->setChild(1, gis[1]);
    if (chull.get()) {
        geometrygroup->setChild(2, gis[2]);
    }
    geometrygroup->setAcceleration(context->createAcceleration("NoAccel"));

    context["top_object"]->set(geometrygroup);
    context["top_shadower"]->set(geometrygroup);

}

void setupLights()
{
    BasicLight lights[] = {
        { make_float3(-5.0f, 60.0f, -16.0f), make_float3(1.0f, 1.0f, 1.0f), 1 }
    };

    optix::Buffer light_buffer = context->createBuffer(RT_BUFFER_INPUT);
    light_buffer->setFormat(RT_FORMAT_USER);
    light_buffer->setElementSize(sizeof(BasicLight));
    light_buffer->setSize(sizeof(lights) / sizeof(lights[0]));
    memcpy(light_buffer->map(), lights, sizeof(lights));
    light_buffer->unmap();

    context["lights"]->set(light_buffer);
}

void updateCamera(ci::Camera &cam)
{
    const float vfov = cam.getFov();
    const float aspect_ratio = cam.getAspectRatio();

    vec3 camEye = cam.getEyePoint();
    float3 camera_eye = make_float3(camEye.x, camEye.y, camEye.z);

    vec3 camLookat = cam.getEyePoint() + cam.getViewDirection() * 10.0f;
    float3 camera_lookat = make_float3(camLookat.x, camLookat.y, camLookat.z);

    float3 camera_u, camera_v, camera_w;
    sutil::calculateCameraVariables(
        camera_eye, camera_lookat, make_float3(0, 1, 0), vfov, aspect_ratio,
        camera_u, camera_v, camera_w, true);

    context["eye"]->setFloat(camera_eye);
    context["U"]->setFloat(camera_u);
    context["V"]->setFloat(camera_v);
    context["W"]->setFloat(camera_w);
}

///
/// TODO:
/// move to class methods
/// PBO
///

class CinderOptiXApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void resize() override;

private:
    gl::TextureRef mTexture;
    CameraPersp mCam;
    CameraUi mCamUi;
};

//-----------------------------------------------------------------------------
//
//  tutorial
//
//-----------------------------------------------------------------------------

// 0 - normal shader
// 1 - lambertian
// 2 - specular
// 3 - shadows
// 4 - reflections
// 5 - miss
// 6 - schlick
// 7 - procedural texture on floor
// 8 - LGRustyMetal
// 9 - intersection
// 10 - anyhit
// 11 - camera

void handleError2(RTcontext context, RTresult code, const char* file,
    int line)
{
    const char* message;
    char s[2048];
    rtContextGetErrorString(context, code, &message);
    sprintf(s, "%s\n(%s:%d)", message, file, line);
    CI_LOG_E(s);
}

void CinderOptiXApp::setup() {
    log::makeLogger<log::LoggerFile>();

    createConfigUI({ 200, 200 });

    gl::enableVerticalSync(false);

    tutorial_number = 1; // 0-11

    // set up path to ptx file associated with tutorial number
    std::stringstream ss;
    ss << "tutorial" << tutorial_number << ".cu";
    tutorial_ptx_path = ptxPath(ss.str());

    auto format = gl::Texture::Format().dataType(GL_UNSIGNED_BYTE);
    mTexture = gl::Texture::create(nullptr, GL_BGRA, APP_WIDTH, APP_HEIGHT, format);

    mCam.setPerspective(60, getWindowAspectRatio(), 0.1f, 1000.0f);
    mCam.lookAt(vec3(7.0f, 9.2f, -6.0f), vec3(0.0f, 4.0f, 0.0f));
    mCamUi.setCamera(&mCam);
    mCamUi.connect(getWindow());

    try {
        createContext();
        createGeometry();
        setupLights();

        context->validate();
        // destroyContext();
    }
    catch (sutil::APIError& e) {
        handleError2(context->get(), e.code, e.file.c_str(), e.line);
    }
    catch (std::exception& e) {
        CI_LOG_EXCEPTION("", e);
    }
}

void CinderOptiXApp::resize() {
    APP_WIDTH = getWindowWidth();
    APP_HEIGHT = getWindowHeight();
    sutil::resizeBuffer(getOutputBuffer(), APP_WIDTH, APP_HEIGHT);
    auto format = gl::Texture::Format().dataType(GL_UNSIGNED_BYTE);
    mTexture = gl::Texture::create(nullptr, GL_BGRA, APP_WIDTH, APP_HEIGHT, format);

    mCam.setPerspective(60, getWindowAspectRatio(), 0.1f, 1000.0f);
}

void CinderOptiXApp::update() {
    getWindow()->setTitle(to_string((int)getAverageFps()));

    updateCamera(mCam);

    context->launch(0, APP_WIDTH, APP_HEIGHT);

    optix::Buffer buffer = context["output_buffer"]->getBuffer();
    const unsigned char *hostBuffer = (const unsigned char *)buffer->map();
    mTexture->update(hostBuffer, GL_BGRA, GL_UNSIGNED_BYTE, 0, APP_WIDTH, APP_HEIGHT);
    buffer->unmap();
}

void CinderOptiXApp::draw() {
    gl::setMatricesWindow(getWindowSize());
    gl::clear();

    gl::draw(mTexture);
}

CINDER_APP(CinderOptiXApp, RendererGl(RendererGl::Options().msaa(0)), [](App::Settings *settings) {
    readConfig();

    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);

    settings->disableFrameRate();
    //	settings->setHighDensityDisplayEnabled();
})
