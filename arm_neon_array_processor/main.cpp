#include "arm_neon_processor.h"
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <sys/utsname.h>

std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_now = std::localtime(&time_t_now);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d_%H-%M-%S", tm_now);
    return std::string(buf);
}

std::string get_system_info() {
    struct utsname un;
    if (uname(&un) == 0) {
        return std::string(un.sysname) + " " + un.release + " (" + un.machine + ")";
    }
    return "Unknown system";
}

bool test_correctness(std::ofstream& log) {
    const size_t test_sizes[] = {0, 1, 2, 3, 4, 5, 7, 8, 15, 16, 100, 1000, 10000};
    
    for (size_t n : test_sizes) {
        std::vector<int32_t> data(n);
        for (size_t i = 0; i < n; ++i) {
            data[i] = static_cast<int32_t>(i % 100) - 50;
        }
        
        int64_t scalar_result = process_array_scalar(data.data(), n);
        int64_t neon_result = process_array_neon(data.data(), n);
        int64_t neon_unrolled_result = process_array_neon_unrolled(data.data(), n);
        
        if (scalar_result != neon_result || scalar_result != neon_unrolled_result) {
            log << "FAIL: Mismatch at size " << n << ": scalar=" << scalar_result 
                << ", neon=" << neon_result 
                << ", unrolled=" << neon_unrolled_result << std::endl;
            std::cerr << "Mismatch at size " << n << ": scalar=" << scalar_result 
                      << ", neon=" << neon_result 
                      << ", unrolled=" << neon_unrolled_result << std::endl;
            return false;
        }
        log << "PASS: Size " << n << " -> " << scalar_result << std::endl;
    }
    
    std::vector<int32_t> mixed = {-100, 0, 50, -25, 0, 75, -1, 1};
    int64_t expected = 100 + 0 + 50 + 25 + 0 + 75 + 1 + 1;
    int64_t result = process_array_neon(mixed.data(), mixed.size());
    if (result != expected) {
        log << "FAIL: Mixed test: expected=" << expected << ", got=" << result << std::endl;
        std::cerr << "Mixed test failed: expected=" << expected << ", got=" << result << std::endl;
        return false;
    }
    log << "PASS: Mixed test -> " << result << std::endl;
    
    return true;
}

bool test_null_pointer(std::ofstream& log) {
    try {
        process_array_scalar(nullptr, 0);
        process_array_neon(nullptr, 0);
        process_array_neon_unrolled(nullptr, 0);
    } catch (...) {
        log << "FAIL: Null pointer with size 0 should not throw" << std::endl;
        std::cerr << "Null pointer with size 0 should not throw" << std::endl;
        return false;
    }
    
    bool caught_scalar = false;
    bool caught_neon = false;
    bool caught_unrolled = false;
    
    try {
        process_array_scalar(nullptr, 1);
    } catch (const std::invalid_argument&) {
        caught_scalar = true;
    }
    
    try {
        process_array_neon(nullptr, 1);
    } catch (const std::invalid_argument&) {
        caught_neon = true;
    }
    
    try {
        process_array_neon_unrolled(nullptr, 1);
    } catch (const std::invalid_argument&) {
        caught_unrolled = true;
    }
    
    if (!caught_scalar || !caught_neon || !caught_unrolled) {
        log << "FAIL: Null pointer with non-zero size should throw" << std::endl;
        std::cerr << "Null pointer with non-zero size should throw" << std::endl;
        return false;
    }
    
    log << "PASS: Null pointer handling" << std::endl;
    return true;
}

