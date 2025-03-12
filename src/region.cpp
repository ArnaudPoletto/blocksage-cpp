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

const uint16_t Region::getBlockAt(int x, int y, int z) const
{
    int sx = x / SECTION_SIZE;
    int sy = y / SECTION_SIZE;
    int sz = z / SECTION_SIZE;
    
    int dx = x % SECTION_SIZE;
    int dy = y % SECTION_SIZE;
    int dz = z % SECTION_SIZE;

    //std::cout << "sx: " << sx << " sy: " << sy << " sz: " << sz << " dx: " << dx << " dy: " << dy << " dz: " << dz << std::endl;
    //std::cout << data.size() << std::endl;

    return data[sx][sz][sy][dx][dy][dz];
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