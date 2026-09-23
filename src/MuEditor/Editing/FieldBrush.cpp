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

// Calls apply(x, y, values, weight) for each corner of `rect` whose weightAt(x, y) is
// above 0; returns `rect`.
template <typename WeightAt, typename Apply>
CellRect ForEachWeighted(const FloatField& field, const CellRect& rect, WeightAt weightAt, Apply apply)
{
    for (int y = rect.minY; y <= rect.maxY; ++y)
    {
        for (int x = rect.minX; x <= rect.maxX; ++x)
        {
            const float weight = weightAt(x, y);
            if (weight > 0.0f)
                apply(x, y, ValueAt(field, x, y), weight);
        }
    }
    return rect;
}

// Where a round brush acts, and how strongly.
struct CircleWeights
{
    const BrushCircle& circle;
    float operator()(int x, int y) const
    {
        return SoftWeight(circle, x, y, CellAnchor::Corner);
    }
};

// Where a brush of any shape acts, and how strongly.
struct MaskWeights
{
    const WeightMask& mask;
    float operator()(int x, int y) const
    {
        return mask.At(x, y);
    }
};

CellRect MaskRect(const FloatField& field, const WeightMask& mask)
{
    return Grow(mask.rect, 0, field.width, field.height);
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

template <typename WeightAt>
CellRect Add(const FloatField& field, const CellRect& rect, WeightAt weightAt, const float* amount)
{
    return ForEachWeighted(field, rect, weightAt,
                           [&field, amount](int, int, float* values, float weight)
                           {
                               for (int c = 0; c < field.channels; ++c)
                                   values[c] += amount[c] * weight;
                           });
}

template <typename WeightAt>
CellRect MoveToward(const FloatField& field, const CellRect& rect, WeightAt weightAt, const float* target, float rate)
{
    const float clampedRate = std::clamp(rate, 0.0f, 1.0f);
    return ForEachWeighted(field, rect, weightAt,
                           [&field, target, clampedRate](int, int, float* values, float weight)
                           {
                               for (int c = 0; c < field.channels; ++c)
                                   values[c] += (target[c] - values[c]) * clampedRate * weight;
                           });
}

template <typename WeightAt>
CellRect Smooth(const FloatField& field, const CellRect& rect, WeightAt weightAt, float rate)
{
    thread_local Snapshot snapshot;
    if (rect.IsEmpty())
        return rect;
    Capture(field, rect, snapshot);

    const float clampedRate = std::clamp(rate, 0.0f, 1.0f);
    return ForEachWeighted(field, rect, weightAt,
                           [&field, clampedRate](int x, int y, float* values, float weight)
                           {
                               for (int c = 0; c < field.channels; ++c)
                               {
                                   const float average = KernelAverage(snapshot, field.channels, x, y, c);
                                   values[c] += (average - values[c]) * clampedRate * weight;
                               }
                           });
}
} // namespace

CellRect AddToField(const FloatField& field, const BrushCircle& circle, const float* amount)
{
    return Add(field, Footprint(circle, field.width, field.height), CircleWeights{circle}, amount);
}

CellRect MoveFieldToward(const FloatField& field, const BrushCircle& circle, const float* target, float rate)
{
    return MoveToward(field, Footprint(circle, field.width, field.height), CircleWeights{circle}, target, rate);
}

CellRect SmoothField(const FloatField& field, const BrushCircle& circle, float rate)
{
    return Smooth(field, Footprint(circle, field.width, field.height), CircleWeights{circle}, rate);
}

CellRect AddToField(const FloatField& field, const WeightMask& mask, const float* amount)
{
    return Add(field, MaskRect(field, mask), MaskWeights{mask}, amount);
}

CellRect MoveFieldToward(const FloatField& field, const WeightMask& mask, const float* target, float rate)
{
    return MoveToward(field, MaskRect(field, mask), MaskWeights{mask}, target, rate);
}

CellRect SmoothField(const FloatField& field, const WeightMask& mask, float rate)
{
    return Smooth(field, MaskRect(field, mask), MaskWeights{mask}, rate);
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
