#include "Tasks.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace CustomTasks {
    // Prevent over-optimization by anchoring results
    static volatile std::uint64_t g_sink = 0;

    // Simple sieve to get kth prime for large k efficiently
    static std::uint64_t kthPrimeSieveLarge(int k) {
        if (k <= 0) return 0;
        // Upper bound for nth prime: n (ln n + ln ln n) for n >= 6 (Rosser's theorem)
        // Add a safety margin factor 1.2
        double dk = static_cast<double>(k);
        double ln = std::log(dk);
        double bound = dk * (ln + std::log(ln));
        bound = std::max(bound, 15.0);
        bound *= 1.2;
        std::size_t N = static_cast<std::size_t>(bound) + 10;
        std::vector<bool> isPrime(N, true);
        isPrime[0] = false;
        if (N > 1) isPrime[1] = false;
        for (std::size_t p = 2; p * p < N; ++p) {
            if (!isPrime[p]) continue;
            for (std::size_t m = p * p; m < N; m += p) {
                isPrime[m] = false;
            }
        }
        int count = 0;
        for (std::size_t i = 2; i < N; ++i) {
            if (isPrime[i]) {
                ++count;
                if (count == k) return static_cast<std::uint64_t>(i);
            }
        }
        // In unlikely case the bound was too low, fallback to incremental check
        std::uint64_t n = static_cast<std::uint64_t>(N);
        while (true) {
            bool p = true;
            for (std::uint64_t d = 2; d * d <= n; ++d) if (n % d == 0) { p = false; break; }
            if (p) {
                ++count;
                if (count == k) return n;
            }
            ++n;
        }
    }

    static bool isPrime(std::uint64_t x) {
        if (x < 2) return false;
        if (x % 2 == 0) return x == 2;
        for (std::uint64_t d = 3; d * d <= x; d += 2) {
            if (x % d == 0) return false;
        }
        return true;
    }

    // Scale factor: run inner computation multiple times to increase CPU time
    static int repeatsFromK(int k) {
        if (k <= 0) return 1;
        // For very large k, don't repeat (inner work is already heavy).
        if (k >= 100000) return 1;
        if (k >= 50000) return 2;
        // For moderate/small k, scale repeats up to 200
        int capped = std::min(200, k);
        return std::max(1, capped);
    }

    std::uint64_t kthPrime(int k) {
        if (k <= 0) return 0;
        // Use sieve for large k to avoid O(k * sqrt(n)) explosion
        if (k >= 100000) {
            std::uint64_t ans = kthPrimeSieveLarge(k);
            g_sink ^= ans;
            return ans;
        }
        std::uint64_t last = 0;
        int repeats = repeatsFromK(k);
        for (int r = 0; r < repeats; ++r) {
            std::uint64_t count = 0;
            std::uint64_t n = 1;
            while (count < static_cast<std::uint64_t>(k)) {
                ++n;
                if (isPrime(n)) ++count;
            }
            last = n;
            g_sink ^= last;
        }
        return last;
    }

    std::uint64_t fibonacciK(int k) {
        if (k <= 0) return 0;
        std::uint64_t last = 0;
        int repeats = repeatsFromK(k);
        for (int r = 0; r < repeats; ++r) {
            if (k == 1) last = 0;
            else if (k == 2) last = 1;
            else {
                std::uint64_t a = 0, b = 1;
                for (int i = 3; i <= k; ++i) {
                    std::uint64_t c = a + b;
                    a = b;
                    b = c;
                }
                last = b;
            }
            g_sink ^= last;
        }
        return last;
    }

    std::uint64_t factorialK(int k) {
        if (k < 0) return 0;
        std::uint64_t last = 1;
        int repeats = repeatsFromK(k);
        for (int r = 0; r < repeats; ++r) {
            std::uint64_t res = 1;
            for (int i = 2; i <= k; ++i) res *= static_cast<std::uint64_t>(i);
            last = res;
            g_sink ^= last;
        }
        return last;
    }

    static bool isPalindrome(std::uint64_t x) {
        std::uint64_t orig = x, rev = 0;
        while (x > 0) {
            rev = rev * 10 + (x % 10);
            x /= 10;
        }
        return rev == orig;
    }

    std::uint64_t kthPalindrome(int k) {
        if (k <= 0) return 0;
        std::uint64_t last = 0;
        int repeats = repeatsFromK(k);
        for (int r = 0; r < repeats; ++r) {
            std::uint64_t count = 0;
            std::uint64_t n = 0;
            while (count < static_cast<std::uint64_t>(k)) {
                ++n;
                if (isPalindrome(n)) ++count;
            }
            last = n;
            g_sink ^= last;
        }
        return last;
    }
}


