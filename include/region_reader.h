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
    static Region getRegion(
        const std::filesystem::path &filePath,
        const std::unordered_map<std::string, uint16_t> &blockIdDict);

private:
    static std::vector<char> getRegionData(const std::filesystem::path &filePath);
    static std::vector<uint32_t> getChunkLocationData(const std::vector<char> &regionData);
    static std::vector<uint16_t> processSection(const std::vector<uint64_t> &data, int bitLength);
    static ByteBuffer getChunkDataStream(const std::vector<uint32_t> &locations, int chunkIdx, const std::vector<char> &regionData);
    static std::tuple<int, int, int, int, ChunkData> readAndProcessChunk(const ByteBuffer &chunkDataStream, const std::unordered_map<std::string, uint16_t> &blockIdDict);
    static std::tuple<int, int> processChunks(const std::vector<uint32_t> &chunkLocationData, const std::vector<char> &regionData, const std::unordered_map<std::string, uint16_t> &blockIdDict, RegionData &data);
};