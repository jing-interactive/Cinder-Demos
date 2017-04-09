#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/CameraUi.h"
#include "cinder/Log.h"

#include "AssetManager.h"
#include "MiniConfig.h"

#include "yocto/yocto_gltf.h"
#include "nodes/Node.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace ph::nodes;

struct RootGLTF;

struct AnimationGLTF
{
    typedef shared_ptr<AnimationGLTF> Ref;
    ygltf::animation_t property;

    static Ref create(const RootGLTF& rootGLTF, const ygltf::animation_t& property)
    {
        Ref ref = make_shared<AnimationGLTF>();
        ref->property = property;
        return ref;
    }
};

struct BufferGLTF
{
    typedef shared_ptr<BufferGLTF> Ref;
    ygltf::buffer_t property;

    BufferRef buffer;

    static Ref create(const RootGLTF& rootGLTF, const ygltf::buffer_t& property);
};

struct BufferViewGLTF
{
    typedef shared_ptr<BufferViewGLTF> Ref;
    ygltf::bufferView_t property;

    BufferGLTF::Ref buffer;

    static Ref create(const RootGLTF& rootGLTF, const ygltf::bufferView_t& property);
};

struct AccessorGLTF
{
    typedef shared_ptr<AccessorGLTF> Ref;
    ygltf::accessor_t property;

    BufferViewGLTF::Ref bufferView;

    static Ref create(const RootGLTF& rootGLTF, const ygltf::accessor_t& property);
};


struct CameraGLTF
{
    typedef shared_ptr<CameraGLTF> Ref;
    ygltf::camera_t property;

    unique_ptr<Camera> camera;

    static Ref create(const RootGLTF& rootGLTF, const ygltf::camera_t& property)
    {
        Ref ref = make_shared<CameraGLTF>();
        ref->property = property;
        if (property.type == ygltf::camera_t::type_t::perspective_t)
            ref->camera = make_unique<CameraPersp>();
        else
            ref->camera = make_unique<CameraOrtho>();
        return ref;
    }
};

struct ImageGLTF
{
    typedef shared_ptr<ImageGLTF> Ref;
    ygltf::image_t property;

    BufferViewGLTF::Ref bufferView;
    SurfaceRef surface;

    static Ref create(const RootGLTF& rootGLTF, const ygltf::image_t& property);
};

struct SamplerGLTF
{
    typedef shared_ptr<SamplerGLTF> Ref;
    ygltf::sampler_t property;

    // TODO: support texture1d / 3d
    gl::Texture2d::Format format;

    static Ref create(const RootGLTF& rootGLTF, const ygltf::sampler_t& property)
    {
        Ref ref = make_shared<SamplerGLTF>();
        ref->property = property;

        ref->format.minFilter((GLenum)property.minFilter);
        ref->format.magFilter((GLenum)property.magFilter);
        ref->format.wrapS((GLenum)property.wrapS);
        ref->format.wrapT((GLenum)property.wrapT);

        return ref;
    }
};

struct TextureGLTF
{
    typedef shared_ptr<TextureGLTF> Ref;
    ygltf::texture_t property;

    SamplerGLTF::Ref sampler;
    ImageGLTF::Ref source;

    static Ref create(const RootGLTF& rootGLTF, const ygltf::texture_t& property);
};

struct MaterialGLTF
{
    typedef shared_ptr<MaterialGLTF> Ref;
    ygltf::material_t property;

    gl::GlslProgRef shader;

    TextureGLTF::Ref emissiveTexture;
    TextureGLTF::Ref normalTexture;
    TextureGLTF::Ref occlusionTexture;

    // PBR
    TextureGLTF::Ref baseColorTexture;
    TextureGLTF::Ref metallicRoughnessTexture;

    static Ref create(const RootGLTF& rootGLTF, const ygltf::material_t& property);
};

struct MeshPrimitiveGLTF
{
    typedef shared_ptr<MeshPrimitiveGLTF> Ref;
    ygltf::mesh_primitive_t property;

    map<string, AccessorGLTF::Ref> attributes;
    AccessorGLTF::Ref indices;
    MaterialGLTF::Ref material;

    static Ref create(const RootGLTF& rootGLTF, const ygltf::mesh_primitive_t& property);
};

struct MeshGLTF
{
    typedef shared_ptr<MeshGLTF> Ref;
    ygltf::mesh_t property;

    vector<MeshPrimitiveGLTF::Ref> primitives;

    static Ref create(const RootGLTF& rootGLTF, const ygltf::mesh_t& property)
    {
        Ref ref = make_shared<MeshGLTF>();
        ref->property = property;
        for (auto& item : property.primitives) ref->primitives.emplace_back(MeshPrimitiveGLTF::create(rootGLTF, item));

        return ref;
    }
};

struct SkinGLTF
{
    typedef shared_ptr<SkinGLTF> Ref;
    ygltf::skin_t property;

    static Ref create(const RootGLTF& rootGLTF, const ygltf::skin_t& property)
    {
        Ref ref = make_shared<SkinGLTF>();
        ref->property = property;
        return ref;
    }
};

