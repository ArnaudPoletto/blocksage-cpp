#pragma once

#include "byte_buffer.h"
#include <vector>
#include <string>
#include <unordered_map>

const int MAX_NBT_DEPTH = 512;

class NBTParser
{
public:
    enum class TagType {
        TagEnd = 0,
        TagByte = 1,
        TagShort = 2,
        TagInt = 3,
        TagLong = 4,
        TagFloat = 5,
        TagDouble = 6,
        TagByteArray = 7,
        TagString = 8,
        TagList = 9,
        TagCompound = 10,
        TagIntArray = 11,
        TagLongArray = 12
    };

    struct NBTTag {
        std::string name;
        TagType type;

        union {
            int8_t byteValue;
            int16_t shortValue;
            int32_t intValue;
            int64_t longValue;
            float floatValue;
            double doubleValue;
        };

        std::vector<int8_t> byteArrayValue;
        std::string stringValue;
        std::vector<NBTTag> listValue;
        std::unordered_map<std::string, NBTTag> compoundValue;
        std::vector<int32_t> intArrayValue;
        std::vector<uint64_t> longArrayValue;
    };

    static NBTTag parseTag(ByteBuffer &buffer, TagType type, bool named = true, int currentDepth = 0);
    static NBTTag parseNBT(ByteBuffer &buffer);
};