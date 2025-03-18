#include "region.h"
#include "config.h"
#include <iostream>

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

const SectionData &Region::getSectionAt(int sx, int sy, int sz) const
{
    return data[sx][sz][sy];
}

const uint16_t Region::getBlockAt(int x, int y, int z) const
{
    int sx = x / SECTION_SIZE;
    int sy = y / SECTION_SIZE;
    int sz = z / SECTION_SIZE;
    
    int dx = x % SECTION_SIZE;
    int dy = y % SECTION_SIZE;
    int dz = z % SECTION_SIZE;

    return this->getSectionAt(sx, sy, sz)[dx][dy][dz];
}

int Region::getRegionXWorld() const
{
    return regionXWorld;
}

int Region::getRegionZWorld() const
{
    return regionZWorld;
}

int Region::getSizeX() const
{
    return N_CHUNKS_PER_REGION_XZ * SECTION_SIZE;
}

int Region::getSizeY() const
{
    return CHUNK_SIZE_Y;
}

int Region::getSizeZ() const
{
    return N_CHUNKS_PER_REGION_XZ * SECTION_SIZE;
}