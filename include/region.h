# pragma once

#include <vector>
#include "config.h"

using RegionData = std::vector<std::vector<std::vector<std::vector<std::vector<std::vector<uint16_t>>>>>>;

class Region
{
public:
    Region(RegionData data, int regionXWorld, int regionZWorld);
    ~Region();

    const RegionData &getDataByRegion() const;
    const SectionData &getSectionAt(int sx, int sy, int sz) const;
    const uint16_t getBlockAt(int x, int y, int z) const;
    int getRegionXWorld() const;
    int getRegionZWorld() const;
    int getSizeX() const;
    int getSizeY() const;
    int getSizeZ() const;
private:
    RegionData data;
    int regionXWorld;
    int regionZWorld;
};