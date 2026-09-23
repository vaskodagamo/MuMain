#include "App/stdafx.h"

#include <doctest.h>

#include "Render/Terrain/ZzzLodTerrain.h"

#include <cmath>
#include <cstring>
#include <memory>

extern vec3_t TerrainNormal[]; // ZzzLodTerrain.cpp

namespace
{
constexpr int CELLS = TERRAIN_SIZE * TERRAIN_SIZE;
constexpr size_t NORMAL_BYTES = sizeof(vec3_t) * CELLS;

// A map with rolling hills, a flat plateau and a cliff, so every kind of slope
// is covered.
constexpr float HILL_HEIGHT = 180.0f;
constexpr float HILL_WAVELENGTH = 17.0f;
constexpr int PLATEAU_START = 100;
constexpr int PLATEAU_END = 140;
constexpr float PLATEAU_HEIGHT = 300.0f;
constexpr int CLIFF_COLUMN = 200;
constexpr float CLIFF_HEIGHT = 380.0f;

// A cell a Map Editor stroke or an earthquake recomputes with the _Part version.
constexpr int PART_X = 60;
constexpr int PART_Y = 90;

void FillHeights()
{
    for (int y = 0; y < TERRAIN_SIZE; ++y)
    {
        for (int x = 0; x < TERRAIN_SIZE; ++x)
        {
            float height = HILL_HEIGHT * (1.0f + std::sin(x / HILL_WAVELENGTH) * std::cos(y / HILL_WAVELENGTH));
            if (x >= PLATEAU_START && x < PLATEAU_END && y >= PLATEAU_START && y < PLATEAU_END)
                height = PLATEAU_HEIGHT;
            if (x == CLIFF_COLUMN)
                height = CLIFF_HEIGHT;
            BackTerrainHeight[TERRAIN_INDEX(x, y)] = height;
        }
    }
}

// What the first map load has always produced: CreateTerrainNormal as it was
// before the fix (adding two triangle normals per cell), run on the zeroed array
// a fresh client starts with.
std::unique_ptr<vec3_t[]> FirstLoadNormals()
{
    auto normals = std::make_unique<vec3_t[]>(CELLS); // zeroed
    for (int y = 0; y < TERRAIN_SIZE; y++)
    {
        for (int x = 0; x < TERRAIN_SIZE; x++)
        {
            int Index = TERRAIN_INDEX(x, y);
            vec3_t v1, v2, v3, v4;
            Vector((x * TERRAIN_SCALE), (y * TERRAIN_SCALE), BackTerrainHeight[TERRAIN_INDEX_REPEAT(x, y)], v4);
            Vector(((x + 1) * TERRAIN_SCALE), (y * TERRAIN_SCALE), BackTerrainHeight[TERRAIN_INDEX_REPEAT((x + 1), y)],
                   v1);
            Vector(((x + 1) * TERRAIN_SCALE), ((y + 1) * TERRAIN_SCALE),
                   BackTerrainHeight[TERRAIN_INDEX_REPEAT((x + 1), (y + 1))], v2);
            Vector((x * TERRAIN_SCALE), ((y + 1) * TERRAIN_SCALE), BackTerrainHeight[TERRAIN_INDEX_REPEAT(x, (y + 1))],
                   v3);
            vec3_t face_normal;
            FaceNormalize(v1, v2, v3, face_normal);
            VectorAdd(normals[Index], face_normal, normals[Index]);
            FaceNormalize(v3, v4, v1, face_normal);
            VectorAdd(normals[Index], face_normal, normals[Index]);
        }
    }
    return normals;
}

bool NormalsEqual(const std::unique_ptr<vec3_t[]>& expected)
{
    return std::memcmp(expected.get(), TerrainNormal, NORMAL_BYTES) == 0;
}

// A painted light map with a different colour in every cell.
void FillLightMap()
{
    for (int y = 0; y < TERRAIN_SIZE; ++y)
    {
        for (int x = 0; x < TERRAIN_SIZE; ++x)
        {
            const int index = TERRAIN_INDEX(x, y);
            TerrainLight[index][0] = static_cast<float>(x) / TERRAIN_SIZE_MASK;
            TerrainLight[index][1] = static_cast<float>(y) / TERRAIN_SIZE_MASK;
            TerrainLight[index][2] = 0.5f;
        }
    }
}

// CreateTerrainLight as it was before its loop body became ComputeTerrainLight.
std::unique_ptr<vec3_t[]> OriginalLight()
{
    auto light = std::make_unique<vec3_t[]>(CELLS);
    vec3_t Light;
    Vector(0.5f, -0.5f, 0.5f, Light);
    for (int y = 0; y < TERRAIN_SIZE; y++)
    {
        for (int x = 0; x < TERRAIN_SIZE; x++)
        {
            int Index = TERRAIN_INDEX(x, y);
            float Luminosity = DotProduct(TerrainNormal[Index], Light) + 0.5f;
            if (Luminosity < 0.f)
                Luminosity = 0.f;
            else if (Luminosity > 1.f)
                Luminosity = 1.f;
            for (int i = 0; i < 3; i++)
                light[Index][i] = TerrainLight[Index][i] * Luminosity;
        }
    }
    return light;
}

// Raises the heights of a rectangle, as a brush stroke would.
void Sculpt(int minX, int minY, int maxX, int maxY)
{
    constexpr float BUMP = 37.5f;
    for (int y = minY; y <= maxY; ++y)
        for (int x = minX; x <= maxX; ++x)
            BackTerrainHeight[TERRAIN_INDEX(x, y)] += BUMP * static_cast<float>((x + y) % 3 + 1);
}

// Sculpts the rectangle, rebuilds only it grown by one cell (as the Map Editor's
// height brush does), then checks the normals and light against a whole-map rebuild.
bool RectRebuildMatchesWholeMap(int minX, int minY, int maxX, int maxY)
{
    Sculpt(minX, minY, maxX, maxY);
    CreateTerrainNormal_Rect(minX - 1, minY - 1, maxX + 1, maxY + 1);
    CreateTerrainLight_Rect(minX - 1, minY - 1, maxX + 1, maxY + 1);
    auto normals = std::make_unique<vec3_t[]>(CELLS);
    auto light = std::make_unique<vec3_t[]>(CELLS);
    std::memcpy(normals.get(), TerrainNormal, NORMAL_BYTES);
    std::memcpy(light.get(), BackTerrainLight, NORMAL_BYTES);

    CreateTerrainNormal();
    CreateTerrainLight();
    return std::memcmp(normals.get(), TerrainNormal, NORMAL_BYTES) == 0 &&
           std::memcmp(light.get(), BackTerrainLight, NORMAL_BYTES) == 0;
}
} // namespace

