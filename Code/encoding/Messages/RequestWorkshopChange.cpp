#include <Messages/RequestWorkshopChange.h>

void RequestWorkshopChange::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Change.Serialize(aWriter);
}

void RequestWorkshopChange::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ClientMessage::DeserializeRaw(aReader);

    Change.Deserialize(aReader);
}
