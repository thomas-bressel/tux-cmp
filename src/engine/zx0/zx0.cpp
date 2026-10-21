#include "zx0/zx0.h"

#include <algorithm>

// The compressor is zx0's own (optimize.c, compress.c and memory.c, version
// 1, by Einar Saukas, under the BSD 3-clause licence), as PackZX0 has it:
// the same search for the cheapest way to say each byte, the same bits.

namespace tuxcmp::zx0 {

namespace {

constexpr int kInitialOffset = 1;

int eliasGammaBits(int value)
{
    int bits = 1;
    while (value >>= 1)
        bits += 2;
    return bits;
}

int offsetCeiling(int index, int limit)
{
    return index > limit ? limit : index < kInitialOffset ? kInitialOffset : index;
}

// What the file is made of, as far as one of its bytes: literals or a copy
// (`offset` back), and the cheapest way there before it (`chain`). The
// blocks are shared by the ways that go through them, and counted: one no
// way goes through any more is used again, as memory.c does, which keeps
// the millions the search makes to a few thousand at a time.
struct Block {
    int bits = 0;
    int index = 0;
    int offset = 0;
    int chain = -1;
    int references = 0;
    int ghostChain = -1;
};

class Blocks {
public:
    int allocate(int bits, int index, int offset, int chain)
    {
        int at;
        if (ghostRoot_ >= 0) {
            at = ghostRoot_;
            ghostRoot_ = pool_[at].ghostChain;
            const int before = pool_[at].chain;
            if (before >= 0 && --pool_[before].references == 0) {
                pool_[before].ghostChain = ghostRoot_;
                ghostRoot_ = before;
            }
        } else {
            at = static_cast<int>(pool_.size());
            pool_.emplace_back();
        }
        if (chain >= 0)
            ++pool_[chain].references;
        pool_[at] = Block{bits, index, offset, chain, 0, -1};
        return at;
    }

    void assign(int& slot, int block)
    {
        ++pool_[block].references;
        if (slot >= 0 && --pool_[slot].references == 0) {
            pool_[slot].ghostChain = ghostRoot_;
            ghostRoot_ = slot;
        }
        slot = block;
    }

