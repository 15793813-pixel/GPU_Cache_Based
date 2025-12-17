#include "GASHashManager.h"

// XXHash64 核心常数
static const uint64_t PRIME64_1 = 11400714785056284775ULL;
static const uint64_t PRIME64_2 = 14020567174883302171ULL;
static const uint64_t PRIME64_3 = 1609587929392839161ULL;
static const uint64_t PRIME64_4 = 9650029242287828579ULL;
static const uint64_t PRIME64_5 = 2870177450012600261ULL;

static inline uint64_t RotateLeft64(uint64_t x, int r) {
    return (x << r) | (x >> (64 - r));
}

uint64_t CalculateXXHash64(const void* Data, size_t Length, uint64_t Seed) {
    const uint8_t* p = (const uint8_t*)Data;
    const uint8_t* const end = p + Length;
    uint64_t h64;

    if (Length >= 32) {
        const uint8_t* const limit = end - 32;
        uint64_t v1 = Seed + PRIME64_1 + PRIME64_2;
        uint64_t v2 = Seed + PRIME64_2;
        uint64_t v3 = Seed + 0;
        uint64_t v4 = Seed - PRIME64_1;

        do {
            v1 += (*(uint64_t*)p) * PRIME64_2; v1 = RotateLeft64(v1, 31); v1 *= PRIME64_1; p += 8;
            v2 += (*(uint64_t*)p) * PRIME64_2; v2 = RotateLeft64(v2, 31); v2 *= PRIME64_1; p += 8;
            v3 += (*(uint64_t*)p) * PRIME64_2; v3 = RotateLeft64(v3, 31); v3 *= PRIME64_1; p += 8;
            v4 += (*(uint64_t*)p) * PRIME64_2; v4 = RotateLeft64(v4, 31); v4 *= PRIME64_1; p += 8;
        } while (p <= limit);

        h64 = RotateLeft64(v1, 1) + RotateLeft64(v2, 7) + RotateLeft64(v3, 12) + RotateLeft64(v4, 18);

        v1 *= PRIME64_2; v1 = RotateLeft64(v1, 31); v1 *= PRIME64_1; h64 ^= v1;
        h64 = h64 * PRIME64_1 + PRIME64_4;
        v2 *= PRIME64_2; v2 = RotateLeft64(v2, 31); v2 *= PRIME64_1; h64 ^= v2;
        h64 = h64 * PRIME64_1 + PRIME64_4;
        v3 *= PRIME64_2; v3 = RotateLeft64(v3, 31); v3 *= PRIME64_1; h64 ^= v3;
        h64 = h64 * PRIME64_1 + PRIME64_4;
        v4 *= PRIME64_2; v4 = RotateLeft64(v4, 31); v4 *= PRIME64_1; h64 ^= v4;
        h64 = h64 * PRIME64_1 + PRIME64_4;
    }
    else {
        h64 = Seed + PRIME64_5;
    }

    h64 += (uint64_t)Length;

    while (p + 8 <= end) {
        uint64_t k1 = (*(uint64_t*)p) * PRIME64_2;
        k1 = RotateLeft64(k1, 31);
        k1 *= PRIME64_1;
        h64 ^= k1;
        h64 = RotateLeft64(h64, 27) * PRIME64_1 + PRIME64_4;
        p += 8;
    }

    if (p + 4 <= end) {
        h64 ^= (*(uint32_t*)p) * PRIME64_1;
        h64 = RotateLeft64(h64, 23) * PRIME64_2 + PRIME64_3;
        p += 4;
    }

    while (p < end) {
        h64 ^= (*p) * PRIME64_5;
        h64 = RotateLeft64(h64, 11) * PRIME64_1;
        p++;
    }

    // Final Mix (Avalanche)
    h64 ^= h64 >> 33;
    h64 *= PRIME64_2;
    h64 ^= h64 >> 29;
    h64 *= PRIME64_3;
    h64 ^= h64 >> 32;

    return h64;
}

uint64_t GenerateGUID64(const std::string& InString)
{
    if (InString.empty()) return 0;

    static boost::uuids::name_generator_sha1 Gen(boost::uuids::ns::dns());
    boost::uuids::uuid U = Gen(InString);
   
    uint64_t Data[2];
    std::memcpy(Data, U.data, 16); 

    return Data[0] ^ Data[1];
}