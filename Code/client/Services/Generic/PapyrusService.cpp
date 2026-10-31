#include <TiltedOnlinePCH.h>

#include <Events/PapyrusFunctionRegisterEvent.h>

#include <Services/PapyrusService.h>

#include <algorithm>
#include <cctype>

namespace
{
// The engine's string pool ignores case, so a name can arrive spelled like the first string of that text
String MakeKey(const String& acNamespace, const String& acFunction) noexcept
{
    String key = acNamespace + "::" + acFunction;
    std::transform(std::begin(key), std::end(key), std::begin(key), [](unsigned char aChar) { return static_cast<char>(std::tolower(aChar)); });
    return key;
}
} // namespace

PapyrusService::PapyrusService(entt::dispatcher& aDispatcher) noexcept
{
    m_papyrusFunctionRegisterConnection = aDispatcher.sink<PapyrusFunctionRegisterEvent>().connect<&PapyrusService::HandlePapyrusFunctionEvent>(this);
}

const void* PapyrusService::Get(const String& acNamespace, const String& acFunction) const noexcept
{
    const auto itor = m_functions.find(MakeKey(acNamespace, acFunction));
    if (itor != std::end(m_functions))
        return itor->second;

    return nullptr;
}

void PapyrusService::HandlePapyrusFunctionEvent(const PapyrusFunctionRegisterEvent& acEvent) noexcept
{
    m_functions[MakeKey(acEvent.Namespace, acEvent.Name)] = acEvent.Function;
}
