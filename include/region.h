# pragma once

#include <vector>

using RegionData = std::vector<std::vector<std::vector<std::vector<std::vector<std::vector<uint16_t>>>>>>;

class Region
{
public:
    Region(RegionData data, int regionXWorld, int regionZWorld);
    ~Region();

    const RegionData &getDataByRegion() const;
    int getRegionXWorld() const;
    int getRegionZWorld() const;
private:
    RegionData data;
    int regionXWorld;
    int regionZWorld;
};