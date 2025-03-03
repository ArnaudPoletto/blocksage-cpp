#include "nbt_parser.h"
#include <iostream>

NBTParser::NBTTag NBTParser::parseTag(ByteBuffer &buffer, TagType type, bool named, int currentDepth) {
    if (currentDepth > MAX_NBT_DEPTH) {
        throw std::runtime_error("NBT depth exceeds maximum depth");
    }

    NBTTag tag;
    tag.type = type;

    if (named) {
        uint8_t nameLength1 = buffer.read<uint8_t>();
        uint8_t nameLength2 = buffer.read<uint8_t>();
        uint16_t nameLength = (static_cast<uint16_t>(nameLength1) << 8) | (static_cast<uint16_t>(nameLength2));
        if (nameLength == 0) {
            tag.name = "";
        }else{
            tag.name = buffer.readString(nameLength);
        }
    }

    switch (type) {
        case TagType::TagEnd:
            break;
        case TagType::TagByte:
            tag.byteValue = buffer.read<int8_t>();
            break;
        case TagType::TagShort:
            tag.shortValue = buffer.read<int16_t>();
            break;
        case TagType::TagInt:
            tag.intValue = buffer.read<int32_t>();
            break;
        case TagType::TagLong:
            tag.longValue = buffer.read<int64_t>();
            break;
        case TagType::TagFloat:
            tag.floatValue = buffer.read<float>();
            break;
        case TagType::TagDouble:
            tag.doubleValue = buffer.read<double>();
            break;
        case TagType::TagByteArray: {
            int32_t byteArrayLength = buffer.read<int32_t>();
            tag.byteArrayValue = buffer.readByteArray(byteArrayLength);
            break;
        }
        case TagType::TagString: {
            int16_t stringLength = buffer.read<int16_t>();
            tag.stringValue = buffer.readString(stringLength);
            break;
        }
        case TagType::TagList: {
            TagType listType = static_cast<TagType>(buffer.read<int8_t>());
            int32_t listLength = buffer.read<int32_t>();
            for (int i = 0; i < listLength; ++i) {
                tag.listValue.push_back(parseTag(buffer, listType, false, currentDepth + 1));
            }
            break;
        }
        case TagType::TagCompound: {
            while (true) {
                TagType compoundType = static_cast<TagType>(buffer.read<uint8_t>());
                if (compoundType == TagType::TagEnd) {
                    break;
                }
                NBTTag compound = parseTag(buffer, compoundType, true, currentDepth + 1);
                tag.compoundValue[compound.name] = compound;
            }
            break;
        }
        case TagType::TagIntArray: {
            int32_t intArrayLength = buffer.read<int32_t>();
            tag.intArrayValue.resize(intArrayLength);
            for (int i = 0; i < intArrayLength; i++) {
                tag.intArrayValue[i] = buffer.read<int32_t>();
            }
            break;
        }
        case TagType::TagLongArray: {
            uint32_t longArrayLength = buffer.read<uint32_t>();
            tag.longArrayValue.resize(longArrayLength);
            for (int i = 0; i < longArrayLength; i++) {
                tag.longArrayValue[i] = buffer.read<uint64_t>();
            }
            break;
        }
    }

    return tag;
}

NBTParser::NBTTag NBTParser::parseNBT(ByteBuffer &buffer) {
    TagType rootType = static_cast<TagType>(buffer.read<uint8_t>());
    if (rootType != TagType::TagCompound) {
        throw std::runtime_error("Root tag must be a compound tag");
    }

    return parseTag(buffer, rootType);
}