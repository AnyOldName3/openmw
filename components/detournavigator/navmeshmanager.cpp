#include "navmeshmanager.hpp"
#include "debug.hpp"
#include "exceptions.hpp"
#include "gettilespositions.hpp"
#include "makenavmesh.hpp"
#include "navmeshcacheitem.hpp"
#include "settings.hpp"
#include "settingsutils.hpp"
#include "waitconditiontype.hpp"

#include <components/bullethelpers/heightfield.hpp>
#include <components/debug/debuglog.hpp>
#include <components/esm/refid.hpp>
#include <components/misc/convert.hpp>

#include <osg/io_utils>

#include <DetourNavMesh.h>

#include <iterator>

namespace
{
    /// Safely reset shared_ptr with definite underlying object destrutor call.
    /// Assuming there is another thread holding copy of this shared_ptr or weak_ptr to this shared_ptr.
    template <class T>
    bool resetIfUnique(std::shared_ptr<T>& ptr)
    {
        const std::weak_ptr<T> weak(ptr);
        ptr.reset();
        if (auto shared = weak.lock())
        {
            ptr = std::move(shared);
            return false;
        }
        return true;
    }
}

namespace DetourNavigator
{
    namespace
    {
        TileBounds makeBounds(const RecastSettings& settings, const osg::Vec2f& center, int maxTiles)
        {
            const float radius = fromNavMeshCoordinates(
                settings, std::ceil(std::sqrt(static_cast<float>(maxTiles) / osg::PIf) + 1) * getTileSize(settings));
            TileBounds result;
            result.mMin = center - osg::Vec2f(radius, radius);
            result.mMax = center + osg::Vec2f(radius, radius);
            return result;
        }
    }

    NavMeshManager::NavMeshManager(const Settings& settings, std::unique_ptr<NavMeshDb>&& db)
        : mSettings(settings)
        , mRecastMeshManager(settings.mRecast)
        , mOffMeshConnectionsManager(settings.mRecast)
        , mAsyncNavMeshUpdater(settings, mRecastMeshManager, mOffMeshConnectionsManager, std::move(db))
    {
    }

    void NavMeshManager::setWorldspace(const ESM::RefId& worldspace, const UpdateGuard* guard)
    {
        if (worldspace == mWorldspace)
            return;
        mRecastMeshManager.setWorldspace(worldspace, getImpl(guard));
        for (auto& [agent, cache] : mCache)
            cache = std::make_shared<GuardedNavMeshCacheItem>(makeEmptyNavMesh(mSettings), ++mGenerationCounter);
        mWorldspace = worldspace;
    }

    void NavMeshManager::updateBounds(const osg::Vec3f& playerPosition, const UpdateGuard* guard)
    {
        const TileBounds bounds = makeBounds(
            mSettings.mRecast, osg::Vec2f(playerPosition.x(), playerPosition.y()), mSettings.mMaxTilesNumber);
        mRecastMeshManager.setBounds(bounds, getImpl(guard));
    }

    bool NavMeshManager::addObject(const ObjectId id, const CollisionShape& shape, const btTransform& transform,
        const AreaType areaType, const UpdateGuard* guard)
    {
        return mRecastMeshManager.addObject(id, shape, transform, areaType, getImpl(guard));
    }

    bool NavMeshManager::updateObject(
        const ObjectId id, const btTransform& transform, const AreaType areaType, const UpdateGuard* guard)
    {
        return mRecastMeshManager.updateObject(id, transform, areaType, getImpl(guard));
    }

    void NavMeshManager::removeObject(const ObjectId id, const UpdateGuard* guard)
    {
        mRecastMeshManager.removeObject(id, getImpl(guard));
    }

    void NavMeshManager::addWater(const osg::Vec2i& cellPosition, int cellSize, float level, const UpdateGuard* guard)
    {
        mRecastMeshManager.addWater(cellPosition, cellSize, level, getImpl(guard));
    }

    void NavMeshManager::removeWater(const osg::Vec2i& cellPosition, const UpdateGuard* guard)
    {
        mRecastMeshManager.removeWater(cellPosition, getImpl(guard));
    }

    void NavMeshManager::addHeightfield(
        const osg::Vec2i& cellPosition, int cellSize, const HeightfieldShape& shape, const UpdateGuard* guard)
    {
        mRecastMeshManager.addHeightfield(cellPosition, cellSize, shape, getImpl(guard));
    }

    void NavMeshManager::removeHeightfield(const osg::Vec2i& cellPosition, const UpdateGuard* guard)
    {
        mRecastMeshManager.removeHeightfield(cellPosition, getImpl(guard));
    }

    void NavMeshManager::addAgent(const AgentBounds& agentBounds)
    {
        auto cached = mCache.find(agentBounds);
        if (cached != mCache.end())
            return;
        mCache.insert(std::make_pair(
            agentBounds, std::make_shared<GuardedNavMeshCacheItem>(makeEmptyNavMesh(mSettings), ++mGenerationCounter)));
        mPlayerTile.reset();
        Log(Debug::Debug) << "cache add for agent=" << agentBounds;
    }

