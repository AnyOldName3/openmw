#include "types.hpp"

#include <components/esm3/loadingr.hpp>

#include <components/lua/luastate.hpp>

#include <apps/openmw/mwworld/esmstore.hpp>

#include <apps/openmw/mwbase/environment.hpp>
#include <apps/openmw/mwbase/world.hpp>
#include <components/esm3/loadalch.hpp>

namespace sol
{
    template <>
    struct is_automagical<ESM::Ingredient> : std::false_type
    {
    };
}

namespace MWLua
{
    void addIngredientBindings(sol::table ingredient, const Context& context)
    {
        const MWWorld::Store<ESM::Ingredient>* store
            = &MWBase::Environment::get().getWorld()->getStore().get<ESM::Ingredient>();
        ingredient["record"] = sol::overload(
            [](const Object& obj) -> const ESM::Ingredient* { return obj.ptr().get<ESM::Ingredient>()->mBase; },
            [store](const std::string& recordID) -> const ESM::Ingredient* {
                return store->find(ESM::RefId::stringRefId(recordID));
            });
        sol::usertype<ESM::Ingredient> record = context.mLua->sol().new_usertype<ESM::Ingredient>(("ESM3_Ingredient"));
        record[sol::meta_function::to_string]
            = [](const ESM::Potion& rec) { return "ESM3_Ingredient[" + rec.mId.getRefIdString() + "]"; };
        record["id"] = sol::readonly_property(
            [](const ESM::Ingredient& rec) -> std::string { return rec.mId.getRefIdString(); });
        record["name"] = sol::readonly_property([](const ESM::Ingredient& rec) -> std::string { return rec.mName; });
        record["model"] = sol::readonly_property([](const ESM::Ingredient& rec) -> std::string { return rec.mModel; });
        record["mwscript"] = sol::readonly_property(
            [](const ESM::Ingredient& rec) -> std::string { return rec.mScript.getRefIdString(); });
        record["icon"] = sol::readonly_property([](const ESM::Ingredient& rec) -> std::string { return rec.mIcon; });
        record["weight"]
            = sol::readonly_property([](const ESM::Ingredient& rec) -> float { return rec.mData.mWeight; });
        record["value"] = sol::readonly_property([](const ESM::Ingredient& rec) -> int { return rec.mData.mValue; });
    }
}
