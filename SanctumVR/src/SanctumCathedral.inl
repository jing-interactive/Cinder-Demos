//
//  SanctumCathedral.h
//  SanctumVR
//
//  Created by Felix Faire on 15/12/2015.
//
//

using namespace ci;
using namespace ci::app;
using namespace std;


class SanctumCathedral
{
public:
    SanctumCathedral()
    {
    }

    void setupModel()
    {
        auto ceiling = createCeilingGeometry();
        addNode("Vault", ceiling, "textures/ElyCeiling.png");
        auto node = addNode("VaultPiping", ceiling, "textures/ElyCeiling.png");
        node->modelMatrix = glm::translate(vec3(0.0f, -0.2f, 0.0f));

        addNode("WindowSides", createWindowSidesGeometry(), "textures/ElyCeiling.png");
        addNode("MainWindows", createMainWindowGeometry(), "textures/ElyCeiling.png");

        addNode("Floor",
            geom::Plane().size(vec2(27.15f, 60.96f))
            .normal(vec3(0.0f, -1.0f, 0.0f))
            >> geom::Translate(0.0f, -6.6f, 0.0f),
            "textures/ElyCeiling.png"
            );

        addNode("WindowPanes", createWindowPanesGeometry(), "textures/ElyCeiling.png");
        addNode("WindowMullions", createWindowMullionsGeometry(), "textures/ElyCeiling.png");
        addNode("CentreMullions", createCentreMullionsGeometry(), "textures/ElyCeiling.png");
        addNode("CentrePanes", createCentrePanesGeometry(), "textures/ElyCeiling.png");
        node = addNode("EndColumns", createEndColumGeometry(), "textures/ElyCeiling.png");
        node->depthWrite = false;
        addNode("Walls", createWallsGeometry(), "textures/ElyCeiling.png");
        node = addNode("Light", createLightGeometry(), "textures/LightBeam.png");
        node->ambient = { 1.0f, 1.0f, 1.0f, 0.15f };
    }

    void update()
    {
        for (auto& node : mNodes)
        {
            node->update();
        }
    }

    void draw()
    {
        gfx::enableAlphaBlending();
        for (auto& node : mNodes)
        {
            node->draw();
        }
    }

private:

    // GEOMETRY FUNCTIONS ////////////////////////////////////////////////////
    geom::SourceMods createCeilingGeometry()
    {
        ObjLoader vaultGeom(loadAsset("obj/ArchElement.obj"));
        geom::SourceMods arches;

        for (int i = 0; i < 5; i++)
        {
            for (int j = 0; j < 2; j++)
            {
                arches &= vaultGeom
                    >> geom::Rotate((j > 0 ? -1.0f : 1.0f) * M_PI * 0.5f, vec3(0.0f, 1.0f, 0.0f))
                    >> geom::Translate(0.0f, 31.71f, 24.38f - (float)i * 12.19f);
            }
        }
        return arches;
    }

    geom::SourceMods createMainWindowGeometry()
    {
        geom::SourceMods mainWindows;

        for (int i = 0; i < 2; i++)
            mainWindows &= geom::Plane().size(vec2(27.15f, 37.31f)).normal(vec3(0.0f, 0.0f, -1.0f))
            >> geom::Translate(0.0f, 12.0f, 30.48f)
            >> geom::Rotate(M_PI * (float)i, vec3(0.0f, 1.0f, 0.0f));

        return mainWindows;
    }

    geom::SourceMods createWindowPanesGeometry()
    {
        geom::SourceMods windowPanes;

        for (int i = -2; i < 3; i++)
            for (int j = -1; j < 2; j++)
                if (i != 0 && j != 0)
                    windowPanes &= geom::Plane().size(vec2(12.19f, 37.31f)).normal(vec3(1.0f, 0.0f, 0.0f))
                    >> geom::Rotate((float)M_PI * 0.5, vec3(-1.0f, 0.0f, 0.0f))
                    >> geom::Translate((float)j * 14.99f, 12.0f, (float)i * 12.19f);

        return windowPanes;
    }

    geom::SourceMods createWindowMullionsGeometry()
    {
        geom::SourceMods windowMullions;

        for (int i = -2; i < 3; i++)
            for (int j = -1; j < 2; j++)
                if (i != 0 && j != 0)
                    windowMullions &= geom::Plane().size(vec2(12.19f, 37.31f)).normal(vec3(1.0f, 0.0f, 0.0f))
                    >> geom::Rotate((float)M_PI * 0.5, vec3(-1.0f, 0.0f, 0.0f))
                    >> geom::Translate((float)j * 14.77f, 12.0f, (float)i * 12.19f);

        return windowMullions;
    }

    geom::SourceMods createCentrePanesGeometry()
    {
        geom::SourceMods centrePanes;

        for (int i = -1; i < 2; i++)
            if (i != 0)
                centrePanes &= geom::Plane().size(vec2(12.19f, 37.31f)).normal(vec3(1.0f, 0.0f, 0.0f))
                >> geom::Rotate((float)M_PI * 0.5, vec3(-1.0f, 0.0f, 0.0f))
                >> geom::Translate((float)i * 14.99, 12.0f, -0.15f);

        return centrePanes;
    }

    geom::SourceMods createCentreMullionsGeometry()
    {
        geom::SourceMods centreMullions;

        for (int i = -1; i < 2; i++)
            if (i != 0)
                centreMullions &= geom::Plane().size(vec2(12.19f, 37.31f)).normal(vec3(1.0f, 0.0f, 0.0f))
                >> geom::Rotate((float)M_PI * 0.5, vec3(-1.0f, 0.0f, 0.0f))
                >> geom::Translate((float)i * 14.77, 12.0f, -0.15f);

        return centreMullions;
    }

