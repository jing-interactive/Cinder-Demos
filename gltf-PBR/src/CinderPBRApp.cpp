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

    BufferRef cpuBuffer;

    static Ref create(const RootGLTF& rootGLTF, const ygltf::buffer_t& property);
};

struct BufferViewGLTF
{
    typedef shared_ptr<BufferViewGLTF> Ref;
    ygltf::bufferView_t property;

    BufferRef cpuBuffer;
    gl::VboRef gpuBuffer;

    static Ref create(const RootGLTF& rootGLTF, const ygltf::bufferView_t& property);
};

struct AccessorGLTF
{
    typedef shared_ptr<AccessorGLTF> Ref;
    ygltf::accessor_t property;
    int byteStride; // from ygltf::bufferView_t

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

    //BufferViewGLTF::Ref bufferView;
    SurfaceRef surface;

    static Ref create(const RootGLTF& rootGLTF, const ygltf::image_t& property);
};

struct SamplerGLTF
{
    typedef shared_ptr<SamplerGLTF> Ref;
    ygltf::sampler_t property;

    // TODO: support texture1d / 3d
    gl::Texture2d::Format oglTexFormat;

    static Ref create(const RootGLTF& rootGLTF, const ygltf::sampler_t& property)
    {
        Ref ref = make_shared<SamplerGLTF>();
        ref->property = property;

        ref->oglTexFormat.minFilter((GLenum)property.minFilter);
        ref->oglTexFormat.magFilter((GLenum)property.magFilter);
        ref->oglTexFormat.wrapS((GLenum)property.wrapS);
        ref->oglTexFormat.wrapT((GLenum)property.wrapT);

        return ref;
    }
};

struct TextureGLTF
{
    typedef shared_ptr<TextureGLTF> Ref;
    ygltf::texture_t property;

    gl::Texture2dRef oglTexture;

    static Ref create(const RootGLTF& rootGLTF, const ygltf::texture_t& property);
};

struct MaterialGLTF
{
    typedef shared_ptr<MaterialGLTF> Ref;
    ygltf::material_t property;

    gl::GlslProgRef oglShader;

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

    MaterialGLTF::Ref material;

    gl::VboMeshRef oglVboMesh;

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
    CI_ASSERT_MSG(property.sparse.count == -1, "Unsupported");

    AccessorGLTF::Ref ref = make_shared<AccessorGLTF>();
    auto bufferView = rootGLTF.bufferViews[property.bufferView];
    ref->property = property;
    ref->byteStride = bufferView->property.byteStride;

    return ref;
}

ImageGLTF::Ref ImageGLTF::create(const RootGLTF& rootGLTF, const ygltf::image_t& property)
{
    CI_ASSERT_MSG(property.bufferView == -1, "Unsupported");

    ImageGLTF::Ref ref = make_shared<ImageGLTF>();
    ref->property = property;

    ref->surface = am::surface((rootGLTF.gltfPath.parent_path() / property.uri).string());

    return ref;
}

