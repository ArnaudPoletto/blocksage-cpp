#include "region_reader.h"
#include "nbt_parser.h"
#include "config.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>
#include <future>
#include <zlib/zlib.h>

namespace fs = std::filesystem;

const int ZLIB_COMPRESSION_TYPE = 2;
const int OFFSET_SHIFT = 8;
const int SECTOR_BYTES = 4096;
const int SECTOR_COUNT_MASK = 0xFF;
const int TOTAL_SECTION_BLOCKS = SECTION_SIZE * SECTION_SIZE * SECTION_SIZE;

std::vector<char> RegionReader::getRegionData(const std::filesystem::path &filePath)
{
    // Open the region file
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open region file: " + filePath.string());
    }

    // Get file size
    file.seekg(0, std::ios::end);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read the entire file into memory
    std::vector<char> regionData(size);
    if (!file.read(regionData.data(), size))
    {
        throw std::runtime_error("Failed to read region file: " + filePath.string());
    }

    return regionData;
}

std::vector<uint32_t> RegionReader::getChunkLocationData(const std::vector<char> &regionData)
{
    // Read the chunk location table
    std::vector<uint32_t> locations(SECTOR_BYTES / 4);
    for (int i = 0; i < SECTOR_BYTES / 4; ++i)
    {
        uint32_t location =
            (static_cast<uint32_t>(static_cast<unsigned char>(regionData[i * 4])) << 24) |
            (static_cast<uint32_t>(static_cast<unsigned char>(regionData[i * 4 + 1])) << 16) |
            (static_cast<uint32_t>(static_cast<unsigned char>(regionData[i * 4 + 2])) << 8) |
            (static_cast<uint32_t>(static_cast<unsigned char>(regionData[i * 4 + 3])));
        locations[i] = location;
    }

    return locations;
}

std::vector<uint16_t> RegionReader::processSection(const std::vector<uint64_t> &data, int bitLength)
{
    if (bitLength <= 0 || bitLength > 64)
    {
        throw std::invalid_argument("Invalid bit length: " + std::to_string(bitLength));
    }

    int indicesPerLong = 64 / bitLength;
    uint64_t mask = (1ULL << bitLength) - 1;

    std::vector<uint16_t> result;
    result.reserve(TOTAL_SECTION_BLOCKS);

    for (uint64_t value : data)
    {
        for (int i = 0; i < indicesPerLong && result.size() < TOTAL_SECTION_BLOCKS; ++i)
        {
            uint16_t index = (value >> (i * bitLength)) & mask;
            result.push_back(index);
        }
    }

    return result;
}

ByteBuffer RegionReader::getChunkDataStream(const std::vector<uint32_t> &locations, int chunkIdx, const std::vector<char> &regionData)
{
    uint32_t offset = (locations[chunkIdx] >> OFFSET_SHIFT) * SECTOR_BYTES;
    uint32_t sectorCount = (locations[chunkIdx] & SECTOR_COUNT_MASK) * SECTOR_BYTES;

    if (offset + sectorCount > regionData.size())
    {
        throw std::out_of_range("Chunk offset is outside region data range");
    }

    std::vector<char> chunkData(regionData.begin() + offset, regionData.begin() + offset + sectorCount);
    return ByteBuffer(std::move(chunkData));
}