    Block& operator[](int at) { return pool_[static_cast<size_t>(at)]; }

private:
    std::vector<Block> pool_;
    int ghostRoot_ = -1;
};

// The cheapest way to the file's last byte; -1 if the search was stopped.
int optimize(Blocks& blocks, std::span<const uint8_t> input, int offsetLimit, const Progress& progress)
{
    const int size = static_cast<int>(input.size());
    const int maxOffset = offsetCeiling(size - 1, offsetLimit);
    const auto slots = static_cast<size_t>(maxOffset + 1);
    std::vector<int> lastLiteral(slots, -1);
    std::vector<int> lastMatch(slots, -1);
    std::vector<int> optimal(static_cast<size_t>(size), -1);
    std::vector<int> matchLength(slots, 0);
    std::vector<int> bestLength(static_cast<size_t>(size) + 3, 0);
    bestLength[2] = 2;

    // A block before the first byte, from which the first literals start.
    blocks.assign(lastMatch[kInitialOffset], blocks.allocate(-1, -1, kInitialOffset, -1));

    for (int index = 0; index < size; ++index) {
        if (progress && (index & 0xFF) == 0 && !progress(index, size))
            return -1;
        int bestLengthSize = 2;
        const int ceiling = offsetCeiling(index, offsetLimit);
        for (int offset = 1; offset <= ceiling; ++offset) {
            if (index != 0 && index >= offset && input[index] == input[index - offset]) {
                // A copy from the last offset.
                if (lastLiteral[offset] >= 0) {
                    const int length = index - blocks[lastLiteral[offset]].index;
                    const int bits = blocks[lastLiteral[offset]].bits + 1 + eliasGammaBits(length);
                    blocks.assign(lastMatch[offset], blocks.allocate(bits, index, offset, lastLiteral[offset]));
                    if (optimal[index] < 0 || blocks[optimal[index]].bits > bits)
                        blocks.assign(optimal[index], lastMatch[offset]);
                }
                // A copy from a new offset.
                if (++matchLength[offset] > 1) {
                    if (bestLengthSize < matchLength[offset]) {
                        int bits = blocks[optimal[index - bestLength[bestLengthSize]]].bits
                                   + eliasGammaBits(bestLength[bestLengthSize] - 1);
                        do {
                            ++bestLengthSize;
                            const int bits2 = blocks[optimal[index - bestLengthSize]].bits + eliasGammaBits(bestLengthSize - 1);
                            if (bits2 <= bits) {
                                bestLength[bestLengthSize] = bestLengthSize;
                                bits = bits2;
                            } else {
                                bestLength[bestLengthSize] = bestLength[bestLengthSize - 1];
                            }
                        } while (bestLengthSize < matchLength[offset]);
                    }
                    const int length = bestLength[matchLength[offset]];
                    const int bits = blocks[optimal[index - length]].bits + 8 + eliasGammaBits((offset - 1) / 128 + 1)
                                     + eliasGammaBits(length - 1);
                    if (lastMatch[offset] < 0 || blocks[lastMatch[offset]].index != index || blocks[lastMatch[offset]].bits > bits) {
                        blocks.assign(lastMatch[offset], blocks.allocate(bits, index, offset, optimal[index - length]));
                        if (optimal[index] < 0 || blocks[optimal[index]].bits > bits)
                            blocks.assign(optimal[index], lastMatch[offset]);
                    }
                }
            } else {
                // Literals.
                matchLength[offset] = 0;
                if (lastMatch[offset] >= 0) {
                    const int length = index - blocks[lastMatch[offset]].index;
                    const int bits = blocks[lastMatch[offset]].bits + 1 + eliasGammaBits(length) + length * 8;
                    blocks.assign(lastLiteral[offset], blocks.allocate(bits, index, 0, lastMatch[offset]));
                    if (optimal[index] < 0 || blocks[optimal[index]].bits > bits)
                        blocks.assign(optimal[index], lastLiteral[offset]);
                }
            }
        }
    }
    if (progress)
        progress(size, size);
    return optimal[static_cast<size_t>(size - 1)];
}

// The bits and bytes of the compressed data, as compress.c writes them.
class Writer {
public:
    Writer(int outputSize, int inputSize)
        : diff_(outputSize - inputSize)
    {
        out_.reserve(static_cast<size_t>(outputSize));
    }

    void byte(int value)
    {
        out_.push_back(static_cast<uint8_t>(value));
        --diff_;
    }

    // The first bit after an offset's low byte goes in that byte's lowest
    // bit, as does the first literals' indicator, which is not written.
    void bit(bool value)
    {
        if (backtrack_) {
            if (value && !out_.empty())
                out_.back() |= 1;
            backtrack_ = false;
            return;
        }
        if (!mask_) {
            mask_ = 0x80;
            at_ = out_.size();
            byte(0);
        }
        if (value)
            out_[at_] |= static_cast<uint8_t>(mask_);
        mask_ >>= 1;
    }

    void backtrack() { backtrack_ = true; }

    // Interlaced Elias gamma: before each bit of the number but its first,
    // a 0; after the last, a 1.
    void gamma(int value)
    {
        int i = 2;
        while (i <= value)
            i <<= 1;
        i >>= 1;
        while (i >>= 1) {
            bit(false);
            bit(value & i);
        }
        bit(true);
    }

    // The bytes of the file the data now stand for.
    void consumed(int count)
    {
        diff_ += count;
        delta_ = std::max(delta_, diff_);
    }

