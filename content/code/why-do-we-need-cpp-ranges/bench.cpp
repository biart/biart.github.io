// Benchmark for "Why do we need C++ ranges".
//
//   cl /nologo /std:c++latest /EHsc /O2 bench.cpp                  -> timings, inlinable real()
//   cl /nologo /std:c++latest /EHsc /O2 /DOPAQUE_REAL bench.cpp    -> timings, opaque real()
//   cl /nologo /std:c++latest /EHsc /O2 /DCOUNT_CALLS bench.cpp    -> real() invocation counts
//
//   g++ -std=c++23 -O2 bench.cpp    /    clang++ -std=c++23 -O2 bench.cpp
//
// C++23 is required: variant 9 uses std::ranges::fold_left.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <numeric>
#include <random>
#include <ranges>
#include <string_view>
#include <vector>

// ---------------------------------------------------------------------------
// real() instrumentation
//
// COUNT_CALLS makes real() increment a global. That is an observable side
// effect, so the optimiser MUST emit every call -- which is exactly why the
// counts and the timings have to come from separate builds.
//
// OPAQUE_REAL routes the computation through a volatile function pointer, so
// the compiler can neither inline it nor prove it pure, and cannot CSE two
// calls on the same element.
// ---------------------------------------------------------------------------

#ifdef COUNT_CALLS
    inline long long g_calls = 0;
    #define TICK() (++g_calls)
#else
    #define TICK() ((void)0)
#endif

#ifdef OPAQUE_REAL
    using RealFn = double (*)(double, double);
    inline double real_impl(double angle, double length) { return std::cos(angle) * length; }
    inline volatile RealFn g_real_fn = &real_impl;
#endif

struct Entry
{
    double angle;
    double length;

    double real() const
    {
        TICK();
#ifdef OPAQUE_REAL
        return g_real_fn(angle, length);
#else
        return std::cos(angle) * length;
#endif
    }
};

struct Meow
{
    // ... more stuff that justifies a struct ...
    // ...
    std::vector<Entry> entries;
};

static constexpr double inf = std::numeric_limits<double>::infinity();

// ---------------------------------------------------------------------------
// timing
// ---------------------------------------------------------------------------

// Keeps the result observable so /O2 cannot delete the whole loop.
inline volatile double g_sink = 0.0;

struct Timing
{
    double best_ms;   // best round, per single invocation
    double spread;    // (worst - best) / best, over the rounds
};

// Runs f(data) `inner` times per round, over `rounds` rounds. Minimum-of-rounds
// is the usual choice here: it is the round least disturbed by scheduling noise.
//
// The spread is reported so that untrustworthy rows announce themselves. On a
// quiet machine it stays a few percent; anything above ~20% means the run was
// disturbed and that row should not be quoted.
//
// f is taken by reference on purpose -- Min<3> carries a scratch buffer that
// must survive between invocations.
template <class F>
Timing measure_ms(F& f, const Meow& data, int inner, int rounds)
{
    using Clock = std::chrono::steady_clock;

    for (int i = 0; i < inner; ++i) g_sink = f(data);   // warm up caches / first alloc

    double best = inf;
    double worst = 0.0;
    for (int r = 0; r < rounds; ++r)
    {
        const auto t0 = Clock::now();
        for (int i = 0; i < inner; ++i) g_sink = f(data);
        const auto t1 = Clock::now();
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        best = std::min(best, ms);
        worst = std::max(worst, ms);
    }
    return Timing{best / inner, (worst - best) / best};
}

// ---------------------------------------------------------------------------
// the variants -- numbering follows the list in the post
// ---------------------------------------------------------------------------

template <int I>
struct Min;

// ---- C++17 ----------------------------------------------------------------

template <>
struct Min<1>
{
    static constexpr std::string_view name = "C++17 boomer loop";
    double operator()(const Meow& object_with_entries) const
    {
        double result = inf;
        for (const auto& entry : object_with_entries.entries)
        {
            const double current_real = entry.real();
            if (current_real < result) result = current_real;
        }
        return result;
    }
};

