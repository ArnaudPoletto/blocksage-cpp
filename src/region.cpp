#include "region.h"

Region::Region(RegionData data, int regionXWorld, int regionZWorld)
    : data(data), regionXWorld(regionXWorld), regionZWorld(regionZWorld)
{
}

Region::~Region()
{
}

const RegionData &Region::getDataByRegion() const
{
    return data;
}

int Region::getRegionXWorld() const
{
    return regionXWorld;
}

int Region::getRegionZWorld() const
{
    return regionZWorld;
}