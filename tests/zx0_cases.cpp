// The compressor on files of every kind, each decompressed back as data and
// in the CPC's memory: the shortest, runs of a byte, noise, matches at the
// furthest offset the format has and just beyond it, 64K; the data
// decompressed in place over the end of the compressed ones, `delta` bytes
// beyond; data that are not compressed ones; a compression stopped.

#include <algorithm>
#include <memory>
#include <random>

#include "check.h"
#include "zx0/zx0.h"

namespace {

using tuxcmp::zx0::Memory;

// Compressed, then decompressed both ways: true if the file came back.
bool roundTrip(const std::vector<uint8_t>& original, size_t* size = nullptr)
{
    const auto packed = tuxcmp::zx0::compress(original);
    if (!packed)
        return false;
    if (size)
        *size = packed->data.size();
    if (tuxcmp::zx0::decompress(packed->data) != original)
        return false;
    if (original.size() > 0x8000)
        return true;
    auto memory = std::make_unique<Memory>();
    memory->fill(0xAA);
    std::copy(packed->data.begin(), packed->data.end(), memory->begin() + 0x0100);
    const auto run = tuxcmp::zx0::depack(*memory, 0x0100, 0x8000);
    return run && run->written == static_cast<int>(original.size())
           && std::equal(original.begin(), original.end(), memory->begin() + 0x8000);
}

// In place: the compressed data end `delta` bytes after the decompressed.
// A destination of -1 is as high as they go, the compressed data ending
// with the memory.
bool inPlace(const std::vector<uint8_t>& original, int destination)
{
    const auto packed = tuxcmp::zx0::compress(original);
    if (!packed)
        return false;
    if (destination < 0)
        destination = 0x10000 - packed->delta - static_cast<int>(original.size());
    const int end = destination + static_cast<int>(original.size()) + packed->delta;
    const int source = end - static_cast<int>(packed->data.size());
    if (end > 0x10000)
        return false;
    auto memory = std::make_unique<Memory>();
    memory->fill(0);
    std::copy(packed->data.begin(), packed->data.end(), memory->begin() + source);
    const auto run = tuxcmp::zx0::depack(*memory, static_cast<uint16_t>(source), static_cast<uint16_t>(destination));
    return run && std::equal(original.begin(), original.end(), memory->begin() + destination);
}

std::vector<uint8_t> noise(size_t size, unsigned seed)
{
    std::mt19937 random(seed);
    std::vector<uint8_t> bytes(size);
    for (uint8_t& byte : bytes)
        byte = static_cast<uint8_t>(random());
    return bytes;
}

// Text-like: words of a few letters, repeated now and then.
std::vector<uint8_t> words(size_t size, unsigned seed)
{
    std::mt19937 random(seed);
    std::vector<uint8_t> bytes;
    while (bytes.size() < size) {
        if (bytes.size() > 64 && random() % 3 == 0) {
            const size_t back = 1 + random() % std::min<size_t>(bytes.size(), 4000);
            const size_t length = 2 + random() % 30;
            for (size_t i = 0; i < length && bytes.size() < size; ++i)
                bytes.push_back(bytes[bytes.size() - back]);
        } else {
            bytes.push_back(static_cast<uint8_t>('a' + random() % 26));
        }
    }
    return bytes;
}

}  // namespace

