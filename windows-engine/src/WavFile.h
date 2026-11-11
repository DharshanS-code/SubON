#pragma once

// Desktop WAV decoding has no Windows dependencies so malformed-file handling
// and every supported sample representation can be tested on the host.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <istream>
#include <stdexcept>
#include <vector>

namespace subon::desktop {
struct WavFormat {
    std::uint16_t tag{};
    std::uint16_t channels{};
    std::uint32_t sampleRate{};
    std::uint32_t byteRate{};
    std::uint16_t blockAlign{};
    std::uint16_t bitsPerSample{};
};
struct WavData {
    WavFormat format{};
    std::vector<std::uint8_t> pcm;
};
class WavError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
namespace detail {
inline std::uint16_t le16(const std::uint8_t* bytes) noexcept {
    return static_cast<std::uint16_t>(bytes[0] | (std::uint16_t{bytes[1]} << 8));
}
inline std::uint32_t le32(const std::uint8_t* bytes) noexcept {
    return std::uint32_t{bytes[0]} | (std::uint32_t{bytes[1]} << 8) |
           (std::uint32_t{bytes[2]} << 16) | (std::uint32_t{bytes[3]} << 24);
}
inline void read(std::istream& input, std::uint8_t* bytes, std::size_t size) {
    if (size != 0 && !input.read(reinterpret_cast<char*>(bytes), static_cast<std::streamsize>(size)))
        throw WavError("WAV file is truncated or could not be read.");
}
}
inline WavData readWav(std::istream& input) {
    std::array<std::uint8_t, 12> header{};
    detail::read(input, header.data(), header.size());
    const auto riffSize = detail::le32(header.data() + 4);
    if (std::memcmp(header.data(), "RIFF", 4) != 0 ||
        std::memcmp(header.data() + 8, "WAVE", 4) != 0 || riffSize < 4)
        throw WavError("Choose a valid RIFF/WAVE file.");
    input.seekg(0, std::ios::end);
    const auto fileEnd = input.tellg();
    const std::uint64_t riffEnd = 8ull + riffSize;
    if (fileEnd == std::streampos(-1) ||
        riffEnd > static_cast<std::uint64_t>(static_cast<std::streamoff>(fileEnd)))
        throw WavError("WAV file is truncated or has an invalid RIFF size.");
    input.seekg(12, std::ios::beg);
    WavData result;
    bool hasFormat = false;
    bool hasData = false;
    std::uint64_t position = 12;
    while (position < riffEnd) {
        if (riffEnd - position < 8) throw WavError("Incomplete WAV chunk header.");
        std::array<std::uint8_t, 8> chunk{};
        detail::read(input, chunk.data(), chunk.size());
        const auto size = detail::le32(chunk.data() + 4);
        position += 8;
        const std::uint64_t paddedSize = std::uint64_t{size} + (size & 1u);
        if (paddedSize > riffEnd - position)
            throw WavError("WAV file contains a truncated or out-of-bounds chunk.");
        if (std::memcmp(chunk.data(), "fmt ", 4) == 0) {
            if (hasFormat || size < 16 || size > 4096)
                throw WavError("Unsupported or duplicate WAV format chunk.");
            std::array<std::uint8_t, 16> format{};
            detail::read(input, format.data(), format.size());
            result.format = {detail::le16(format.data()), detail::le16(format.data() + 2),
                detail::le32(format.data() + 4), detail::le32(format.data() + 8),
                detail::le16(format.data() + 12), detail::le16(format.data() + 14)};
            hasFormat = true;
        } else if (std::memcmp(chunk.data(), "data", 4) == 0) {
            if (hasData) throw WavError("Multiple WAV audio chunks are not supported.");
            if (size > 512u * 1024u * 1024u)
                throw WavError("File is too large (maximum audio data: 512 MB).");
            result.pcm.resize(size);
            detail::read(input, result.pcm.data(), result.pcm.size());
            hasData = true;
        }
        position += paddedSize;
        input.seekg(static_cast<std::streamoff>(position), std::ios::beg);
        if (!input) throw WavError("Could not seek to the next WAV chunk.");
    }
    if (!hasFormat || !hasData) throw WavError("WAV file is missing format or audio data.");
    const auto& format = result.format;
    const bool pcm = format.tag == 1 && (format.bitsPerSample == 16 ||
        format.bitsPerSample == 24 || format.bitsPerSample == 32);
    const bool floating = format.tag == 3 && format.bitsPerSample == 32;
    const unsigned bytesPerSample = format.bitsPerSample / 8;
    if (format.channels < 1 || format.channels > 2 || format.sampleRate <= 32000 ||
        format.sampleRate > 192000 || !(pcm || floating) || !format.blockAlign ||
        format.blockAlign != format.channels * bytesPerSample ||
        format.byteRate != format.sampleRate * format.blockAlign ||
        result.pcm.empty() || result.pcm.size() % format.blockAlign != 0)
        throw WavError("Unsupported or inconsistent WAV format. Use mono/stereo PCM "
                       "16/24/32-bit or float32, above 32 through 192 kHz.");
    return result;
}
inline WavData readWav(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw WavError("Could not open that file. Check its location and permissions.");
    return readWav(input);
}
inline double decodeSample(const std::uint8_t* bytes, const WavFormat& format) noexcept {
    if (format.bitsPerSample != 16 && format.bitsPerSample != 24 && format.bitsPerSample != 32) return 0.0;
    if (format.tag == 3) {
        const auto bits = detail::le32(bytes);
        float value{};
        static_assert(sizeof(value) == sizeof(bits), "32-bit IEEE float is required");
        std::memcpy(&value, &bits, sizeof(value));
        return std::isfinite(value) ? value : 0.0;
    }
    const auto width = format.bitsPerSample;
    std::uint32_t value = detail::le16(bytes);
    if (width >= 24) value |= std::uint32_t{bytes[2]} << 16;
    if (width == 32) value |= std::uint32_t{bytes[3]} << 24;
    const std::int64_t signedValue = (value & (std::uint32_t{1} << (width - 1)))
        ? static_cast<std::int64_t>(value) - (std::int64_t{1} << width) : value;
    return static_cast<double>(signedValue) / static_cast<double>(std::uint64_t{1} << (width - 1));
}
inline void encodeSample(std::uint8_t* bytes, const WavFormat& format, double value) noexcept {
    if (format.bitsPerSample != 16 && format.bitsPerSample != 24 && format.bitsPerSample != 32) return;
    value = std::isfinite(value) ? std::clamp(value, -1.0, 1.0) : 0.0;
    std::uint32_t bits{};
    if (format.tag == 3) {
        const auto sample = static_cast<float>(value);
        std::memcpy(&bits, &sample, sizeof(bits));
    } else {
        const auto peak = (std::int64_t{1} << (format.bitsPerSample - 1)) - 1;
        bits = static_cast<std::uint32_t>(std::llround(value * static_cast<double>(peak)));
    }
    for (unsigned i = 0; i < static_cast<unsigned>(format.bitsPerSample) / 8u; ++i)
        bytes[i] = static_cast<std::uint8_t>(bits >> (8 * i));
}
} // namespace subon::desktop