void run_benchmark(std::ofstream& log) {
    const size_t sizes[] = {1000, 10000, 100000, 1000000};
    
    log << std::left << std::setw(12) << "Size" 
              << std::setw(15) << "Scalar (ms)" 
              << std::setw(15) << "NEON (ms)" 
              << std::setw(15) << "Unrolled (ms)" 
              << std::setw(12) << "Speedup" << std::endl;
    
    std::cout << std::left << std::setw(12) << "Size" 
              << std::setw(15) << "Scalar (ms)" 
              << std::setw(15) << "NEON (ms)" 
              << std::setw(15) << "Unrolled (ms)" 
              << std::setw(12) << "Speedup" << std::endl;
    
    for (size_t n : sizes) {
        std::vector<int32_t> data(n);
        for (size_t i = 0; i < n; ++i) {
            data[i] = static_cast<int32_t>(i % 100) - 50;
        }
        
        const int iterations = 100;
        
        clock_t start = clock();
        volatile int64_t sum_scalar = 0;
        for (int iter = 0; iter < iterations; ++iter) {
            sum_scalar = process_array_scalar(data.data(), n);
        }
        double time_scalar = static_cast<double>(clock() - start) / CLOCKS_PER_SEC * 1000.0 / iterations;
        
        start = clock();
        volatile int64_t sum_neon = 0;
        for (int iter = 0; iter < iterations; ++iter) {
            sum_neon = process_array_neon(data.data(), n);
        }
        double time_neon = static_cast<double>(clock() - start) / CLOCKS_PER_SEC * 1000.0 / iterations;
        
        start = clock();
        volatile int64_t sum_unrolled = 0;
        for (int iter = 0; iter < iterations; ++iter) {
            sum_unrolled = process_array_neon_unrolled(data.data(), n);
        }
        double time_unrolled = static_cast<double>(clock() - start) / CLOCKS_PER_SEC * 1000.0 / iterations;
        
        double speedup = time_scalar / time_neon;
        
        log << std::left << std::setw(12) << n 
                  << std::setw(15) << std::fixed << std::setprecision(4) << time_scalar
                  << std::setw(15) << time_neon 
                  << std::setw(15) << time_unrolled 
                  << std::setw(12) << std::setprecision(2) << speedup << std::endl;
        
        std::cout << std::left << std::setw(12) << n 
                  << std::setw(15) << std::fixed << std::setprecision(4) << time_scalar
                  << std::setw(15) << time_neon 
                  << std::setw(15) << time_unrolled 
                  << std::setw(12) << std::setprecision(2) << speedup << std::endl;
    }
}

int main() {
    std::string log_filename = "benchmark_" + get_timestamp() + ".log";
    std::ofstream log_file(log_filename);
    
    if (!log_file.is_open()) {
        std::cerr << "Failed to open log file: " << log_filename << std::endl;
        return 1;
    }
    
    log_file << "ARM NEON Array Processor Benchmark Log" << std::endl;
    log_file << "=======================================" << std::endl;
    log_file << "Timestamp: " << get_timestamp() << std::endl;
    log_file << "System: " << get_system_info() << std::endl;
    log_file << std::endl;
    
    std::cout << "ARM NEON Array Processor Tests" << std::endl;
    std::cout << "==============================" << std::endl;
    std::cout << "Log file: " << log_filename << std::endl;
    
    log_file << "=== Correctness Tests ===" << std::endl;
    std::cout << "\nRunning correctness tests..." << std::endl;
    if (!test_correctness(log_file)) {
        log_file << "RESULT: FAILED" << std::endl;
        std::cerr << "Correctness tests FAILED" << std::endl;
        return 1;
    }
    log_file << "RESULT: PASSED" << std::endl << std::endl;
    std::cout << "Correctness tests PASSED" << std::endl;
    
    log_file << "=== Null Pointer Tests ===" << std::endl;
    std::cout << "\nRunning null pointer tests..." << std::endl;
    if (!test_null_pointer(log_file)) {
        log_file << "RESULT: FAILED" << std::endl;
        std::cerr << "Null pointer tests FAILED" << std::endl;
        return 1;
    }
    log_file << "RESULT: PASSED" << std::endl << std::endl;
    std::cout << "Null pointer tests PASSED" << std::endl;
    
    log_file << "=== Benchmark Results ===" << std::endl;
    std::cout << "\nRunning benchmark..." << std::endl;
    run_benchmark(log_file);
    
    log_file << std::endl << "=== Summary ===" << std::endl;
    log_file << "All tests completed successfully!" << std::endl;
    
    std::cout << "\nAll tests completed successfully!" << std::endl;
    std::cout << "Results saved to: " << log_filename << std::endl;
    
    log_file.close();
    
    return 0;
}
