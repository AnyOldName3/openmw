#ifndef OPENMW_ESM_CREATURESTATS_H
#define OPENMW_ESM_CREATURESTATS_H

#include <map>
#include <string>
#include <vector>

#include "statstate.hpp"

#include "components/esm/defs.hpp"

#include "activespells.hpp"
#include "aisequence.hpp"
#include "components/esm/attr.hpp"
#include "components/esm/refid.hpp"
#include "magiceffects.hpp"
#include "spellstate.hpp"

namespace ESM
{
    class ESMReader;
    class ESMWriter;

    // format 0, saved games only
    struct CreatureStats
    {
        struct CorprusStats
        {
            int mWorsenings[Attribute::Length];
            TimeStamp mNextWorsening;
        };

        StatState<float> mAttributes[Attribute::Length];
        StatState<float> mDynamic[3];

        MagicEffects mMagicEffects;

        AiSequence::AiSequence mAiSequence;

        bool mHasAiSettings;
        StatState<int> mAiSettings[4];

        std::map<SummonKey, int> mSummonedCreatureMap;
        std::multimap<int, int> mSummonedCreatures;
        std::vector<int> mSummonGraveyard;

        TimeStamp mTradeTime;
        int mGoldPool;
        int mActorId;
        // int mHitAttemptActorId;

        enum Flags
        {
            Dead = 0x0001,
            DeathAnimationFinished = 0x0002,
            Died = 0x0004,
            Murdered = 0x0008,
            TalkedTo = 0x0010,
            Alarmed = 0x0020,
            Attacked = 0x0040,
            Knockdown = 0x0080,
            KnockdownOneFrame = 0x0100,
            KnockdownOverOneFrame = 0x0200,
            HitRecovery = 0x0400,
            Block = 0x0800,
            RecalcDynamicStats = 0x1000
        };
        bool mDead;
        bool mDeathAnimationFinished;
        bool mDied;
        bool mMurdered;
        bool mTalkedTo;
        bool mAlarmed;
        bool mAttacked;
        bool mKnockdown;
        bool mKnockdownOneFrame;
        bool mKnockdownOverOneFrame;
        bool mHitRecovery;
        bool mBlock;
        unsigned int mMovementFlags;
        float mFallHeight;
        ESM::RefId mLastHitObject;
        ESM::RefId mLastHitAttemptObject;
        bool mRecalcDynamicStats;
        int mDrawState;
        signed char mDeathAnimation;
        TimeStamp mTimeOfDeath;
        int mLevel;
        bool mMissingACDT;

        std::map<ESM::RefId, CorprusStats> mCorprusSpells;
        SpellState mSpells;
        ActiveSpells mActiveSpells;

        /// Initialize to default state
        void blank();

        void load(ESMReader& esm);
        void save(ESMWriter& esm) const;
    };
}

#endif