template <>
struct Min<2>
{
    static constexpr std::string_view name = "C++17 min_element + comparator";
    double operator()(const Meow& object_with_entries) const
    {
        const auto iter = std::min_element(
            object_with_entries.entries.cbegin(), object_with_entries.entries.cend(),
            [](const auto& e0, const auto& e1) { return e0.real() < e1.real(); });
        return iter->real();
    }
};

template <>
struct Min<3>
{
    static constexpr std::string_view name = "C++17 transform + min_element (scratch reused)";

    // Lives in the functor, so its capacity survives every invocation: the
    // first call allocates, the rest do not. Compare against Min<12>.
    std::vector<double> scratch;

    double operator()(const Meow& object_with_entries)
    {
        const auto& entries = object_with_entries.entries;
        scratch.resize(entries.size());          // no-op after the first call
        std::transform(entries.cbegin(), entries.cend(), scratch.begin(),
                       [](const auto& e) { return e.real(); });
        return *std::min_element(scratch.cbegin(), scratch.cend());
    }
};

template <>
struct Min<4>
{
    // Timings are indistinguishable from Min<11>, std::accumulate.
    static constexpr std::string_view name = "C++17 std::transform_reduce";
    double operator()(const Meow& object_with_entries) const
    {
        return std::transform_reduce(
            object_with_entries.entries.cbegin(), object_with_entries.entries.cend(), inf,
            [](const double a, const double b) { return (std::min)(a, b); },  // reduce
            [](const Entry& e) { return e.real(); });                         // transform
    }
};

// ---- C++20 ----------------------------------------------------------------

template <>
struct Min<5>
{
    static constexpr std::string_view name = "C++20 ranges::min_element + transform";
    double operator()(const Meow& object_with_entries) const
    {
        // Named, because ranges::min_element over a temporary view returns
        // std::ranges::dangling and will not compile.
        const auto reals_view = object_with_entries.entries | std::views::transform(&Entry::real);
        return *std::ranges::min_element(reals_view);
    }
};

template <>
struct Min<6>
{
    static constexpr std::string_view name = "C++20 ranges::min_element + projection";
    double operator()(const Meow& object_with_entries) const
    {
        // Returns an iterator into entries, not the projected value, so the
        // final real() is unavoidable here.
        const auto iter = std::ranges::min_element(object_with_entries.entries, {}, &Entry::real);
        return iter->real();
    }
};

template <>
struct Min<7>
{
    static constexpr std::string_view name = "C++20 ranges::min + transform";
    double operator()(const Meow& object_with_entries) const
    {
        // Returns a value, so the temporary view is fine.
        return std::ranges::min(object_with_entries.entries | std::views::transform(&Entry::real));
    }
};

template <>
struct Min<8>
{
    static constexpr std::string_view name = "C++20 ranges::min + projection";
    double operator()(const Meow& object_with_entries) const
    {
        // Returns the Entry itself (by value), so real() has to be called again.
        const Entry result = std::ranges::min(object_with_entries.entries, {}, &Entry::real);
        return result.real();
    }
};

// ---- C++23 ----------------------------------------------------------------

template <>
struct Min<9>
{
    static constexpr std::string_view name = "C++23 ranges::fold_left + transform";
    double operator()(const Meow& object_with_entries) const
    {
        return std::ranges::fold_left(
            object_with_entries.entries | std::views::transform(&Entry::real), inf,
            [](const double a, const double b) { return (std::min)(a, b); });
    }
};

// ---- additional variants, not in the post's main list ----------------------

template <>
struct Min<10>
{
    static constexpr std::string_view name = "C++17 boomer loop with std::min";
    double operator()(const Meow& object_with_entries) const
    {
        double result = inf;
        for (const auto& entry : object_with_entries.entries)
            result = (std::min)(result, entry.real());
        return result;
    }
};

