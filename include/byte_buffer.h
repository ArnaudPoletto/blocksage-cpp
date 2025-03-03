#pragma once

#include <vector>
#include <string>
#include <cstring>
#include <type_traits>
#include <stdexcept>

class ByteBuffer
{
public:
    ByteBuffer(const std::vector<char>& data) : data_(data), position_(0) {}
    ByteBuffer(std::vector<char>&& data) : data_(std::move(data)), position_(0) {}

    void seek(size_t position) { 
        if (position > data_.size()) {
            throw std::out_of_range("Seek position out of bounds");
        }
        position_ = position; 
    }

    template <typename T>
    T read()
    {
        if (position_ + sizeof(T) > data_.size()) {
            throw std::out_of_range("Read beyond buffer bounds");
        }

        T value;
        std::memcpy(&value, data_.data() + position_, sizeof(T));
        position_ += sizeof(T);

        // Convert from big-endian to host endian if needed
        if constexpr (sizeof(T) > 1 && std::is_integral_v<T>)
        {
            if constexpr (sizeof(T) == 2) { // 16-bit
                value = ((value & 0xFF00) >> 8) | 
                        ((value & 0x00FF) << 8);
            }
            else if constexpr (sizeof(T) == 4) { // 32-bit
                value = ((value & 0xFF000000) >> 24) |
                        ((value & 0x00FF0000) >> 8) |
                        ((value & 0x0000FF00) << 8) |
                        ((value & 0x000000FF) << 24);
            }
            else if constexpr (sizeof(T) == 8) { // 64-bit
                uint64_t hi = ((value & 0xFF00000000000000) >> 56) |
                              ((value & 0x00FF000000000000) >> 40) |
                              ((value & 0x0000FF0000000000) >> 24) |
                              ((value & 0x000000FF00000000) >> 8);
                uint64_t lo = ((value & 0x00000000FF000000) << 8) |
                              ((value & 0x0000000000FF0000) << 24) |
                              ((value & 0x000000000000FF00) << 40) |
                              ((value & 0x00000000000000FF) << 56);
                value = hi | lo;
            }
        }
        return value;
    }

    std::vector<char> read(size_t size)
    {
        if (position_ + size > data_.size()) {
            throw std::out_of_range("Read beyond buffer bounds");
        }

        std::vector<char> result(size);
        std::memcpy(result.data(), data_.data() + position_, size);
        position_ += size;
        return result;
    }

    std::string readString(size_t length)
    {
        if (position_ + length > data_.size()) {
            throw std::out_of_range("Read beyond buffer bounds");
        }

        std::string result(data_.data() + position_, length);
        position_ += length;
        return result;
    }

    template <typename T>
    std::vector<T> readVector(size_t length)
    {
        std::vector<T> result;
        result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            result.push_back(read<T>());
        }
        return result;
    }

    std::vector<int8_t> readByteArray(size_t length)
    {
        if (position_ + length > data_.size()) {
            throw std::out_of_range("Read beyond buffer bounds");
        }

        std::vector<int8_t> result(length);
        std::memcpy(result.data(), data_.data() + position_, length);
        position_ += length;
        return result;
    }

    size_t remaining() const {
        return data_.size() - position_;
    }

    size_t position() const {
        return position_;
    }

    const std::vector<char>& data() const { 
        return data_; 
    }

private:
    std::vector<char> data_;
    size_t position_;
};