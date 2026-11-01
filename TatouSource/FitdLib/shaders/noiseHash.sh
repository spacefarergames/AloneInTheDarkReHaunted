// Integer avalanche hash: no sine approximation or large floating-point
// products that can turn nearby scanlines into correlated moving bands.
// Also compiled directly by the CPU regression test (uint is uint32_t there).
uint pixelNoiseHash(uint x, uint y, uint seed)
{
    uint h = x * 0x9e3779b9u ^ y * 0x85ebca6bu ^ seed * 0xc2b2ae35u;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return h;
}

float pixelNoise(uint x, uint y, uint seed)
{
    // Top 24 bits are exactly representable in a shader float, in [0, 1).
    return float(pixelNoiseHash(x, y, seed) >> 8) * (1.0 / 16777216.0);
}
