#include "quickkeys.hpp"

#include "esmreader.hpp"
#include "esmwriter.hpp"

namespace ESM
{

    void QuickKeys::load(ESMReader& esm)
    {
        if (esm.isNextSub("KEY_"))
            esm.getSubHeader(); // no longer used, because sub-record hierachies do not work properly in esmreader

        while (esm.isNextSub("TYPE"))
        {
            int keyType;
            esm.getHT(keyType);

            QuickKey key;
            key.mType = keyType;
            key.mId = esm.getHNRefId("ID__");

            mKeys.push_back(key);

            if (esm.isNextSub("KEY_"))
                esm.getSubHeader(); // no longer used, because sub-record hierachies do not work properly in esmreader
        }
    }

    void QuickKeys::save(ESMWriter& esm) const
    {
        for (std::vector<QuickKey>::const_iterator it = mKeys.begin(); it != mKeys.end(); ++it)
        {
            esm.writeHNT("TYPE", it->mType);
            esm.writeHNRefId("ID__", it->mId);
        }
    }

}
