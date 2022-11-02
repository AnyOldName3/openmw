#include "scope.hpp"

#include <string_view>

#include <components/misc/strings/lower.hpp>

CSMWorld::Scope CSMWorld::getScopeFromId(const std::string& id)
{
    // get root namespace
    std::string namespace_;

    std::string::size_type i = id.find("::");

    if (i != std::string::npos)
        namespace_ = Misc::StringUtils::lowerCase(std::string_view(id).substr(0, i));

    if (namespace_ == "project")
        return Scope_Project;

    if (namespace_ == "session")
        return Scope_Session;

    return Scope_Content;
}
