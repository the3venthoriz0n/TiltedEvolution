#pragma once

#include <cstdint>
#include <encoding/Structs/TimeModel.h>

struct DateTime
{
    [[nodiscard]] static uint32_t GetNumberOfDaysByMonthIndex(int index);

    DateTime() = default;
    DateTime(TimeModel aTimeModel)
        : m_timeModel(aTimeModel)
    {}

    bool operator==(const DateTime& other) const;
    void Update(uint64_t aDeltaTick);
    float GetDeltaTime(uint64_t aDeltaTick) const noexcept;
    float GetTimeInDays() const noexcept;
    // Adds game hours to the clock and carries into day, month and year. Ignores non-finite or non-positive input
    void AdvanceHours(float aHours) noexcept;
    // Game hours until the clock next reads aHour:00, 24 if it reads that right now, -1 if aHour is not 0 to 23
    [[nodiscard]] float HoursUntil(int aHour) const noexcept;

    TimeModel m_timeModel;

private:
    // Carries overflowing hours into day, month and year
    void Normalize() noexcept;
};
