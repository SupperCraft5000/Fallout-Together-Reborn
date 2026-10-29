#include <TiltedOnlinePCH.h>

#include <Forms/ActorValueInfo.h>

ActorValueInfo* ActorValueInfo::Resolve(uint32_t aId) noexcept
{
    using TGetActorValueInfoArray = ActorValueInfo**();
    POINTER_FALLOUT4(TGetActorValueInfoArray, GetActorValueInfoArray, 405390);

    return GetActorValueInfoArray()[aId];
}