    std::vector<uint8_t>& data() { return out_; }
    int delta() const { return delta_; }

private:
    std::vector<uint8_t> out_;
    size_t at_ = 0;
    int mask_ = 0;
    bool backtrack_ = true;
    int diff_;
    int delta_ = 0;
};

// The bits DepackZX0 reads: register A, a byte's bits with a marker after
// them, and the carry. The routine fetches a byte only when it reads an
// Elias gamma number's stop bit, never for the other bits: the format sees
// to it that those are never the first of a byte.
struct Bits {
    uint8_t a = 0x80;

    // add a,a / jr nz / ld a,(hl) / inc hl / rla
    template <class Source>
    bool fetching(Source& source)
    {
        bool carry = a & 0x80;
        a = static_cast<uint8_t>(a << 1);
        if (a == 0) {
            const uint8_t next = source.next();
            const bool high = next & 0x80;
            a = static_cast<uint8_t>(next << 1 | carry);
            carry = high;
        }
        return carry;
    }

    // add a,a
    bool plain()
    {
        const bool carry = a & 0x80;
        a = static_cast<uint8_t>(a << 1);
        return carry;
    }

    // dzx0s_elias_loop, from a number begun.
    template <class Source>
    uint16_t elias(Source& source, uint16_t value)
    {
        while (!fetching(source) && !source.failed())
            value = static_cast<uint16_t>(value << 1 | plain());
        return value;
    }
};

// DepackZX0, step by step, on a machine that reads bytes and writes them.
// `Machine` has next(), failed(), put(byte) and copy(offset, length), the
// offset a negative one in 16 bits as the routine keeps it. False if the
// machine failed before the end of the data was read.
template <class Machine>
bool unpack(Machine& machine)
{
    Bits bits;
    uint16_t offset = 0xFFFF;  // -1: the default offset
    bool literals = true;      // what comes next: literals, or a copy from a new offset
    for (;;) {
        if (literals) {
            const uint16_t count = bits.elias(machine, 1);
            for (int i = 0; i < (count ? count : 0x10000) && !machine.failed(); ++i)
                machine.put(machine.next());
            if (machine.failed())
                return false;
            // A copy from the last offset, or a new one?
            if (!bits.plain()) {
                const uint16_t length = bits.elias(machine, 1);
                if (machine.failed() || !machine.copy(offset, length))
                    return false;
                if (!bits.plain())
                    continue;
            }
        }
        // The offset's high bits, made negative; 256 ends the data.
        const uint16_t high = bits.elias(machine, 1);
        if (machine.failed())
            return false;
        const auto c = static_cast<uint8_t>(high);
        if (c == 0)
            return true;
        const auto b = static_cast<uint8_t>(-c);
        const uint8_t low = machine.next();
        // rr b / rr c, the carry set by the subtraction that made B.
        const auto newB = static_cast<uint8_t>(0x80 | b >> 1);
        const auto newC = static_cast<uint8_t>((b & 1) << 7 | low >> 1);
        offset = static_cast<uint16_t>(newB << 8 | newC);
        // The length less one, its first stop bit the low byte's last bit.
        uint16_t length = 1;
        if (!(low & 1)) {
            length = static_cast<uint16_t>(length << 1 | bits.plain());
            length = bits.elias(machine, length);
        }
        length = static_cast<uint16_t>(length + 1);
        if (machine.failed() || !machine.copy(offset, length))
            return false;
        literals = !bits.plain();
    }
}

// From data to bytes of their own.
struct Unpacker {
    std::span<const uint8_t> in;
    size_t at = 0;
    size_t limit = 0;
    bool broken = false;
    std::vector<uint8_t> out;

    uint8_t next()
    {
        if (at >= in.size()) {
            broken = true;
            return 0;
        }
        return in[at++];
    }
    bool failed() const { return broken; }
    void put(uint8_t value)
    {
        if (out.size() >= limit)
            broken = true;
        else
            out.push_back(value);
    }
    bool copy(uint16_t offset, uint16_t length)
    {
        const size_t back = 0x10000 - offset;
        const size_t count = length ? length : 0x10000;
        if (back > out.size() || out.size() + count > limit)
            return !(broken = true);
        for (size_t i = 0; i < count; ++i) {
            const uint8_t value = out[out.size() - back];
            out.push_back(value);
        }
        return true;
    }
};

// In the CPC's memory, as the Z80 does it.
struct InMemory {
    Memory& memory;
    uint16_t hl;
    uint16_t de;
    int limit;
    int written = 0;
    int read = 0;
    bool broken = false;

