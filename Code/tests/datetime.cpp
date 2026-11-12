#include <catch2/catch.hpp>

#include "../common/DateTime.h"

namespace
{
DateTime MakeDateTime(float aTime, uint32_t aDay, uint32_t aMonth, uint32_t aYear = 1)
{
    DateTime dateTime;
    dateTime.m_timeModel.Time = aTime;
    dateTime.m_timeModel.Day = aDay;
    dateTime.m_timeModel.Month = aMonth;
    dateTime.m_timeModel.Year = aYear;
    return dateTime;
}
} // namespace

TEST_CASE("DateTime advances within a day", "[common.datetime]")
{
    DateTime dateTime = MakeDateTime(22.5f, 1u, 0u);
    dateTime.AdvanceHours(1.f);

    REQUIRE(dateTime.m_timeModel.Time == Approx(23.5).margin(1e-4));
    REQUIRE(dateTime.m_timeModel.Day == 1u);
}

TEST_CASE("DateTime advance rolls over the day", "[common.datetime]")
{
    DateTime dateTime = MakeDateTime(23.f, 1u, 0u);
    dateTime.AdvanceHours(2.f);

    REQUIRE(dateTime.m_timeModel.Time == Approx(1.0).margin(1e-4));
    REQUIRE(dateTime.m_timeModel.Day == 2u);
}

TEST_CASE("DateTime advance rolls over the month", "[common.datetime]")
{
    DateTime dateTime = MakeDateTime(23.f, 31u, 0u, 5u);
    dateTime.AdvanceHours(2.f);

    REQUIRE(dateTime.m_timeModel.Time == Approx(1.0).margin(1e-4));
    REQUIRE(dateTime.m_timeModel.Day == 1u);
    REQUIRE(dateTime.m_timeModel.Month == 1u);
    REQUIRE(dateTime.m_timeModel.Year == 5u);
}

TEST_CASE("DateTime advance rolls over the year", "[common.datetime]")
{
    DateTime dateTime = MakeDateTime(23.f, 31u, 11u, 5u);
    dateTime.AdvanceHours(2.f);

    REQUIRE(dateTime.m_timeModel.Time == Approx(1.0).margin(1e-4));
    REQUIRE(dateTime.m_timeModel.Day == 1u);
    REQUIRE(dateTime.m_timeModel.Month == 0u);
    REQUIRE(dateTime.m_timeModel.Year == 6u);
}

TEST_CASE("DateTime advances exactly one day", "[common.datetime]")
{
    DateTime dateTime = MakeDateTime(5.f, 3u, 0u);
    dateTime.AdvanceHours(24.f);

    REQUIRE(dateTime.m_timeModel.Time == Approx(5.0).margin(1e-4));
    REQUIRE(dateTime.m_timeModel.Day == 4u);
}

TEST_CASE("DateTime update keeps its behaviour", "[common.datetime]")
{
    DateTime fast = MakeDateTime(23.99f, 1u, 0u);
    fast.m_timeModel.TimeScale = 100.f;
    fast.Update(1000);

    REQUIRE(fast.m_timeModel.Time == Approx(0.0177).margin(1e-3));
    REQUIRE(fast.m_timeModel.Day == 2u);

    DateTime normal = MakeDateTime(12.f, 1u, 0u);
    normal.m_timeModel.TimeScale = 20.f;
    normal.Update(1000);

    REQUIRE(normal.m_timeModel.Time == Approx(12.00556).margin(1e-4));
    REQUIRE(normal.m_timeModel.Day == 1u);
}

TEST_CASE("DateTime hours until a wall clock hour", "[common.datetime]")
{
    REQUIRE(MakeDateTime(13.64f, 1u, 0u).HoursUntil(8) == Approx(18.36).margin(1e-3));
    REQUIRE(MakeDateTime(7.5f, 1u, 0u).HoursUntil(8) == Approx(0.5).margin(1e-3));
    REQUIRE(MakeDateTime(8.f, 1u, 0u).HoursUntil(8) == 24.f);
    REQUIRE(MakeDateTime(0.f, 1u, 0u).HoursUntil(0) == 24.f);
    REQUIRE(MakeDateTime(8.f, 1u, 0u).HoursUntil(24) == -1.f);
    REQUIRE(MakeDateTime(8.f, 1u, 0u).HoursUntil(-1) == -1.f);
}
