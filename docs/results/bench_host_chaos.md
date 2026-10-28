CPU: Intel(R) Xeon(R) Processor @ 2.10GHz (x86_64, 4 threads)  
Compiler: gcc 13.3.0  
Build: optimised (NDEBUG)  
SIMD backend: sse2  
8 s measured per run (+1.5 s warm-up), median of 3 runs, 4 voices, 48 kHz stereo float, chaos 0.50 (reverb/flanger macro running)

| Grains (target / active) | Callback | Interp. | Kernel | mean µs | p99 µs | max µs | load mean | load p99 | ns / grain-frame |
|---:|---:|---|---|---:|---:|---:|---:|---:|---:|
| 64 / 64 | 128 | linear | sse2 | 38.4 | 67.3 | 173.7 | 1.4 % | 2.5 % | 4.69 |
| 64 / 64 | 128 | Hermite | sse2 | 46.1 | 78.6 | 459.0 | 1.7 % | 2.9 % | 5.63 |
| 128 / 128 | 128 | linear | sse2 | 53.7 | 78.8 | 560.4 | 2.0 % | 3.0 % | 3.28 |
| 128 / 128 | 128 | Hermite | sse2 | 85.9 | 151.3 | 685.9 | 3.2 % | 5.7 % | 5.25 |
| 256 / 256 | 128 | linear | sse2 | 104.4 | 188.4 | 337.3 | 3.9 % | 7.1 % | 3.19 |
| 256 / 256 | 128 | Hermite | sse2 | 159.7 | 212.8 | 1105.4 | 6.0 % | 8.0 % | 4.88 |
| 512 / 505 | 128 | linear | sse2 | 200.2 | 302.1 | 3033.0 | 7.5 % | 11.3 % | 3.10 |
| 512 / 505 | 128 | Hermite | sse2 | 326.3 | 624.9 | 4567.9 | 12.2 % | 23.4 % | 5.05 |
| 64 / 64 | 192 | linear | sse2 | 49.1 | 108.2 | 1007.5 | 1.2 % | 2.7 % | 4.00 |
| 64 / 64 | 192 | Hermite | sse2 | 64.1 | 83.8 | 126.4 | 1.6 % | 2.1 % | 5.22 |
| 128 / 128 | 192 | linear | sse2 | 81.0 | 125.9 | 954.7 | 2.0 % | 3.1 % | 3.30 |
| 128 / 128 | 192 | Hermite | sse2 | 122.3 | 170.9 | 251.8 | 3.1 % | 4.3 % | 4.98 |
| 256 / 256 | 192 | linear | sse2 | 165.7 | 289.0 | 2601.3 | 4.1 % | 7.2 % | 3.37 |
| 256 / 256 | 192 | Hermite | sse2 | 248.6 | 372.0 | 1796.5 | 6.2 % | 9.3 % | 5.06 |
| 512 / 503 | 192 | linear | sse2 | 310.0 | 665.0 | 5038.9 | 7.8 % | 16.6 % | 3.21 |
| 512 / 503 | 192 | Hermite | sse2 | 466.2 | 986.4 | 2912.5 | 11.7 % | 24.7 % | 4.83 |
