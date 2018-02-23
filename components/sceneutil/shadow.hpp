#ifndef COMPONENTS_SCENEUTIL_SHADOW_H
#define COMPONENTS_SCENEUTIL_SHADOW_H

#include <osgShadow/ShadowSettings>
#include <osgShadow/ViewDependentShadowMap>

#include <components/terrain/quadtreeworld.hpp>
#include <components/shader/shadermanager.hpp>

namespace SceneUtil
{
    class MWShadow : public osgShadow::ViewDependentShadowMap
    {
    public:
        static void setupShadowSettings(osg::ref_ptr<osgShadow::ShadowSettings> settings, int castsShadowMask);

        static void disableShadowsForStateSet(osg::ref_ptr<osg::StateSet> stateSet);

        MWShadow();

        virtual void cull(osgUtil::CullVisitor& cv) override;

        virtual Shader::ShaderManager::DefineMap getShadowDefines();

        virtual Shader::ShaderManager::DefineMap getShadowsDisabledDefines();

        class ComputeLightSpaceBounds : public osg::NodeVisitor, public osg::CullStack
        {
        public:
            ComputeLightSpaceBounds(osg::Viewport* viewport, const osg::Matrixd& projectionMatrix, osg::Matrixd& viewMatrix);

            void apply(osg::Node& node);

            void apply(osg::Geode& node);

            void apply(osg::Drawable& drawable);

            void apply(Terrain::QuadTreeWorld& quadTreeWorld);

            void apply(osg::Billboard&);

            void apply(osg::Projection&);

            void apply(osg::Transform& transform);

            void apply(osg::Camera&);

            void updateBound(const osg::BoundingBox& bb);

            void update(const osg::Vec3& v);

            osg::BoundingBox _bb;
        };
    protected:
        const int debugTextureUnit;

        std::vector<osg::ref_ptr<osg::Camera>> debugCameras;

        osg::ref_ptr<osg::Program> debugProgram;

        std::vector<osg::ref_ptr<osg::Node>> debugGeometry;

        const int numberOfShadowMapsPerLight;
        const bool enableShadows;
        const bool debugHud;

        const int baseShadowTextureUnit;

        // Minimum Near Far Ratio tuning parameters
        double minNF = 0.0;
        double maxNF = 1.0;
        int numberOfModeToggles = 5;
        double duration = 20000.0; // milliseconds
        int stepsPerModeToggle = 10;
    };
}

#endif //COMPONENTS_SCENEUTIL_SHADOW_H
