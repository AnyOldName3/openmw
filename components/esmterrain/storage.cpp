#include "storage.hpp"

#include <algorithm>
#include <stdexcept>

#include <osg/Image>
#include <osg/Plane>

#include <components/debug/debuglog.hpp>
#include <components/esm/esmterrain.hpp>
#include <components/esm4/loadland.hpp>
#include <components/misc/resourcehelpers.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/vfs/manager.hpp>

#include "gridsampling.hpp"

namespace ESMTerrain
{

    class LandCache
    {
    public:
        typedef std::map<ESM::ExteriorCellLocation, osg::ref_ptr<const LandObject>> Map;
        Map mMap;
    };

    LandObject::LandObject()
        : mLand(nullptr)
    {
    }

    LandObject::LandObject(const ESM4::Land* land, int loadFlags)
        : mLand(nullptr)
        , mData(*land, loadFlags)
    {
    }

    LandObject::LandObject(const ESM::Land* land, int loadFlags)
        : mLand(land)
        , mData(*land, loadFlags)
    {
    }

    LandObject::LandObject(const LandObject& copy, const osg::CopyOp& copyop)
        : mLand(nullptr)
    {
    }

    LandObject::~LandObject() {}

    const float defaultHeight = ESM::Land::DEFAULT_HEIGHT;

    Storage::Storage(const VFS::Manager* vfs, const std::string& normalMapPattern,
        const std::string& normalHeightMapPattern, bool autoUseNormalMaps, const std::string& specularMapPattern,
        bool autoUseSpecularMaps)
        : mVFS(vfs)
        , mNormalMapPattern(normalMapPattern)
        , mNormalHeightMapPattern(normalHeightMapPattern)
        , mAutoUseNormalMaps(autoUseNormalMaps)
        , mSpecularMapPattern(specularMapPattern)
        , mAutoUseSpecularMaps(autoUseSpecularMaps)
    {
    }

    bool Storage::getMinMaxHeights(float size, const osg::Vec2f& center, ESM::RefId worldspace, float& min, float& max)
    {
        assert(size <= 1 && "Storage::getMinMaxHeights, chunk size should be <= 1 cell");

        osg::Vec2f origin = center - osg::Vec2f(size / 2.f, size / 2.f);

        int cellX = static_cast<int>(std::floor(origin.x()));
        int cellY = static_cast<int>(std::floor(origin.y()));
        osg::ref_ptr<const LandObject> land = getLand(ESM::ExteriorCellLocation(cellX, cellY, worldspace));
        const ESM::LandData* data = land ? land->getData(ESM::Land::DATA_VHGT) : nullptr;
        const int landSize = ESM::getLandSize(worldspace);
        int startRow = (origin.x() - cellX) * landSize;
        int startColumn = (origin.y() - cellY) * landSize;

        int endRow = startRow + size * (landSize - 1) + 1;
        int endColumn = startColumn + size * (landSize - 1) + 1;

        if (data)
        {
            min = std::numeric_limits<float>::max();
            max = -std::numeric_limits<float>::max();
            for (int row = startRow; row < endRow; ++row)
            {
                for (int col = startColumn; col < endColumn; ++col)
                {
                    float h = data->getHeights()[col * landSize + row];
                    if (h > max)
                        max = h;
                    if (h < min)
                        min = h;
                }
            }
            return true;
        }

        min = defaultHeight;
        max = defaultHeight;
        return false;
    }

    void Storage::fixNormal(
        osg::Vec3f& normal, ESM::ExteriorCellLocation cellLocation, int col, int row, LandCache& cache)
    {

        const int landSize = ESM::getLandSize(cellLocation.mWorldspace);

        while (col >= landSize - 1)
        {
            ++cellLocation.mY;
            col -= landSize - 1;
        }
        while (row >= landSize - 1)
        {
            ++cellLocation.mX;
            row -= landSize - 1;
        }
        while (col < 0)
        {
            --cellLocation.mY;
            col += landSize - 1;
        }
        while (row < 0)
        {
            --cellLocation.mX;
            row += landSize - 1;
        }
        const LandObject* land = getLand(cellLocation, cache);
        const ESM::LandData* data = land ? land->getData(ESM::Land::DATA_VNML) : nullptr;
        if (data)
        {
            normal.x() = data->getNormals()[col * landSize * 3 + row * 3];
            normal.y() = data->getNormals()[col * landSize * 3 + row * 3 + 1];
            normal.z() = data->getNormals()[col * landSize * 3 + row * 3 + 2];
            normal.normalize();
        }
        else
            normal = osg::Vec3f(0, 0, 1);
    }

