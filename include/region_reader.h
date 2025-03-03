#pragma once

#include "region.h"
#include "config.h"
#include "byte_buffer.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <thread>
#include <filesystem>

namespace fs = std::filesystem;

class RegionReader
{
public:
    static std::vector<uint16_t> processSection(const std::vector<uint64_t> &data, int bitLength);

    // static std::tuple<int, int, int, int, ChunkData> processChunk(const NBTData &nbtData, const std::unordered_map<std::string, int> &blockIdDict);

    static std::tuple<int, int, int, int, ChunkData> readAndProcessChunk(const ByteBuffer &chunkDataStream, const std::unordered_map<std::string, int> &blockIdDict);

    static ByteBuffer getChunkDataStream(const std::vector<uint32_t> &locations, int chunkIdx, const std::vector<char> &regionData);

    static Region getRegion(
        const std::filesystem::path &filePath,
        const std::unordered_map<std::string, int> &blockIdDict
    );
};