#include "FieldBrush.h"

#ifdef _EDITOR

#include <algorithm>
#include <cstddef>
#include <vector>

namespace Editor::Editing
{
namespace
{
// Centre plus the four neighbours (the legacy editor's light smoothing kernel).
constexpr float KERNEL_POINTS = 5.0f;

float* ValueAt(const FloatField& field, int x, int y)
{
    const std::size_t cell = static_cast<std::size_t>(y) * static_cast<std::size_t>(field.width) + x;
    return field.data + cell * static_cast<std::size_t>(field.channels);
}

// Calls apply(values, weight) for each corner of `circle` with a weight above 0.
template <typename Apply> CellRect ForEachWeighted(const FloatField& field, const BrushCircle& circle, Apply apply)
{
    const CellRect rect = Footprint(circle, field.width, field.height);
    for (int y = rect.minY; y <= rect.maxY; ++y)
    {
        for (int x = rect.minX; x <= rect.maxX; ++x)
        {
            const float weight = SoftWeight(circle, x, y, CellAnchor::Corner);
            if (weight > 0.0f)
                apply(x, y, ValueAt(field, x, y), weight);
        }
    }
    return rect;
}

// The values of `rect` grown by one corner on every side (clipped), row by row.
struct Snapshot
{
    CellRect rect;
    std::vector<float> values;
};

void Capture(const FloatField& field, const CellRect& footprint, Snapshot& snapshot)
{
    snapshot.rect = Grow(footprint, 1, field.width, field.height);
    snapshot.values.clear();
    for (int y = snapshot.rect.minY; y <= snapshot.rect.maxY; ++y)
    {
        const float* row = ValueAt(field, snapshot.rect.minX, y);
        snapshot.values.insert(snapshot.values.end(), row, row + snapshot.rect.Width() * field.channels);
    }
}

// The snapshot's value of channel `c` at (x, y); a corner outside the map (or the
// snapshot) is replaced by the corner (fallbackX, fallbackY).
float SnapshotValue(const Snapshot& snapshot, int channels, int x, int y, int c, int fallbackX, int fallbackY)
{
    const CellRect& rect = snapshot.rect;
    if (x < rect.minX || x > rect.maxX || y < rect.minY || y > rect.maxY)
    {
        x = fallbackX;
        y = fallbackY;
    }
    const std::size_t cell = static_cast<std::size_t>(y - rect.minY) * static_cast<std::size_t>(rect.Width()) +
                             static_cast<std::size_t>(x - rect.minX);
    return snapshot.values[cell * static_cast<std::size_t>(channels) + static_cast<std::size_t>(c)];
}

float KernelAverage(const Snapshot& snapshot, int channels, int x, int y, int c)
{
    const float sum =
        SnapshotValue(snapshot, channels, x, y, c, x, y) + SnapshotValue(snapshot, channels, x - 1, y, c, x, y) +
        SnapshotValue(snapshot, channels, x + 1, y, c, x, y) + SnapshotValue(snapshot, channels, x, y - 1, c, x, y) +
        SnapshotValue(snapshot, channels, x, y + 1, c, x, y);
    return sum / KERNEL_POINTS;
}
} // namespace

CellRect AddToField(const FloatField& field, const BrushCircle& circle, const float* amount)
{
    return ForEachWeighted(field, circle,
                           [&field, amount](int, int, float* values, float weight)
                           {
                               for (int c = 0; c < field.channels; ++c)
                                   values[c] += amount[c] * weight;
                           });
}

CellRect MoveFieldToward(const FloatField& field, const BrushCircle& circle, const float* target, float rate)
{
    const float clampedRate = std::clamp(rate, 0.0f, 1.0f);
    return ForEachWeighted(field, circle,
                           [&field, target, clampedRate](int, int, float* values, float weight)
                           {
                               for (int c = 0; c < field.channels; ++c)
                                   values[c] += (target[c] - values[c]) * clampedRate * weight;
                           });
}

CellRect SmoothField(const FloatField& field, const BrushCircle& circle, float rate)
{
    thread_local Snapshot snapshot;
    const CellRect footprint = Footprint(circle, field.width, field.height);
    if (footprint.IsEmpty())
        return footprint;
    Capture(field, footprint, snapshot);

    const float clampedRate = std::clamp(rate, 0.0f, 1.0f);
    return ForEachWeighted(field, circle,
                           [&field, clampedRate](int x, int y, float* values, float weight)
                           {
                               for (int c = 0; c < field.channels; ++c)
                               {
                                   const float average = KernelAverage(snapshot, field.channels, x, y, c);
                                   values[c] += (average - values[c]) * clampedRate * weight;
                               }
                           });
}

void ClampField(const FloatField& field, const CellRect& rect, float low, float high)
{
    for (int y = rect.minY; y <= rect.maxY; ++y)
    {
        float* row = ValueAt(field, rect.minX, y);
        const int count = rect.Width() * field.channels;
        for (int i = 0; i < count; ++i)
            row[i] = std::clamp(row[i], low, high);
    }
}
} // namespace Editor::Editing

#endif // _EDITOR