    void Storage::averageNormal(
        osg::Vec3f& normal, ESM::ExteriorCellLocation cellLocation, int col, int row, LandCache& cache)
    {
        osg::Vec3f n1, n2, n3, n4;
        fixNormal(n1, cellLocation, col + 1, row, cache);
        fixNormal(n2, cellLocation, col - 1, row, cache);
        fixNormal(n3, cellLocation, col, row + 1, cache);
        fixNormal(n4, cellLocation, col, row - 1, cache);
        normal = (n1 + n2 + n3 + n4);
        normal.normalize();
    }

    void Storage::fixColour(
        osg::Vec4ub& color, ESM::ExteriorCellLocation cellLocation, int col, int row, LandCache& cache)
    {

        const int landSize = ESM::getLandSize(cellLocation.mWorldspace);

        if (col == landSize - 1)
        {
            ++cellLocation.mY;
            col = 0;
        }
        if (row == landSize - 1)
        {
            ++cellLocation.mX;
            row = 0;
        }
        const LandObject* land = getLand(cellLocation, cache);
        const ESM::LandData* data = land ? land->getData(ESM::Land::DATA_VCLR) : nullptr;
        if (data)
        {
            color.r() = data->getColors()[col * landSize * 3 + row * 3];
            color.g() = data->getColors()[col * landSize * 3 + row * 3 + 1];
            color.b() = data->getColors()[col * landSize * 3 + row * 3 + 2];
        }
        else
        {
            color.r() = 255;
            color.g() = 255;
            color.b() = 255;
        }
    }

    void Storage::fillVertexBuffers(int lodLevel, float size, const osg::Vec2f& center, ESM::RefId worldspace,
        osg::Vec3Array& positions, osg::Vec3Array& normals, osg::Vec4ubArray& colours)
    {
        if (lodLevel < 0 || 63 < lodLevel)
            throw std::invalid_argument("Invalid terrain lod level: " + std::to_string(lodLevel));

        if (size <= 0)
            throw std::invalid_argument("Invalid terrain size: " + std::to_string(size));

        // LOD level n means every 2^n-th vertex is kept
        const std::size_t sampleSize = std::size_t{ 1 } << lodLevel;
        const std::size_t cellSize = static_cast<std::size_t>(ESM::getLandSize(worldspace));
        const std::size_t numVerts = static_cast<std::size_t>(size * (cellSize - 1) / sampleSize) + 1;

        positions.resize(numVerts * numVerts);
        normals.resize(numVerts * numVerts);
        colours.resize(numVerts * numVerts);

        LandCache cache;

        const bool alteration = useAlteration();
        const int landSizeInUnits = ESM::getCellSize(worldspace);
        const osg::Vec2f origin = center - osg::Vec2f(size, size) * 0.5f;
        const int startCellX = static_cast<int>(std::floor(origin.x()));
        const int startCellY = static_cast<int>(std::floor(origin.y()));
        ESM::ExteriorCellLocation lastCellLocation(startCellX - 1, startCellY - 1, worldspace);
        const LandObject* land = nullptr;
        const ESM::LandData* heightData = nullptr;
        const ESM::LandData* normalData = nullptr;
        const ESM::LandData* colourData = nullptr;
        bool validHeightDataExists = false;

        const auto handleSample = [&](std::size_t cellShiftX, std::size_t cellShiftY, std::size_t row, std::size_t col,
                                      std::size_t vertX, std::size_t vertY) {
            const int cellX = startCellX + cellShiftX;
            const int cellY = startCellY + cellShiftY;
            const ESM::ExteriorCellLocation cellLocation(cellX, cellY, worldspace);

            if (lastCellLocation != cellLocation)
            {
                land = getLand(cellLocation, cache);

                heightData = nullptr;
                normalData = nullptr;
                colourData = nullptr;

                if (land != nullptr)
                {
                    heightData = land->getData(ESM::Land::DATA_VHGT);
                    normalData = land->getData(ESM::Land::DATA_VNML);
                    colourData = land->getData(ESM::Land::DATA_VCLR);
                    validHeightDataExists = true;
                }

                lastCellLocation = cellLocation;
            }

            float height = defaultHeight;
            if (heightData != nullptr)
                height = heightData->getHeights()[col * cellSize + row];
            if (alteration)
                height += getAlteredHeight(col, row);

            const std::size_t vertIndex = vertX * numVerts + vertY;

            positions[vertIndex]
                = osg::Vec3f((vertX / static_cast<float>(numVerts - 1) - 0.5f) * size * landSizeInUnits,
                    (vertY / static_cast<float>(numVerts - 1) - 0.5f) * size * landSizeInUnits, height);

            const std::size_t srcArrayIndex = col * cellSize * 3 + row * 3;

            osg::Vec3f normal(0, 0, 1);

            if (normalData != nullptr)
            {
                for (std::size_t i = 0; i < 3; ++i)
                    normal[i] = normalData->getNormals()[srcArrayIndex + i];

                normal.normalize();
            }

            // Normals apparently don't connect seamlessly between cells
            if (col == cellSize - 1 || row == cellSize - 1)
                fixNormal(normal, cellLocation, col, row, cache);

            // some corner normals appear to be complete garbage (z < 0)
            if ((row == 0 || row == cellSize - 1) && (col == 0 || col == cellSize - 1))
                averageNormal(normal, cellLocation, col, row, cache);

            assert(normal.z() > 0);

            normals[vertIndex] = normal;

            osg::Vec4ub color(255, 255, 255, 255);

            if (colourData != nullptr)
                for (std::size_t i = 0; i < 3; ++i)
                    color[i] = colourData->getColors()[srcArrayIndex + i];

            // Does nothing by default, override in OpenMW-CS
            if (alteration)
                adjustColor(col, row, heightData, color);

            // Unlike normals, colors mostly connect seamlessly between cells, but not always...
            if (col == cellSize - 1 || row == cellSize - 1)
                fixColour(color, cellLocation, col, row, cache);

            colours[vertIndex] = color;
        };

        const std::size_t beginX = static_cast<std::size_t>((origin.x() - startCellX) * cellSize);
        const std::size_t beginY = static_cast<std::size_t>((origin.y() - startCellY) * cellSize);
        const std::size_t distance = static_cast<std::size_t>(size * (cellSize - 1)) + 1;

        sampleCellGrid(cellSize, sampleSize, beginX, beginY, distance, handleSample);

        if (!validHeightDataExists && ESM::isEsm4Ext(worldspace))
            std::fill(positions.begin(), positions.end(), osg::Vec3f());
    }

