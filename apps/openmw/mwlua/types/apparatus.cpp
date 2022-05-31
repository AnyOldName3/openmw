#include "types.hpp"

#include <components/esm3/loadappa.hpp>

#include <apps/openmw/mwworld/esmstore.hpp>

#include "../luabindings.hpp"

namespace sol
{
    template <>
    struct is_automagical<ESM::Apparatus> : std::false_type {};
}

namespace MWLua
{
    void addApparatusBindings(sol::table apparatus, const Context& context)
    {
        apparatus["TYPE"] = LuaUtil::makeStrictReadOnly(context.mLua->tableFromPairs<std::string_view, int>({
            {"MortarPestle", ESM::Apparatus::MortarPestle},
            {"Alembic", ESM::Apparatus::Alembic},
            {"Calcinator", ESM::Apparatus::Calcinator},
            {"Retort", ESM::Apparatus::Retort},
            }));

        const MWWorld::Store<ESM::Apparatus>* store = &MWBase::Environment::get().getWorld()->getStore().get<ESM::Apparatus>();
        apparatus["record"] = sol::overload(
            [](const Object& obj) -> const ESM::Apparatus* { return obj.ptr().get<ESM::Apparatus>()->mBase; },
            [store](const std::string& recordId) -> const ESM::Apparatus* { return store->find(recordId); });
        sol::usertype<ESM::Apparatus> record = context.mLua->sol().new_usertype<ESM::Apparatus>("ESM3_Apparatus");
        record[sol::meta_function::to_string] = [](const ESM::Apparatus& rec) { return "ESM3_Apparatus[" + rec.mId + "]"; };
        record["id"] = sol::readonly_property([](const ESM::Apparatus& rec) -> std::string { return rec.mId; });
        record["name"] = sol::readonly_property([](const ESM::Apparatus& rec) -> std::string { return rec.mName; });
        record["model"] = sol::readonly_property([](const ESM::Apparatus& rec) -> std::string { return rec.mModel; });
        record["mwscript"] = sol::readonly_property([](const ESM::Apparatus& rec) -> std::string { return rec.mScript; });
        record["icon"] = sol::readonly_property([](const ESM::Apparatus& rec) -> std::string { return rec.mIcon; });
        record["type"] = sol::readonly_property([](const ESM::Apparatus& rec) -> int { return rec.mData.mType; });
        record["value"] = sol::readonly_property([](const ESM::Apparatus& rec) -> int { return rec.mData.mValue; });
        record["weight"] = sol::readonly_property([](const ESM::Apparatus& rec) -> float { return rec.mData.mWeight; });
        record["quality"] = sol::readonly_property([](const ESM::Apparatus& rec) -> float { return rec.mData.mQuality; });
    }
}