template <>
struct Min<11>
{
    static constexpr std::string_view name = "C++17 std::accumulate";
    double operator()(const Meow& object_with_entries) const
    {
        return std::accumulate(
            object_with_entries.entries.cbegin(), object_with_entries.entries.cend(), inf,
            [](const double acc, const auto& e) { return (std::min)(acc, e.real()); });
    }
};

template <>
struct Min<12>
{
    static constexpr std::string_view name = "C++17 transform + min_element (fresh buffer)";
    double operator()(const Meow& object_with_entries) const
    {
        const auto& entries = object_with_entries.entries;
        std::vector<double> entry_real_parts;      // allocates on every call
        entry_real_parts.reserve(entries.size());
        std::transform(entries.cbegin(), entries.cend(),
                       std::back_inserter(entry_real_parts),
                       [](const auto& e) { return e.real(); });
        return *std::min_element(entry_real_parts.cbegin(), entry_real_parts.cend());
    }
};

// ---------------------------------------------------------------------------
// driver
// ---------------------------------------------------------------------------

static Meow make_data(std::size_t n)
{
    std::mt19937_64 rng(20260811);
    std::uniform_real_distribution<double> angle(0.0, 6.283185307179586);
    std::uniform_real_distribution<double> length(0.5, 2.0);

    Meow m;
    m.entries.reserve(n);
    for (std::size_t i = 0; i < n; ++i) m.entries.push_back(Entry{angle(rng), length(rng)});
    return m;
}

#ifdef COUNT_CALLS

template <int I>
static void count_one(const Meow& data)
{
    Min<I> f;
    g_calls = 0;
    const double v = f(data);
    const long long n = static_cast<long long>(data.entries.size());
    std::printf("  %2d  %-46.46s calls=%-5lld (N=%lld, 2N-1=%lld)  min=%.6f\n",
                I, Min<I>::name.data(), g_calls, n, 2 * n - 1, v);
}

#else

template <int I>
static void time_one(const Meow& data, int inner, int rounds)
{
    Min<I> f;                                   // one instance: Min<3> keeps its buffer
    const Timing t = measure_ms(f, data, inner, rounds);
    const double ns_per_elem = t.best_ms * 1e6 / static_cast<double>(data.entries.size());
    std::printf("  %2d  %-46.46s %10.5f ms  %8.3f ns/elem  spread %5.1f%%%s\n",
                I, Min<I>::name.data(), t.best_ms, ns_per_elem, t.spread * 100.0,
                t.spread > 0.20 ? "  <-- NOISY" : "");
}

#endif

template <int I>
static void run_one(const Meow& data, int inner, int rounds)
{
#ifdef COUNT_CALLS
    (void)inner; (void)rounds;
    count_one<I>(data);
#else
    time_one<I>(data, inner, rounds);
#endif
}

static void run_all(const Meow& data, int inner, int rounds)
{
    run_one<1>(data, inner, rounds);
    run_one<2>(data, inner, rounds);
    run_one<3>(data, inner, rounds);
    run_one<4>(data, inner, rounds);
    run_one<5>(data, inner, rounds);
    run_one<6>(data, inner, rounds);
    run_one<7>(data, inner, rounds);
    run_one<8>(data, inner, rounds);
    run_one<9>(data, inner, rounds);
    run_one<10>(data, inner, rounds);
    run_one<11>(data, inner, rounds);
    run_one<12>(data, inner, rounds);
}

int main()
{
#ifdef OPAQUE_REAL
    std::printf("real(): OPAQUE (volatile function pointer)\n");
#else
    std::printf("real(): inlinable\n");
#endif

#ifdef COUNT_CALLS
    std::printf("\n=== real() invocation counts (N=8) ===\n");
    run_all(make_data(8), 0, 0);
#else
    const struct { std::size_t n; int inner; } cases[] = {
        {   2048,  500},
        { 131072,    8},
    };
    for (const auto& c : cases)
    {
        std::printf("\n=== N = %zu (%d reps x 15 rounds, best round) ===\n", c.n, c.inner);
        run_all(make_data(c.n), c.inner, 15);
    }
#endif
    return 0;
}
