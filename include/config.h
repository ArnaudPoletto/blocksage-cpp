#pragma once

#include <vector>

using BlockId = uint16_t;
using SectionLineData = std::vector<BlockId>;          // Z dimension within a section
using SectionPlaneData = std::vector<SectionLineData>;     // Y dimension within a section
using SectionData = std::vector<SectionPlaneData>;         // X dimension within a section
using ChunkData = std::vector<SectionData>;        // Sections in a chunk
using ChunkLineData = std::vector<ChunkData>;          // Chunk in region
using RegionData = std::vector<ChunkLineData>;             // Final 3D region

const int N_CHUNKS_PER_REGION_XZ = 32;
const int CHUNK_SIZE_Y = 384;
const int SECTION_SIZE = 16;
const int N_SECTIONS_PER_CHUNK_Y = CHUNK_SIZE_Y / SECTION_SIZE;
const int MIN_Y = -64;
const int MAX_Y = 320;