struct NodeGLTF : public Node3D
{
    typedef shared_ptr<NodeGLTF> Ref;
    ygltf::node_t property;

    CameraGLTF::Ref camera;
    MeshGLTF::Ref mesh;
    SkinGLTF::Ref skin;

    static Ref create(const RootGLTF& rootGLTF, const ygltf::node_t& property);

    void addChildren(const RootGLTF& rootGLTF);

    void update()
    {
    }

    void draw()
    {
        //if (tex && mShader)
        //{
        //    gl::ScopedTextureBind tex0(tex);
        //    mShader->uniform("tex0", 0);
        //    gl::ScopedGlslProg glsl(mShader);
        //    //gl::draw(mesh);
        //}
    }
};

struct SceneGLTF
{
    typedef shared_ptr<SceneGLTF> Ref;
    ygltf::scene_t property;

    vector<NodeGLTF::Ref> nodes; // The root nodes of a scene

    static Ref create(const RootGLTF& rootGLTF, const ygltf::scene_t& property)
    {
        Ref ref = make_shared<SceneGLTF>();
        ref->property = property;

        return ref;
    }

    void update()
    {
        for (auto node : nodes)
            node->treeUpdate();
    }

    void draw()
    {
        for (auto node : nodes)
            node->treeDraw();
    }
};

struct RootGLTF
{
    typedef shared_ptr<RootGLTF> Ref;
    ygltf::glTF_t property;

    static Ref create(const fs::path& gltfPath)
    {
        unique_ptr<ygltf::glTF_t> glTF_t;

        try
        {
            bool load_bin = true;
            bool load_shaders = true;
            bool load_img = false;
            bool skip_missing = false;

            glTF_t.reset(ygltf::load_gltf(gltfPath.string(), load_bin, load_shaders, load_img, skip_missing));
        }
        catch (exception& ex)
        {
            CI_LOG_EXCEPTION("load_gltf", ex);
        }

        Ref ref = make_shared<RootGLTF>();
        ref->property = *glTF_t;
        ref->gltfPath = gltfPath;

        for (auto& item : glTF_t->buffers) ref->buffers.emplace_back(BufferGLTF::create(*ref, item));
        for (auto& item : glTF_t->bufferViews) ref->bufferViews.emplace_back(BufferViewGLTF::create(*ref, item));
        for (auto& item : glTF_t->animations) ref->animations.emplace_back(AnimationGLTF::create(*ref, item));
        for (auto& item : glTF_t->accessors) ref->accessors.emplace_back(AccessorGLTF::create(*ref, item));

        for (auto& item : glTF_t->images) ref->images.emplace_back(ImageGLTF::create(*ref, item));
        for (auto& item : glTF_t->samplers) ref->samplers.emplace_back(SamplerGLTF::create(*ref, item));
        for (auto& item : glTF_t->textures) ref->textures.emplace_back(TextureGLTF::create(*ref, item));
        for (auto& item : glTF_t->materials) ref->materials.emplace_back(MaterialGLTF::create(*ref, item));

        for (auto& item : glTF_t->meshes) ref->meshes.emplace_back(MeshGLTF::create(*ref, item));
        for (auto& item : glTF_t->skins) ref->skins.emplace_back(SkinGLTF::create(*ref, item));
        for (auto& item : glTF_t->cameras) ref->cameras.emplace_back(CameraGLTF::create(*ref, item));

        for (auto& item : glTF_t->nodes) ref->nodes.emplace_back(NodeGLTF::create(*ref, item));
        for (auto& item : glTF_t->scenes) ref->scenes.emplace_back(SceneGLTF::create(*ref, item));

        for (auto& node : ref->nodes) node->addChildren(*ref);

        return ref;
    }

    void update()
    {
        if (activeScene) activeScene->update();
    }

    void draw()
    {
        if (activeScene) activeScene->draw();
    }

    vector<AccessorGLTF::Ref> accessors;
    vector<AnimationGLTF::Ref> animations;
    vector<BufferViewGLTF::Ref> bufferViews;
    vector<BufferGLTF::Ref> buffers;
    vector<CameraGLTF::Ref> cameras;
    vector<ImageGLTF::Ref> images;
    vector<MaterialGLTF::Ref> materials;
    vector<MeshGLTF::Ref> meshes;
    vector<NodeGLTF::Ref> nodes;
    vector<SamplerGLTF::Ref> samplers;
    vector<SceneGLTF::Ref> scenes;
    vector<SkinGLTF::Ref> skins;
    vector<TextureGLTF::Ref> textures;

    fs::path gltfPath;

    SceneGLTF::Ref activeScene;
};

void NodeGLTF::addChildren(const RootGLTF& rootGLTF)
{
    for (auto& child : property.children)
    {
        addChild(rootGLTF.nodes[child]);
    }
}