    bool NavMeshManager::reset(const AgentBounds& agentBounds)
    {
        const auto it = mCache.find(agentBounds);
        if (it == mCache.end())
            return true;
        if (!resetIfUnique(it->second))
            return false;
        mCache.erase(agentBounds);
        mPlayerTile.reset();
        return true;
    }

    void NavMeshManager::addOffMeshConnection(
        const ObjectId id, const osg::Vec3f& start, const osg::Vec3f& end, const AreaType areaType)
    {
        mOffMeshConnectionsManager.add(id, OffMeshConnection{ start, end, areaType });

        const auto startTilePosition = getTilePosition(mSettings.mRecast, start);
        const auto endTilePosition = getTilePosition(mSettings.mRecast, end);

        mRecastMeshManager.addChangedTile(startTilePosition, ChangeType::add);

        if (startTilePosition != endTilePosition)
            mRecastMeshManager.addChangedTile(endTilePosition, ChangeType::add);
    }

    void NavMeshManager::removeOffMeshConnections(const ObjectId id)
    {
        const auto changedTiles = mOffMeshConnectionsManager.remove(id);
        for (const auto& tile : changedTiles)
            mRecastMeshManager.addChangedTile(tile, ChangeType::update);
    }

    void NavMeshManager::update(const osg::Vec3f& playerPosition, const UpdateGuard* guard)
    {
        const auto playerTile
            = getTilePosition(mSettings.mRecast, toNavMeshCoordinates(mSettings.mRecast, playerPosition));
        if (mLastRecastMeshManagerRevision == mRecastMeshManager.getRevision() && mPlayerTile.has_value()
            && *mPlayerTile == playerTile)
            return;
        mLastRecastMeshManagerRevision = mRecastMeshManager.getRevision();
        mPlayerTile = playerTile;
        const auto changedTiles = mRecastMeshManager.takeChangedTiles(getImpl(guard));
        const TilesPositionsRange range = mRecastMeshManager.getRange();
        for (const auto& [agentBounds, cached] : mCache)
            update(agentBounds, playerTile, range, cached, changedTiles);
    }

    void NavMeshManager::update(const AgentBounds& agentBounds, const TilePosition& playerTile,
        const TilesPositionsRange& range, const SharedNavMeshCacheItem& cached,
        const std::map<osg::Vec2i, ChangeType>& changedTiles)
    {
        std::map<osg::Vec2i, ChangeType> tilesToPost = changedTiles;
        {
            const auto locked = cached->lockConst();
            const auto& navMesh = locked->getImpl();
            const auto maxTiles = std::min(mSettings.mMaxTilesNumber, navMesh.getParams()->maxTiles);
            getTilesPositions(range, [&](const TilePosition& tile) {
                if (changedTiles.find(tile) != changedTiles.end())
                    return;
                const bool shouldAdd = shouldAddTile(tile, playerTile, maxTiles);
                const bool presentInNavMesh = navMesh.getTileAt(tile.x(), tile.y(), 0) != nullptr;
                if (shouldAdd && !presentInNavMesh)
                    tilesToPost.emplace(tile, locked->isEmptyTile(tile) ? ChangeType::update : ChangeType::add);
                else if (!shouldAdd && presentInNavMesh)
                    tilesToPost.emplace(tile, ChangeType::mixed);
            });
        }
        mAsyncNavMeshUpdater.post(agentBounds, cached, playerTile, mWorldspace, tilesToPost);
        Log(Debug::Debug) << "Cache update posted for agent=" << agentBounds << " playerTile=" << playerTile
                          << " recastMeshManagerRevision=" << mLastRecastMeshManagerRevision;
    }

    void NavMeshManager::wait(WaitConditionType waitConditionType, Loading::Listener* listener)
    {
        mAsyncNavMeshUpdater.wait(waitConditionType, listener);
    }

    SharedNavMeshCacheItem NavMeshManager::getNavMesh(const AgentBounds& agentBounds) const
    {
        return getCached(agentBounds);
    }

    std::map<AgentBounds, SharedNavMeshCacheItem> NavMeshManager::getNavMeshes() const
    {
        return mCache;
    }

    Stats NavMeshManager::getStats() const
    {
        return Stats{ .mUpdater = mAsyncNavMeshUpdater.getStats() };
    }

    RecastMeshTiles NavMeshManager::getRecastMeshTiles() const
    {
        RecastMeshTiles result;
        getTilesPositions(mRecastMeshManager.getRange(), [&](const TilePosition& v) {
            if (auto mesh = mRecastMeshManager.getCachedMesh(mWorldspace, v))
                result.emplace(v, std::move(mesh));
        });
        return result;
    }

    SharedNavMeshCacheItem NavMeshManager::getCached(const AgentBounds& agentBounds) const
    {
        const auto cached = mCache.find(agentBounds);
        if (cached != mCache.end())
            return cached->second;
        return SharedNavMeshCacheItem();
    }
}