    Storage::UniqueTextureId Storage::getVtexIndexAt(
        ESM::ExteriorCellLocation cellLocation, const LandObject* land, int x, int y, LandCache& cache)
    {
        // For the first/last row/column, we need to get the texture from the neighbour cell
        // to get consistent blending at the borders
        --x;
        ESM::ExteriorCellLocation cellLocationIn = cellLocation;
        if (x < 0)
        {
            --cellLocation.mX;
            x += ESM::Land::LAND_TEXTURE_SIZE;
        }
        while (x >= ESM::Land::LAND_TEXTURE_SIZE)
        {
            ++cellLocation.mX;
            x -= ESM::Land::LAND_TEXTURE_SIZE;
        }
        while (
            y >= ESM::Land::LAND_TEXTURE_SIZE) // Y appears to be wrapped from the other side because why the hell not?
        {
            ++cellLocation.mY;
            y -= ESM::Land::LAND_TEXTURE_SIZE;
        }

        if (cellLocation != cellLocationIn)
            land = getLand(cellLocation, cache);

        assert(x < ESM::Land::LAND_TEXTURE_SIZE);
        assert(y < ESM::Land::LAND_TEXTURE_SIZE);

        const ESM::LandData* data = land ? land->getData(ESM::Land::DATA_VTEX) : nullptr;
        if (data)
        {
            int tex = data->getTextures()[y * ESM::Land::LAND_TEXTURE_SIZE + x];
            if (tex == 0)
                return std::make_pair(0, 0); // vtex 0 is always the base texture, regardless of plugin
            return std::make_pair(tex, land->getPlugin());
        }
        return std::make_pair(0, 0);
    }

    std::string Storage::getTextureName(UniqueTextureId id)
    {
        static constexpr char defaultTexture[] = "textures\\_land_default.dds";
        if (id.first == 0)
            return defaultTexture; // Not sure if the default texture really is hardcoded?

        // NB: All vtex ids are +1 compared to the ltex ids
        const ESM::LandTexture* ltex = getLandTexture(id.first - 1, id.second);
        if (!ltex)
        {
            Log(Debug::Warning) << "Warning: Unable to find land texture index " << id.first - 1 << " in plugin "
                                << id.second << ", using default texture instead";
            return defaultTexture;
        }

        // this is needed due to MWs messed up texture handling
        std::string texture = Misc::ResourceHelpers::correctTexturePath(ltex->mTexture, mVFS);

        return texture;
    }