std::tuple<int, int, int, int, ChunkData> RegionReader::readAndProcessChunk(const ByteBuffer &chunkDataStream, const std::unordered_map<std::string, uint16_t> &blockIdDict)
{
    ByteBuffer buffer(chunkDataStream.data());
    buffer.seek(0);

    // Read the chunk header
    uint32_t chunkDataLength = buffer.read<uint32_t>();
    uint8_t compressionType = buffer.read<uint8_t>();

    // Invalid chunk or unsupported compression type
    if (chunkDataLength <= 0 || compressionType != ZLIB_COMPRESSION_TYPE)
    {
        throw std::runtime_error("Invalid chunk or unsupported compression type");
    }

    // Read and decompress the chunk data
    std::vector<char> compressedData = buffer.read(chunkDataLength - 1); // -1 to exclude the compression type byte
    z_stream zstream;
    std::memset(&zstream, 0, sizeof(zstream));
    if (inflateInit(&zstream) != Z_OK)
    {
        throw std::runtime_error("Failed to initialize zlib inflation");
    }

    zstream.next_in = reinterpret_cast<Bytef *>(compressedData.data());
    zstream.avail_in = compressedData.size();

    std::vector<char> decompressedData;
    char outBuffer[4096];

    do
    {
        zstream.next_out = reinterpret_cast<Bytef *>(outBuffer);
        zstream.avail_out = sizeof(outBuffer);

        int ret = inflate(&zstream, Z_NO_FLUSH);

        if (ret != Z_OK && ret != Z_STREAM_END)
        {
            inflateEnd(&zstream);
            throw std::runtime_error("Failed to decompress chunk data");
        }

        decompressedData.insert(decompressedData.end(), outBuffer, outBuffer + (sizeof(outBuffer) - zstream.avail_out));
    } while (zstream.avail_out == 0);

    inflateEnd(&zstream);

    // Parse the decompressed data
    ByteBuffer decompressedBuffer(decompressedData);
    NBTParser::NBTTag root = NBTParser::parseNBT(decompressedBuffer);

    // Initialize empty data
    ChunkData chunkBlocks(
        N_SECTIONS_PER_CHUNK_Y,
        SectionData(
            SECTION_SIZE,
            SectionPlaneData(
                SECTION_SIZE,
                SectionLineData(SECTION_SIZE, 0xFFFF) // 0xFFFF for missing sections
                )));

    // Process the chunk data
    int chunkXInWorld = root.compoundValue["xPos"].intValue;
    int chunkZInWorld = root.compoundValue["zPos"].intValue;
    int chunkXInRegion = chunkXInWorld % N_CHUNKS_PER_REGION_XZ;
    int chunkZInRegion = chunkZInWorld % N_CHUNKS_PER_REGION_XZ;

    NBTParser::NBTTag sections = root.compoundValue["sections"];
    for (NBTParser::NBTTag sectionEntry : sections.listValue)
    {
        NBTParser::NBTTag sectionBlockStates = sectionEntry.compoundValue["block_states"];

        // Get the section Y value and index
        NBTParser::NBTTag sectionY = sectionEntry.compoundValue["Y"];
        int sectionYValue = static_cast<int8_t>(sectionY.intValue);
        int sectionYIndex = sectionYValue - (MIN_Y / SECTION_SIZE);

        // Get the palette data
        std::vector<std::string> palette;
        NBTParser::NBTTag sectionPalette = sectionBlockStates.compoundValue["palette"];
        if (sectionPalette.type != NBTParser::TagType::TagList)
        {
            continue;
        }
        for (NBTParser::NBTTag paletteEntry : sectionPalette.listValue)
        {
            if (paletteEntry.type != NBTParser::TagType::TagCompound)
            {
                continue;
            }
            for (auto &entry : paletteEntry.compoundValue)
            {
                if (entry.first != "Name" || entry.second.type != NBTParser::TagType::TagString)
                {
                    continue;
                }
                std::string blockName = entry.second.stringValue;
                blockName = blockName.substr(10);
                palette.push_back(blockName);
            }
        }

        // Get the data
        NBTParser::NBTTag sectionData = sectionBlockStates.compoundValue["data"];

        // By default, all blocks are the same: the first block in the palette
        std::vector<uint16_t> flatSectionBlockIndices(TOTAL_SECTION_BLOCKS, 0x0000);
        if (sectionData.type == NBTParser::TagType::TagLongArray)
        {
            int bit_length = std::max(4, int(ceil(log2(palette.size()))));
            flatSectionBlockIndices = processSection(sectionData.longArrayValue, bit_length);
        }

        // Convert block indices to block IDs
        int i = 0;
        for (int blockIdx : flatSectionBlockIndices)
        {
            // Get block coordinates within the section
            int sx = i % SECTION_SIZE;
            int sz = (i / SECTION_SIZE) % SECTION_SIZE;
            int sy = i / (SECTION_SIZE * SECTION_SIZE);

            const std::string &blockName = palette[blockIdx];
            auto it = blockIdDict.find(blockName);

            if (it != blockIdDict.end())
            {
                chunkBlocks[sectionYIndex][sx][sy][sz] = it->second;
            }
            else
            {
                std::cerr << "Unknown block found: " << blockName << std::endl;
            }
            i += 1;
        }
    }

    return {chunkXInRegion, chunkZInRegion, chunkXInWorld, chunkZInWorld, chunkBlocks};
}