    uint8_t next()
    {
        // More than the memory holds: the data have no end.
        if (++read > 0x10000)
            broken = true;
        return memory[hl++];
    }
    bool failed() const { return broken; }
    void put(uint8_t value)
    {
        if (++written > limit)
            broken = true;
        else
            memory[de++] = value;
    }
    bool copy(uint16_t offset, uint16_t length)
    {
        uint16_t from = static_cast<uint16_t>(de + offset);
        for (int i = 0; i < (length ? length : 0x10000) && !broken; ++i) {
            if (++written > limit)
                broken = true;
            else
                memory[de++] = memory[from++];
        }
        return !broken;
    }
};

}  // namespace

std::optional<Packed> compress(std::span<const uint8_t> input, const Progress& progress)
{
    if (input.empty() || input.size() > static_cast<size_t>(kMaxInput))
        return std::nullopt;
    Blocks blocks;
    int optimal = optimize(blocks, input, kMaxOffset, progress);
    if (optimal < 0)
        return std::nullopt;

    // The 18 bits of the end, and what makes up the last byte.
    const int outputSize = (blocks[optimal].bits + 18 + 7) / 8;
    // The way, from its first block to its last.
    int previous = -1;
    while (optimal >= 0) {
        const int next = blocks[optimal].chain;
        blocks[optimal].chain = previous;
        previous = optimal;
        optimal = next;
    }

    Writer writer(outputSize, static_cast<int>(input.size()));
    int inputIndex = 0;
    int lastOffset = kInitialOffset;
    for (int block = blocks[previous].chain; block >= 0; previous = block, block = blocks[block].chain) {
        const Block& here = blocks[block];
        const int length = here.index - blocks[previous].index;
        if (here.offset == 0) {
            // Literals: their indicator, their count, the bytes.
            writer.bit(false);
            writer.gamma(length);
            for (int i = 0; i < length; ++i) {
                writer.byte(input[static_cast<size_t>(inputIndex++)]);
                writer.consumed(1);
            }
        } else if (here.offset == lastOffset) {
            // A copy from the last offset: its indicator, its length.
            writer.bit(false);
            writer.gamma(length);
            inputIndex += length;
            writer.consumed(length);
        } else {
            // A copy from a new offset: its indicator, the offset's high
            // part, its low part, the length less one.
            writer.bit(true);
            writer.gamma((here.offset - 1) / 128 + 1);
            writer.byte((127 - (here.offset - 1) % 128) << 1);
            writer.backtrack();
            writer.gamma(length - 1);
            inputIndex += length;
            writer.consumed(length);
            lastOffset = here.offset;
        }
    }
    // The end: a new offset of 256 times 128.
    writer.bit(true);
    writer.gamma(256);

    Packed packed;
    packed.data = std::move(writer.data());
    packed.delta = writer.delta();
    if (static_cast<int>(packed.data.size()) != outputSize || inputIndex != static_cast<int>(input.size()))
        return std::nullopt;
    return packed;
}

std::optional<std::vector<uint8_t>> decompress(std::span<const uint8_t> packed, size_t limit)
{
    Unpacker unpacker{packed, 0, limit, false, {}};
    if (!unpack(unpacker))
        return std::nullopt;
    return std::move(unpacker.out);
}

std::optional<Run> depack(Memory& memory, uint16_t source, uint16_t destination, int limit)
{
    InMemory machine{memory, source, destination, limit};
    if (!unpack(machine))
        return std::nullopt;
    return Run{machine.hl, machine.de, machine.written};
}

}  // namespace tuxcmp::zx0
