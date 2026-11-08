#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Allocator.hpp>
#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Serialization.hpp>

#include <functional>
#include <optional>
#include <utility>

#include <Structs/Inventory.h>

#include <catch2/catch.hpp>

using namespace TiltedPhoques;

namespace
{
Inventory::AlchemyData MakePotion()
{
    Inventory::AlchemyData data;
    data.Kind = Inventory::AlchemyData::kPotion;

    Inventory::EffectItem restore;
    restore.Magnitude = 12.5f;
    restore.Area = 0;
    restore.Duration = 30;
    restore.RawCost = 4.25f;
    restore.EffectId = GameId(1, 0x3EB15);
    data.Effects.push_back(restore);

    Inventory::EffectItem fortify;
    fortify.Magnitude = 8.f;
    fortify.Area = 0;
    fortify.Duration = 60;
    fortify.RawCost = 9.5f;
    fortify.EffectId = GameId(1, 0x3EB1E);
    data.Effects.push_back(fortify);

    return data;
}
} // namespace

TEST_CASE("Alchemy data survives serialization", "[encoding.alchemy]")
{
    Buffer buff(1000);

    Inventory::Entry entry;
    entry.AlchemyInfo = MakePotion();
    entry.BaseId = GameId(Inventory::AlchemyData::kKeyModId, entry.AlchemyInfo.ComputeKey());
    entry.Count = 3;

    Buffer::Writer writer(&buff);
    entry.Serialize(writer);

    Buffer::Reader reader(&buff);
    Inventory::Entry result;
    result.Deserialize(reader);

    REQUIRE(result.BaseId == entry.BaseId);
    REQUIRE(result.Count == 3);
    REQUIRE(result.AlchemyInfo.IsPresent());
    REQUIRE(result.AlchemyInfo.Kind == Inventory::AlchemyData::kPotion);
    REQUIRE(result.AlchemyInfo.Effects.size() == 2);
    REQUIRE(result.AlchemyInfo.Effects[1].Duration == 60);
    REQUIRE(result.AlchemyInfo.Effects[1].EffectId == GameId(1, 0x3EB1E));
    REQUIRE(result.AlchemyInfo.ComputeKey() == entry.AlchemyInfo.ComputeKey());
}

TEST_CASE("Ordinary entries carry no alchemy data", "[encoding.alchemy]")
{
    Buffer buff(1000);

    Inventory::Entry entry;
    entry.BaseId = GameId(1, 0x12EB7);
    entry.Count = 1;
    entry.ExtraHealth = 1.5f;

    Buffer::Writer writer(&buff);
    entry.Serialize(writer);

    Buffer::Reader reader(&buff);
    Inventory::Entry result;
    result.Deserialize(reader);

    REQUIRE(result.BaseId == entry.BaseId);
    REQUIRE(result.ExtraHealth == 1.5f);
    REQUIRE_FALSE(result.AlchemyInfo.IsPresent());
    REQUIRE(result.AlchemyInfo.Effects.empty());
}

TEST_CASE("Alchemy key identifies the content", "[encoding.alchemy]")
{
    const auto potion = MakePotion();

    SECTION("identical potions share a key")
    {
        REQUIRE(potion.ComputeKey() == MakePotion().ComputeKey());
    }

    SECTION("a different magnitude gives a different key")
    {
        auto other = MakePotion();
        other.Effects[0].Magnitude = 13.f;
        REQUIRE(other.ComputeKey() != potion.ComputeKey());
    }

    SECTION("a poison is not the same as a potion")
    {
        auto other = MakePotion();
        other.Kind = Inventory::AlchemyData::kPoison;
        REQUIRE(other.ComputeKey() != potion.ComputeKey());
    }

    SECTION("effect order matters")
    {
        auto other = MakePotion();
        std::swap(other.Effects[0], other.Effects[1]);
        REQUIRE(other.ComputeKey() != potion.ComputeKey());
    }

    SECTION("an empty description is not present")
    {
        Inventory::AlchemyData none;
        REQUIRE_FALSE(none.IsPresent());

        Inventory::AlchemyData noEffects;
        noEffects.Kind = Inventory::AlchemyData::kPotion;
        REQUIRE_FALSE(noEffects.IsPresent());
    }
}