TEST_CASE("Terrain normals equal the first load's and stay the same on every recompute [engine][terrain]")
{
    FillHeights();
    std::memset(TerrainNormal, 0, NORMAL_BYTES);
    const std::unique_ptr<vec3_t[]> firstLoad = FirstLoadNormals();

    CreateTerrainNormal();
    CHECK(NormalsEqual(firstLoad));

    // A second load, a sculpt stroke or an undo used to add on top of this.
    CreateTerrainNormal();
    CHECK(NormalsEqual(firstLoad));

    CreateTerrainNormal_Part(PART_X, PART_Y);
    CHECK(NormalsEqual(firstLoad));
}

TEST_CASE("Terrain normals do not keep the previous map's slopes [engine][terrain]")
{
    FillHeights();
    CreateTerrainNormal();

    // The next map is flat everywhere: every normal must point straight up,
    // two unit triangle normals long, whatever the previous map looked like.
    for (int i = 0; i < CELLS; ++i)
        BackTerrainHeight[i] = PLATEAU_HEIGHT;
    CreateTerrainNormal();

    constexpr float UP_LENGTH = 2.0f;
    constexpr float TOLERANCE = 1e-5f;
    int tilted = 0;
    for (int i = 0; i < CELLS; ++i)
    {
        const bool up = std::fabs(TerrainNormal[i][0]) < TOLERANCE && std::fabs(TerrainNormal[i][1]) < TOLERANCE &&
                        std::fabs(TerrainNormal[i][2] - UP_LENGTH) < TOLERANCE;
        if (!up)
            ++tilted;
    }
    CHECK(tilted == 0);
}

TEST_CASE("The terrain light is computed as before [engine][terrain]")
{
    FillHeights();
    FillLightMap();
    CreateTerrainNormal();
    CreateTerrainLight();
    const std::unique_ptr<vec3_t[]> original = OriginalLight();
    CHECK(std::memcmp(original.get(), BackTerrainLight, NORMAL_BYTES) == 0);
}

TEST_CASE("Rebuilding a brush's rectangle gives the whole-map normals and light [engine][terrain]")
{
    FillHeights();
    FillLightMap();
    CreateTerrainNormal();
    CreateTerrainLight();

    // In the open, on the cliff and on the plateau's edge.
    CHECK(RectRebuildMatchesWholeMap(40, 50, 52, 61));
    CHECK(RectRebuildMatchesWholeMap(CLIFF_COLUMN - 3, 20, CLIFF_COLUMN + 3, 30));
    CHECK(RectRebuildMatchesWholeMap(PLATEAU_END - 2, PLATEAU_START - 2, PLATEAU_END + 2, PLATEAU_START + 2));
    // At the map's edges: the normals of the far row and column read these heights too.
    CHECK(RectRebuildMatchesWholeMap(0, 0, 6, 6));
    CHECK(RectRebuildMatchesWholeMap(TERRAIN_SIZE - 5, TERRAIN_SIZE - 5, TERRAIN_SIZE - 1, TERRAIN_SIZE - 1));
}

TEST_CASE("A rectangle wider than the map rebuilds every cell once [engine][terrain]")
{
    FillHeights();
    FillLightMap();
    CreateTerrainNormal();
    CreateTerrainLight();
    auto normals = std::make_unique<vec3_t[]>(CELLS);
    std::memcpy(normals.get(), TerrainNormal, NORMAL_BYTES);

    std::memset(TerrainNormal, 0, NORMAL_BYTES);
    CreateTerrainNormal_Rect(-1, -1, TERRAIN_SIZE, TERRAIN_SIZE);
    CHECK(std::memcmp(normals.get(), TerrainNormal, NORMAL_BYTES) == 0);
}