NodeGLTF::Ref NodeGLTF::create(const RootGLTF& rootGLTF, const ygltf::node_t& property)
{
    NodeGLTF::Ref ref = make_shared<NodeGLTF>();
    ref->property = property;
    if (property.camera != -1) ref->camera = rootGLTF.cameras[property.camera];
    if (property.mesh != -1) ref->mesh = rootGLTF.meshes[property.mesh];
    if (property.skin != -1) ref->skin = rootGLTF.skins[property.skin];
    ref->setTransform(glm::make_mat4x4(property.matrix.data()));

    return ref;
}

AccessorGLTF::Ref AccessorGLTF::create(const RootGLTF& rootGLTF, const ygltf::accessor_t& property)
{
    AccessorGLTF::Ref ref = make_shared<AccessorGLTF>();
    ref->property = property;
    ref->bufferView = rootGLTF.bufferViews[property.bufferView];

    return ref;
}

ImageGLTF::Ref ImageGLTF::create(const RootGLTF& rootGLTF, const ygltf::image_t& property)
{
    ImageGLTF::Ref ref = make_shared<ImageGLTF>();
    ref->property = property;
    CI_ASSERT(property.bufferView == -1);

    ref->surface = am::surface((rootGLTF.gltfPath.parent_path() / property.uri).string());

    return ref;
}

BufferGLTF::Ref BufferGLTF::create(const RootGLTF& rootGLTF, const ygltf::buffer_t& property)
{
    BufferGLTF::Ref ref = make_shared<BufferGLTF>();
    ref->property = property;

    ref->buffer = am::buffer((rootGLTF.gltfPath.parent_path() / property.uri).string());

    return ref;
}

MaterialGLTF::Ref MaterialGLTF::create(const RootGLTF& rootGLTF, const ygltf::material_t& property)
{
    MaterialGLTF::Ref ref = make_shared<MaterialGLTF>();
    ref->property = property;

    auto fn = [&](TextureGLTF::Ref& tex, int idx) {
        if (idx != -1) tex = rootGLTF.textures[idx];
    };
    fn(ref->emissiveTexture, property.emissiveTexture.index);
    fn(ref->normalTexture, property.normalTexture.index);
    fn(ref->occlusionTexture, property.occlusionTexture.index);

    fn(ref->baseColorTexture, property.pbrMetallicRoughness.baseColorTexture.index);
    fn(ref->metallicRoughnessTexture, property.pbrMetallicRoughness.metallicRoughnessTexture.index);

    //ref->shader = 

    return ref;
}

MeshPrimitiveGLTF::Ref MeshPrimitiveGLTF::create(const RootGLTF& rootGLTF, const ygltf::mesh_primitive_t& property)
{
    MeshPrimitiveGLTF::Ref ref = make_shared<MeshPrimitiveGLTF>();
    ref->property = property;

    for (auto& kv : property.attributes)
        ref->attributes[kv.first] = rootGLTF.accessors[kv.second];
    ref->indices = rootGLTF.accessors[property.indices];
    ref->material = rootGLTF.materials[property.material];

    return ref;
}

TextureGLTF::Ref TextureGLTF::create(const RootGLTF& rootGLTF, const ygltf::texture_t& property)
{
    TextureGLTF::Ref ref = make_shared<TextureGLTF>();
    ref->property = property;
    ref->sampler = rootGLTF.samplers[property.sampler];
    ref->source = rootGLTF.images[property.source];

    return ref;
}

BufferViewGLTF::Ref BufferViewGLTF::create(const RootGLTF& rootGLTF, const ygltf::bufferView_t& property)
{
    BufferViewGLTF::Ref ref = make_shared<BufferViewGLTF>();
    ref->buffer = rootGLTF.buffers[property.buffer];
    ref->property = property;

    return ref;
}

class CinderPBRApp : public App
{
public:
    void setup() override
    {
        log::makeLogger<log::LoggerFile>();

        auto filename = getAssetPath(MESH_FILENAME);
        mRoot = RootGLTF::create(filename);

        const vec2 windowSize = toPixels(getWindowSize());
        mCam = CameraPersp((int32_t)windowSize.x, (int32_t)windowSize.y, 60.0f, 0.01f, 1000.0f);
        //mCam.lookAt( aabb.getMax(), aabb.getCenter() );
        mCamUi = CameraUi(&mCam, getWindow(), -1);

        auto mParams = createConfigUI({ 500, 200 });

        gl::enableDepth();
    }

    void resize() override
    {
        mCam.setAspectRatio(getWindowAspectRatio());
    }

    void update() override
    {
        mRoot->update();
    }

    void draw() override
    {
        gl::setMatrices(mCam);
        gl::clear(Color(0, 0, 0));

        mRoot->draw();
    }

private:
    CameraPersp     mCam;
    CameraUi        mCamUi;

    RootGLTF::Ref   mRoot;
};

CINDER_APP(CinderPBRApp, RendererGl, [](App::Settings* settings) {
    readConfig();
    settings->setWindowSize(APP_WIDTH, APP_HEIGHT);
    settings->setMultiTouchEnabled(false);
})
