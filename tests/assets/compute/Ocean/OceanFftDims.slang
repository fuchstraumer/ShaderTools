implementing OceanFft;
// This file declares the extern permutation variables that control subgroup sizing (or how the shader
// interprets it to be set at least), the FFT dimensions, the number of fused stages in wave ops,
// wave cascade count, thread count per WG, and elements processed per thread.
extern const static uint IFFT_SIZE = 512u;
// unlikely we define this one differently
extern const static uint IFFT_NUM_WAVE_CASCADES = 4u;
// permutation parameter
extern const static bool IFFT_USE_WAVE_OPS = true;
// permutation parameter - I will not use wave ops when size < 32u, but we will permute this as 32,64,128
// surprising number of devices can do 128!
extern const static uint IFFT_WAVE_SIZE = 32u;

// these are no all downstream of the above
internal const static uint IFFT_LOG2_SIZE = firstbithigh(IFFT_SIZE);
internal const static uint IFFT_LOG4_SIZE = IFFT_LOG2_SIZE / 2u;
// get remainder so we know how many radix-2 stages we need after radix-4 stages
internal const static uint IFFT_LOG4_REMAINDER = IFFT_LOG2_SIZE % 2u;
// IFFT_LOG2_WAVE_SIZE: 5, IFFT_LOG4_WAVE_SIZE: 2
internal const static uint IFFT_LOG2_WAVE_SIZE = IFFT_USE_WAVE_OPS ? firstbithigh(IFFT_WAVE_SIZE) : 0u;
internal const static uint IFFT_LOG4_WAVE_SIZE = IFFT_USE_WAVE_OPS ? 2u : 0u;
