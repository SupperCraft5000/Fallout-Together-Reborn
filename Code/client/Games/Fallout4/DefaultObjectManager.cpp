#include "DefaultObjectManager.h"

DefaultObjectManager& DefaultObjectManager::Get()
{
    using TGetDefaultObjectManager = DefaultObjectManager&();
    POINTER_FALLOUT4(TGetDefaultObjectManager, GetDefaultObjectManager, 484974);
    return GetDefaultObjectManager();
}