    geom::SourceMods createEndColumGeometry()
    {
        geom::SourceMods endColums;

        for (int i = -1; i < 2; i++)
            for (int j = -1; j < 2; j++)
                if (j != 0 && i != 0)
                    endColums &= geom::Plane().size(vec2(4.0f, 30.02f)).normal(vec3(0.0f, 0.0f, -1.0f))
                    >> geom::Translate((float)i * 11.6f, 8.46f, (float)j * 30.0f);

        return endColums;
    }

    geom::SourceMods createWindowSidesGeometry()
    {
        geom::SourceMods windowSides;
        ObjLoader side(loadAsset("obj/WindowSide.obj"));

        for (int i = 0; i < 5; i++)
            for (int j = 0; j < 2; j++)
                windowSides &= side
                >> geom::Rotate((j > 0 ? 1.0f : -1.0f) * (float)M_PI * 0.5f, vec3(0.0f, 1.0f, 0.0f))
                >> geom::Translate(j > 0 ? -13.57f : 13.57f, 26.91f, 24.38 - (float)i * 12.19f);

        return windowSides;
    }

    geom::SourceMods createWallsGeometry()
    {
        geom::SourceMods walls;

        for (int i = -2; i < 3; i++)
            for (int j = -1; j < 2; j++)
                if (j != 0)
                    walls &= geom::Plane().size(vec2(12.19f, 37.31f)).normal(vec3(1.0f, 0.0f, 0.0f))
                    >> geom::Rotate((float)M_PI * 0.5, vec3(-1.0f, 0.0f, 0.0f))
                    >> geom::Translate((float)j * 13.57f, 12.0f, (float)i * 12.19f);

        return walls;
    }

    geom::SourceMods createLightGeometry()
    {
        geom::SourceMods lightGeometry;

        TriMesh lightMesh;

        lightMesh.appendTexCoord(vec2(1.0f, 1.0f));
        lightMesh.appendPosition(vec3(-13.5, 25.5, -6.9));

        lightMesh.appendTexCoord(vec2(0.0f, 1.0f));
        lightMesh.appendPosition(vec3(-11.7, 2.2, -5.8));

        lightMesh.appendTexCoord(vec2(1.0f, 0.0f));
        lightMesh.appendPosition(vec3(9.7, -6.0, 6.7));

        lightMesh.appendTexCoord(vec2(0.0f, 0.0f));
        lightMesh.appendPosition(vec3(-7.6, -6.0, -3.5));

        for (int i = 0; i < 4; i++)
        {
            lightGeometry &= lightMesh
                >> geom::Translate(-1.0f, 0.0f, 19.5f - 13.22f * (float)i);
        }

        return lightGeometry;
    }

    struct Node;
    using NodePtr = shared_ptr < Node > ;

    struct Node
    {
        static NodePtr create(string name, gfx::BatchRef batch, gfx::Texture2dRef tex)
        {
            Node* node = new Node
            {
                name,
                batch,
                tex,
                { 1, 1, 1, 1 },
                true,
                mat4()
            };

            return NodePtr(node);
        }

        string              name;
        gfx::BatchRef       batch;
        gfx::Texture2dRef   tex;
        vec4                ambient;
        bool                depthWrite;
        mat4                modelMatrix;

        void update()
        {
#ifdef VK_API_VERSION_1_0
            batch->uniform("uTex0", tex);
            batch->uniform("ciBlock1.uAmbient", ambient);
            gfx::ScopedModelMatrix matrix;
            gfx::multModelMatrix(modelMatrix);

            batch->setDefaultUniformVars(gfx::context());
            gfx::context()->addPendingUniformVars(batch);
#else
            batch->getGlslProg()->uniform("uTex0", 0);
            batch->getGlslProg()->uniform("ciBlock1.uAmbient", ambient);
#endif
        }

        void draw()
        {
            gfx::ScopedDepthWrite depthWrite(depthWrite);

#ifndef VK_API_VERSION_1_0
            tex->bind();
            gfx::ScopedModelMatrix matrix;
            gfx::multModelMatrix(modelMatrix);
            gfx::color(ColorA(ambient));
#endif
            batch->draw();
        }
    };


    NodePtr addNode(string name, geom::SourceMods geomSource, char* textureFileName)
    {
        gfx::Texture::Format format;
        format.mipmap(true);
#ifndef VK_API_VERSION_1_0
        format.setMaxAnisotropy(32.0f);
#endif
        if (mDefaultShader == nullptr)
        {
#ifndef VK_API_VERSION
            auto shaderDef = gfx::ShaderDef().texture().color();
            mDefaultShader = gfx::getStockShader(shaderDef);
#else
            gfx::GlslProg::Format shaderFormat = gfx::GlslProg::Format()
                .vertex(loadAsset("shader.vert"))
                .fragment(loadAsset("shader.frag"));
            mDefaultShader = gfx::GlslProg::create(shaderFormat);
#endif
        }

        auto nodePtr = Node::create(name,
            gfx::Batch::create(geomSource, mDefaultShader), am::texture2d(textureFileName, format));
        mNodes.push_back(nodePtr);
        return nodePtr;
    }

    gfx::GlslProgRef mDefaultShader;

    vector<NodePtr>   mNodes;
};
