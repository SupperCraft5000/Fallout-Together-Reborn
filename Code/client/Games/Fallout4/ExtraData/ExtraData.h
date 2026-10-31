#pragma once

#include <Games/Primitives.h>

struct TESObjectREFR;
struct TESForm;

enum class ExtraDataType : uint8_t
{
    None = 0,
    LeveledCreature = 0x2D,
    Faction = 0x5B,
    Soul = 0x9C,
    BendableSplineParams = 0xB4,
    PowerLinks = 0xD0,
};

struct BSExtraData
{
    inline static constexpr auto eExtraData = ExtraDataType::None;

    virtual ~BSExtraData() = 0;
    // TODO: this is different for fallout 4 it seems
    virtual ExtraDataType GetType() const noexcept = 0;

    BSExtraData* next{};
    uint16_t usFlags;
    ExtraDataType type;
};

static_assert(offsetof(BSExtraData, type) == 0x12);
static_assert(sizeof(BSExtraData) == 0x18);
