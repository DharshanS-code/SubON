#include "../src/WavFile.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void append16(std::string& bytes, std::uint16_t value) {
    bytes.push_back(static_cast<char>(value));
    bytes.push_back(static_cast<char>(value >> 8));
}
void append32(std::string& bytes, std::uint32_t value) {
    append16(bytes, static_cast<std::uint16_t>(value));
    append16(bytes, static_cast<std::uint16_t>(value >> 16));
}
void set32(std::string& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = static_cast<char>(value >> (8 * i));
}
std::string chunk(const char* id, const std::string& payload) {
    std::string result(id, 4);
    append32(result, static_cast<std::uint32_t>(payload.size()));
    result += payload;
    if (payload.size() % 2) result += '\0';
    return result;
}
std::string wav(unsigned bits = 16, unsigned tag = 1, unsigned channels = 2,
                unsigned rate = 48000, bool dataFirst = false) {
    std::string format;
    append16(format, static_cast<std::uint16_t>(tag));
    append16(format, static_cast<std::uint16_t>(channels));
    append32(format, rate);
    append32(format, rate * channels * bits / 8);
    append16(format, static_cast<std::uint16_t>(channels * bits / 8));
    append16(format, static_cast<std::uint16_t>(bits));
    const auto fmt = chunk("fmt ", format);
    const auto data = chunk("data", std::string(channels * bits / 8, '\0'));
    const auto chunks = chunk("JUNK", "odd") + (dataFirst ? data + fmt : fmt + data);
    std::string result = "RIFF";
    append32(result, static_cast<std::uint32_t>(4 + chunks.size()));
    return result + "WAVE" + chunks;
}
subon::desktop::WavData parse(const std::string& bytes) {
    std::istringstream stream(bytes, std::ios::binary);
    return subon::desktop::readWav(stream);
}
void rejects(const std::string& bytes, const char* message) {
    bool rejected = false;
    try { (void)parse(bytes); } catch (const subon::desktop::WavError&) { rejected = true; }
    require(rejected, message);
}
void formats() {
    for (unsigned channels : {1u, 2u}) {
        for (unsigned rate : {32001u, 44100u, 48000u, 192000u}) {
            for (unsigned bits : {16u, 24u, 32u}) {
                for (bool dataFirst : {false, true}) {
                    const auto decoded = parse(wav(bits, 1, channels, rate, dataFirst));
                    require(decoded.format.bitsPerSample == bits, "PCM width lost");
                    require(decoded.format.channels == channels, "channel count lost");
                    require(decoded.format.sampleRate == rate, "sample rate lost");
                    require(decoded.pcm.size() == channels * bits / 8, "audio size changed");
                }
            }
            require(parse(wav(32, 3, channels, rate)).format.tag == 3, "float format lost");
        }
    }
}
void malformed() {
    auto good = wav();
    for (std::size_t size = 0; size < good.size(); ++size)
        rejects(good.substr(0, size), "truncated file accepted");
    rejects(wav(8), "8-bit PCM accepted");
    rejects(wav(16, 1, 3), "surround accepted");
    rejects(wav(16, 1, 1, 22050), "unsupported sample rate accepted");
    rejects(wav(16, 1, 1, 32000), "Nyquist-boundary rate accepted by loader but rejected by DSP");
    rejects(wav(16, 1, 1, 192001), "sample rate above player limit accepted");
    rejects(wav(16, 3), "16-bit float accepted");
    rejects(wav(32, 65534), "unsupported extensible format accepted");
    auto broken = good;
    broken[0] = 'X';
    rejects(broken, "non-RIFF file accepted");
    broken = good;
    set32(broken, 4, 0xffffffffu);
    rejects(broken, "oversize RIFF accepted");
    broken = good;
    set32(broken, 16, 0xffffffffu);
    rejects(broken, "out-of-bounds chunk accepted");
    broken = good;
    set32(broken, 40, 1); // fmt byte rate, after odd JUNK chunk
    rejects(broken, "inconsistent byte rate accepted");
    broken = good;
    broken[44] = 1; // block alignment
    rejects(broken, "inconsistent block alignment accepted");
    broken = good;
    set32(broken, 52, 0); // empty data, with trailing bytes inside RIFF
    rejects(broken, "empty audio accepted");
    broken = good;
    set32(broken, 52, 3);
    rejects(broken, "partial audio frame accepted");
    broken = good;
    broken.replace(24, 4, "JUNK");
    rejects(broken, "missing format accepted");
    broken = good;
    broken.replace(48, 4, "JUNK");
    rejects(broken, "missing audio accepted");
    broken = good + good.substr(24, 24);
    set32(broken, 4, static_cast<std::uint32_t>(broken.size() - 8));
    rejects(broken, "duplicate format accepted");
    broken = good + good.substr(48);
    set32(broken, 4, static_cast<std::uint32_t>(broken.size() - 8));
    rejects(broken, "duplicate data accepted");
    broken = good + "trailer";
    set32(broken, 4, static_cast<std::uint32_t>(broken.size() - 8));
    rejects(broken, "incomplete trailing chunk accepted");
    // Bytes outside the declared RIFF are not interpreted as chunks.
    require(parse(good + "unrelated trailing bytes").pcm.size() == 4, "valid RIFF boundary rejected");
}
void samples() {
    for (unsigned bits : {16u, 24u, 32u}) {
        const auto format = parse(wav(bits)).format;
        for (double value : {-1.0, -0.5, 0.0, 0.5, 1.0}) {
            std::uint8_t bytes[4]{};
            subon::desktop::encodeSample(bytes, format, value);
            const double decoded = subon::desktop::decodeSample(bytes, format);
            require(std::abs(decoded - value) < 1.0 / 16000, "PCM round trip changed sample");
        }
        std::uint8_t bytes[4]{};
        subon::desktop::encodeSample(bytes, format, std::numeric_limits<double>::quiet_NaN());
        require(subon::desktop::decodeSample(bytes, format) == 0, "NaN output not silenced");
        subon::desktop::encodeSample(bytes, format, 100.0);
        require(subon::desktop::decodeSample(bytes, format) <= 1, "PCM output not bounded");
    }
    const auto format = parse(wav(32, 3)).format;
    std::uint8_t bytes[4]{};
    const float nan = std::numeric_limits<float>::quiet_NaN();
    std::memcpy(bytes, &nan, 4);
    require(subon::desktop::decodeSample(bytes, format) == 0, "NaN input not silenced");
    subon::desktop::encodeSample(bytes, format, -0.25);
    require(subon::desktop::decodeSample(bytes, format) == -0.25, "float round trip failed");
}
}
int main() {
    try {
        formats();
        malformed();
        samples();
        std::cout << "WAV format, bounds, truncation and sample conversion tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