int main()
{
    // The shortest files.
    CHECK(roundTrip({0x42}));
    CHECK(roundTrip({0x00, 0x00}));
    CHECK(roundTrip({1, 2, 3}));
    size_t size = 0;
    CHECK(roundTrip({0x42}, &size) && size == 4);

    // Runs of one byte, of every short length and some long ones.
    for (const size_t length : {2, 3, 4, 7, 8, 9, 15, 16, 17, 127, 128, 129, 255, 256, 257, 1000, 16384})
        CHECK(roundTrip(std::vector<uint8_t>(length, 0x55)));
    CHECK(roundTrip(std::vector<uint8_t>(16384, 0x00), &size) && size < 40);

    // What does not compress: it comes out a little longer.
    const std::vector<uint8_t> random = noise(4096, 1);
    CHECK(roundTrip(random, &size) && size > 4096 && size < 4096 + 4096 / 64);
    CHECK(roundTrip(words(8000, 2)));
    CHECK(roundTrip(words(3000, 3)));

    // A block of 200 bytes repeated at the furthest offset there is: it
    // is found, and costs a few bytes. One byte further, it is not, and
    // costs its 200 bytes. (The noise around it costs a little more than
    // itself: 81 bytes over 32K.)
    for (const size_t gap : {32639, 32640, 32641}) {
        std::vector<uint8_t> far = noise(gap, 4);
        const std::vector<uint8_t> block = noise(200, 5);
        std::copy(block.begin(), block.end(), far.begin());
        far.insert(far.end(), block.begin(), block.end());
        CHECK(roundTrip(far, &size));
        if (gap <= 32640)
            CHECK(size < far.size() - 100);
        else
            CHECK(size > far.size() + 50);
    }

    // The whole of the CPC's memory: noise, with repeats here and there.
    std::vector<uint8_t> memory64 = noise(65536, 6);
    for (size_t at = 1000; at + 64 < memory64.size(); at += 1500)
        std::copy(memory64.begin() + static_cast<std::ptrdiff_t>(at - 900), memory64.begin() + static_cast<std::ptrdiff_t>(at - 836),
                  memory64.begin() + static_cast<std::ptrdiff_t>(at));
    CHECK(roundTrip(memory64, &size) && size < memory64.size());

    // In place, at the start of the memory, in the middle, to its end.
    for (const int destination : {0x0040, 0x4000, -1}) {
        CHECK(inPlace(words(5000, 7), destination));
        CHECK(inPlace(noise(2000, 8), destination));
        CHECK(inPlace(std::vector<uint8_t>(3000, 0xE5), destination));
    }
    CHECK(inPlace(words(16384, 9), 0xC000 - 16384));

    // Nothing to compress, or too much.
    CHECK(!tuxcmp::zx0::compress({}).has_value());
    CHECK(!tuxcmp::zx0::compress(std::vector<uint8_t>(tuxcmp::zx0::kMaxInput + 1, 0)).has_value());

    // Data that are not compressed ones, or end too soon.
    const auto packed = tuxcmp::zx0::compress(words(1000, 10));
    CHECK(packed.has_value());
    if (packed) {
        const std::vector<uint8_t> cut(packed->data.begin(), packed->data.begin() + 100);
        CHECK(!tuxcmp::zx0::decompress(cut).has_value());
        // More than asked for.
        CHECK(!tuxcmp::zx0::decompress(packed->data, 999).has_value());
        CHECK(tuxcmp::zx0::decompress(packed->data, 1000).has_value());
    }
    CHECK(!tuxcmp::zx0::decompress(std::vector<uint8_t>{0x00, 0x00, 0x00}).has_value());
    // In the memory, a run with no end is stopped.
    auto memory = std::make_unique<Memory>();
    memory->fill(0);
    CHECK(!tuxcmp::zx0::depack(*memory, 0x4000, 0x8000, 0x8000).has_value());

    // Stopped, it gives nothing; told how far it went until then.
    int calls = 0, last = -1, total = 0;
    const std::vector<uint8_t> text = words(8000, 11);
    const auto stopped = tuxcmp::zx0::compress(text, [&](int done, int all) {
        ++calls;
        last = done;
        total = all;
        return done < 4000;
    });
    CHECK(!stopped.has_value());
    CHECK(calls > 10 && last >= 4000 && last < 4300 && total == 8000);
    calls = 0;
    const auto whole = tuxcmp::zx0::compress(text, [&](int done, int all) {
        ++calls;
        last = done;
        return done <= all;
    });
    CHECK(whole.has_value() && last == 8000 && calls > 30);
    return checkSummary("zx0_cases");
}