    void Storage::getBlendmaps(float chunkSize, const osg::Vec2f& chunkCenter, ImageVector& blendmaps,
        std::vector<Terrain::LayerInfo>& layerList, ESM::RefId worldspace)
    {
        osg::Vec2f origin = chunkCenter - osg::Vec2f(chunkSize / 2.f, chunkSize / 2.f);
        int cellX = static_cast<int>(std::floor(origin.x()));
        int cellY = static_cast<int>(std::floor(origin.y()));

        int realTextureSize = ESM::Land::LAND_TEXTURE_SIZE + 1; // add 1 to wrap around next cell

        int rowStart = (origin.x() - cellX) * realTextureSize;
        int colStart = (origin.y() - cellY) * realTextureSize;

        const int blendmapSize = (realTextureSize - 1) * chunkSize + 1;
        // We need to upscale the blendmap 2x with nearest neighbor sampling to look like Vanilla
        const int imageScaleFactor = 2;
        const int blendmapImageSize = blendmapSize * imageScaleFactor;

        LandCache cache;
        std::map<UniqueTextureId, unsigned int> textureIndicesMap;
        ESM::ExteriorCellLocation cellLocation(cellX, cellY, worldspace);

        const LandObject* land = getLand(cellLocation, cache);

        for (int y = 0; y < blendmapSize; y++)
        {
            for (int x = 0; x < blendmapSize; x++)
            {
                UniqueTextureId id = getVtexIndexAt(cellLocation, land, x + rowStart, y + colStart, cache);
                std::map<UniqueTextureId, unsigned int>::iterator found = textureIndicesMap.find(id);
                if (found == textureIndicesMap.end())
                {
                    unsigned int layerIndex = layerList.size();
                    Terrain::LayerInfo info = getLayerInfo(getTextureName(id));

                    // look for existing diffuse map, which may be present when several plugins use the same texture
                    for (unsigned int i = 0; i < layerList.size(); ++i)
                    {
                        if (layerList[i].mDiffuseMap == info.mDiffuseMap)
                        {
                            layerIndex = i;
                            break;
                        }
                    }

                    found = textureIndicesMap.emplace(id, layerIndex).first;

                    if (layerIndex >= layerList.size())
                    {
                        osg::ref_ptr<osg::Image> image(new osg::Image);
                        image->allocateImage(blendmapImageSize, blendmapImageSize, 1, GL_ALPHA, GL_UNSIGNED_BYTE);
                        unsigned char* pData = image->data();
                        memset(pData, 0, image->getTotalDataSize());
                        blendmaps.emplace_back(image);
                        layerList.emplace_back(info);
                    }
                }
                unsigned int layerIndex = found->second;
                unsigned char* pData = blendmaps[layerIndex]->data();
                int realY = (blendmapSize - y - 1) * imageScaleFactor;
                int realX = x * imageScaleFactor;
                pData[((realY + 0) * blendmapImageSize + realX + 0)] = 255;
                pData[((realY + 1) * blendmapImageSize + realX + 0)] = 255;
                pData[((realY + 0) * blendmapImageSize + realX + 1)] = 255;
                pData[((realY + 1) * blendmapImageSize + realX + 1)] = 255;
            }
        }

        if (blendmaps.size() == 1)
            blendmaps.clear(); // If a single texture fills the whole terrain, there is no need to blend
    }

