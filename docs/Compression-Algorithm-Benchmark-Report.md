# Compression Algorithm Benchmark Report

## Introduction
This document presents the benchmarking results and methodology for the lightweight compression algorithm implemented in the EcoWatt acquisition system.  
The implementation uses a **hybrid Delta+RLE (Run-Length Encoding) compression scheme** specifically optimized for time-series sensor data acquisition from Inverter SIM.

---


## Compression Method Used

**Algorithm:** Delta + RLE Hybrid Compression  

Our implementation combines multiple compression techniques in a layered approach:

1. **Delta Encoding Layer**
   - **Purpose:** Reduces data magnitude by storing differences between consecutive samples  
   - **Application:** Applied to timestamps, register addresses, raw values, and scaled values  
   - **Baseline:** First sample stored completely; subsequent samples store only deltas  

2. **Run-Length Encoding (RLE) Layer**
   - **Purpose:** Compresses sequences of identical delta values  
   - **Implementation:** When consecutive deltas are identical, stores value once with repetition count  
   - **Marker System:** Uses high bit flag to distinguish single values from RLE runs  
   - **Efficiency:** Particularly effective for repetitive sensor patterns  

3. **Zigzag Encoding**
   - **Purpose:** Optimizes signed integer encoding for variable-length storage  
   - **Mapping:** Negative numbers → odd positives, positive numbers → even positives  
   - **Example:** `-1 → 1`, `0 → 0`, `1 → 2`, `-2 → 3`, `2 → 4`  

---


## Sample Benchmark Results

| **Metric**                   | **Value**                |
|-------------------------------|---------------------------|
| **Compression Method Used**  | Delta + RLE Hybrid        |
| **Number of Samples**        | 30                        |
| **Original Payload Size**    | 3360 bytes                |
| **Compressed Payload Size**  | 1165 bytes                |
| **Compression Ratio**        | 0.347                     |
| **Compression Time**         | 240.500 µs                |
| **Decompression Time**       | 195.900 µs                |
| **Lossless Recovery Verification** | PASS                | 

---

| **Metric**                   | **Value**                |
|-------------------------------|---------------------------|
| **Compression Method Used**  | Delta + RLE Hybrid        |
| **Number of Samples**        | 20                        |
| **Original Payload Size**    | 2240 bytes                |
| **Compressed Payload Size**  | 779 bytes                 |
| **Compression Ratio**        | 0.348                     |
| **Compression Time**         | 68.900 µs                 |
| **Decompression Time**       | 71.300 µs                 |
| **Lossless Recovery Verification** | PASS                | 