std::tuple<int, int> RegionReader::processChunks(
    const std::vector<uint32_t> &chunkLocationData,
    const std::vector<char> &regionData,
    const std::unordered_map<std::string, uint16_t> &blockIdDict,
    RegionData &data)
{
    // Process chunks in parallel
    std::cout << "Processing chunks..." << std::endl;
    std::vector<std::future<std::tuple<int, int, int, int, ChunkData>>> futures;
    for (int chunkIdx = 0; chunkIdx < N_CHUNKS_PER_REGION_XZ * N_CHUNKS_PER_REGION_XZ; ++chunkIdx)
    {
        if (chunkLocationData[chunkIdx] == 0)
        {
            continue;
        }

        futures.emplace_back(
            std::async(
                std::launch::async, [&, chunkIdx]()
                { 
                    ByteBuffer chunkDataStream = getChunkDataStream(chunkLocationData, chunkIdx, regionData);
                    return readAndProcessChunk(chunkDataStream, blockIdDict); }));
    }

    int regionXWorld = std::numeric_limits<int>::min();
    int regionZWorld = std::numeric_limits<int>::min();
    for (auto &future : futures)
    {
        try
        {
            auto result = future.get();
            auto [chunkXRegion, chunkZRegion, chunkXWorld, chunkZWorld, chunkBlocks] = result;

            // Set the chunk data
            data[chunkXRegion][chunkZRegion] = std::move(chunkBlocks);

            if (regionXWorld == std::numeric_limits<int>::min() || regionZWorld == std::numeric_limits<int>::min())
            {
                regionXWorld = chunkXWorld / N_CHUNKS_PER_REGION_XZ;
                regionZWorld = chunkZWorld / N_CHUNKS_PER_REGION_XZ;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error processing chunk: " << e.what() << std::endl;
        }
    }
    std::cout << "Chunk processing complete!" << std::endl;

    return {regionXWorld, regionZWorld};
}

Region RegionReader::getRegion(
    const std::filesystem::path &filePath,
    const std::unordered_map<std::string, uint16_t> &blockIdDict)
{
    // Initialize empty data
    RegionData data(
        N_CHUNKS_PER_REGION_XZ,
        ChunkLineData(
            N_CHUNKS_PER_REGION_XZ,
            ChunkData(
                CHUNK_SIZE_Y / SECTION_SIZE,
                SectionData(
                    SECTION_SIZE,
                    SectionPlaneData(
                        SECTION_SIZE,
                        SectionLineData(SECTION_SIZE, 0xFFFF) // 0xFFFF for missing sections
                        )))));

    // Get region data into memory
    std::vector<char> regionData = getRegionData(filePath);

    // Read the chunk location table
    std::vector<uint32_t> chunkLocationData = getChunkLocationData(regionData);

    // Process chunks in parallel
    int regionXWorld;
    int regionZWorld;
    std::tie(regionXWorld, regionZWorld) = processChunks(chunkLocationData, regionData, blockIdDict, data);

    std::cout << "Region X: " << regionXWorld << std::endl;
    std::cout << "Region Z: " << regionZWorld << std::endl;

    Region region = Region(std::move(data), regionXWorld, regionZWorld);
    return region;
}