    float Storage::getHeightAt(const osg::Vec3f& worldPos, ESM::RefId worldspace)
    {
        const float cellSize = ESM::getCellSize(worldspace);
        int cellX = static_cast<int>(std::floor(worldPos.x() / cellSize));
        int cellY = static_cast<int>(std::floor(worldPos.y() / cellSize));

        osg::ref_ptr<const LandObject> land = getLand(ESM::ExteriorCellLocation(cellX, cellY, worldspace));
        if (!land)
            return ESM::isEsm4Ext(worldspace) ? std::numeric_limits<float>::lowest() : defaultHeight;

        const ESM::LandData* data = land->getData(ESM::Land::DATA_VHGT);
        if (!data)
            return defaultHeight;
        const int landSize = data->getLandSize();

        // Mostly lifted from Ogre::Terrain::getHeightAtTerrainPosition

        // Normalized position in the cell
        float nX = (worldPos.x() - (cellX * cellSize)) / cellSize;
        float nY = (worldPos.y() - (cellY * cellSize)) / cellSize;

        // get left / bottom points (rounded down)
        float factor = landSize - 1.0f;
        float invFactor = 1.0f / factor;

        int startX = static_cast<int>(nX * factor);
        int startY = static_cast<int>(nY * factor);
        int endX = startX + 1;
        int endY = startY + 1;

        endX = std::min(endX, landSize - 1);
        endY = std::min(endY, landSize - 1);

        // now get points in terrain space (effectively rounding them to boundaries)
        float startXTS = startX * invFactor;
        float startYTS = startY * invFactor;
        float endXTS = endX * invFactor;
        float endYTS = endY * invFactor;

        // get parametric from start coord to next point
        float xParam = (nX - startXTS) * factor;
        float yParam = (nY - startYTS) * factor;

        /* For even / odd tri strip rows, triangles are this shape:
        even     odd
        3---2   3---2
        | / |   | \ |
        0---1   0---1
        */

        // Build all 4 positions in normalized cell space, using point-sampled height
        osg::Vec3f v0(startXTS, startYTS, getVertexHeight(data, startX, startY) / cellSize);
        osg::Vec3f v1(endXTS, startYTS, getVertexHeight(data, endX, startY) / cellSize);
        osg::Vec3f v2(endXTS, endYTS, getVertexHeight(data, endX, endY) / cellSize);
        osg::Vec3f v3(startXTS, endYTS, getVertexHeight(data, startX, endY) / cellSize);
        // define this plane in terrain space
        osg::Plane plane;
        // FIXME: deal with differing triangle alignment
        if (true)
        {
            // odd row
            bool secondTri = ((1.0 - yParam) > xParam);
            if (secondTri)
                plane = osg::Plane(v0, v1, v3);
            else
                plane = osg::Plane(v1, v2, v3);
        }
        /*
        else
        {
            // even row
            bool secondTri = (yParam > xParam);
            if (secondTri)
                plane.redefine(v0, v2, v3);
            else
                plane.redefine(v0, v1, v2);
        }
        */

        // Solve plane equation for z
        return (-plane.getNormal().x() * nX - plane.getNormal().y() * nY - plane[3]) / plane.getNormal().z() * cellSize;
    }

    const LandObject* Storage::getLand(ESM::ExteriorCellLocation cellLocation, LandCache& cache)
    {
        LandCache::Map::iterator found = cache.mMap.find(cellLocation);
        if (found != cache.mMap.end())
            return found->second;
        else
        {
            found = cache.mMap.insert(std::make_pair(cellLocation, getLand(cellLocation))).first;
            return found->second;
        }
    }

    void Storage::adjustColor(int col, int row, const ESM::LandData* heightData, osg::Vec4ub& color) const {}

    float Storage::getAlteredHeight(int col, int row) const
    {
        return 0;
    }

    Terrain::LayerInfo Storage::getLayerInfo(const std::string& texture)
    {
        std::lock_guard<std::mutex> lock(mLayerInfoMutex);

        // Already have this cached?
        std::map<std::string, Terrain::LayerInfo>::iterator found = mLayerInfoMap.find(texture);
        if (found != mLayerInfoMap.end())
            return found->second;

        Terrain::LayerInfo info;
        info.mParallax = false;
        info.mSpecular = false;
        info.mDiffuseMap = texture;

        if (mAutoUseNormalMaps)
        {
            std::string texture_ = texture;
            Misc::StringUtils::replaceLast(texture_, ".", mNormalHeightMapPattern + ".");
            if (mVFS->exists(texture_))
            {
                info.mNormalMap = texture_;
                info.mParallax = true;
            }
            else
            {
                texture_ = texture;
                Misc::StringUtils::replaceLast(texture_, ".", mNormalMapPattern + ".");
                if (mVFS->exists(texture_))
                    info.mNormalMap = texture_;
            }
        }

        if (mAutoUseSpecularMaps)
        {
            std::string texture_ = texture;
            Misc::StringUtils::replaceLast(texture_, ".", mSpecularMapPattern + ".");
            if (mVFS->exists(texture_))
            {
                info.mDiffuseMap = texture_;
                info.mSpecular = true;
            }
        }

        mLayerInfoMap[texture] = info;

        return info;
    }

    float Storage::getCellWorldSize(ESM::RefId worldspace)
    {
        return static_cast<float>(ESM::getCellSize(worldspace));
    }

    int Storage::getCellVertices(ESM::RefId worldspace)
    {
        return ESM::getLandSize(worldspace);
    }

    int Storage::getBlendmapScale(float chunkSize)
    {
        return ESM::Land::LAND_TEXTURE_SIZE * chunkSize;
    }

}