BufferGLTF::Ref BufferGLTF::create(const RootGLTF& rootGLTF, const ygltf::buffer_t& property)
{
    BufferGLTF::Ref ref = make_shared<BufferGLTF>();
    ref->property = property;

    ref->cpuBuffer = am::buffer((rootGLTF.gltfPath.parent_path() / property.uri).string());

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

geom::Attrib getAttribFromString(const string& str)
{
    if (str == "POSITION") return geom::POSITION;
    if (str == "NORMAL") return geom::NORMAL;
    //if (str == "TANGENT") return geom::TANGENT;
    //if (str == "BITANGENT") return geom::BITANGENT;
    if (str == "JOINT") return geom::BONE_INDEX;
    if (str == "WEIGHT") return geom::BONE_WEIGHT;

    if (str == "TEXCOORD_0") return geom::TEX_COORD_0;
    if (str == "TEXCOORD_1") return geom::TEX_COORD_1;
    if (str == "TEXCOORD_2") return geom::TEX_COORD_2;
    if (str == "TEXCOORD_3") return geom::TEX_COORD_3;

    if (str == "COLOR_0") return geom::COLOR;

    CI_ASSERT_MSG(0, str.c_str());
}

MeshPrimitiveGLTF::Ref MeshPrimitiveGLTF::create(const RootGLTF& rootGLTF, const ygltf::mesh_primitive_t& property)
{
    MeshPrimitiveGLTF::Ref ref = make_shared<MeshPrimitiveGLTF>();
    ref->property = property;

    AccessorGLTF::Ref indices = rootGLTF.accessors[property.indices];
    ref->material = rootGLTF.materials[property.material];

#if 0
    vector<pair<geom::BufferLayout, gl::VboRef>> vertexArrayLayouts;
    for (auto& kv : property.attributes)
    {
        geom::Attrib attrib = getAttribFromString(kv.first);
        AccessorGLTF::Ref attrAccessor = rootGLTF.accessors[kv.second];
        geom::BufferLayout layout;
        layout.append(attrib, 3,  sizeof(Vert), offsetof(Vert, pos));
    }

    GLenum glPrimitiveMode = (GLenum)property.mode;


    layout.append(geom::Attrib::COLOR, 4, sizeof(Vert), offsetof(Vert, color));

    // TODO: too ugly
    create(uint32_t numVertices, GLenum glPrimitive, const std::vector<std::pair<geom::BufferLayout, VboRef>> &vertexArrayBuffers, uint32_t numIndices = 0, GLenum indexType = GL_UNSIGNED_SHORT, const VboRef &indexVbo = VboRef());

    static VboMeshRef	create(uint32_t numVertices, GLenum glPrimitive, const std::vector<std::pair<geom::BufferLayout, VboRef>> &vertexArrayBuffers, uint32_t numIndices = 0, GLenum indexType = GL_UNSIGNED_SHORT, const VboRef &indexVbo = VboRef());


    ref->oglVboMesh = gl::VboMesh::create(mVerts.size(), glPrimitiveMode, { { layout, mVbo } }, indices.size());
    ref->oglVboMesh->bufferIndices(indices.size(), indices.data());


    ref->oglVboMesh = gl::VboMesh::create();

    auto oglIndexVbo = ref->indices->gpuBuffer;
    ref->oglVboMesh = gl::VboMesh::create(mParticles.size(), glPrimitiveMode, { { particleLayout, mParticleVbo } }, oglIndexVbo);

    static VboMeshRef	create(const geom::Source &source, const , oglIndexVbo);
#endif

    return ref;
}

TextureGLTF::Ref TextureGLTF::create(const RootGLTF& rootGLTF, const ygltf::texture_t& property)
{
    TextureGLTF::Ref ref = make_shared<TextureGLTF>();
    ref->property = property;

    SamplerGLTF::Ref sampler = rootGLTF.samplers[property.sampler];
    ImageGLTF::Ref source = rootGLTF.images[property.source];

    // FIXME: ugly
    auto oglTexFormat = sampler->oglTexFormat;
    oglTexFormat.target((GLenum)property.target);
    oglTexFormat.internalFormat((GLenum)property.internalFormat);
    oglTexFormat.dataType((GLenum)property.type);

    ref->oglTexture = gl::Texture2d::create(*source->surface, oglTexFormat);

    return ref;
}

BufferViewGLTF::Ref BufferViewGLTF::create(const RootGLTF& rootGLTF, const ygltf::bufferView_t& property)
{
    CI_ASSERT(property.buffer != -1);
    CI_ASSERT(property.byteLength != -1);
    CI_ASSERT(property.byteOffset != -1);
    CI_ASSERT_MSG(property.byteStride == 0, "TODO");

    BufferViewGLTF::Ref ref = make_shared<BufferViewGLTF>();
    ref->property = property;

    auto buffer = rootGLTF.buffers[property.buffer];
    auto cpuBuffer = buffer->cpuBuffer;
    auto offsetedData = (uint8_t*)cpuBuffer->getData() + cpuBuffer->getSize();
    CI_ASSERT(property.byteOffset + property.byteLength <= cpuBuffer->getSize());
    
    ref->cpuBuffer = Buffer::create(offsetedData, property.byteLength);
    ref->gpuBuffer = gl::Vbo::create((GLenum)property.target, ref->cpuBuffer->getSize(), ref->cpuBuffer->getData());

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
