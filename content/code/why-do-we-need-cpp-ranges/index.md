+++
title = "Why do we need C++ ranges"
description = "Slightly dated takes on why we should adopt std::ranges"
date = 2026-08-12
+++


## Intro

After a couple of conversations with colleagues, I realised I had become something of an [`std::ranges`](https://cppreference.com/cpp/ranges) apologist. This post started as an attempt to put those thoughts on paper and make the case to C++ coders who never got around to trying `std::ranges`.

It is 2026 -- six years since C++20, a standard revolutionary enough that the word needs no defending. Yet some of my fellow C++ programmers still aren't closely familiar with everything it brings. Concepts are universally welcomed; `std::ranges` is where opinions split, and scepticism sometimes wins. The [VFX Reference Platform](https://vfxplatform.com/#reference-platform) only scheduled its C++20 transition for this year, so while this post is a little dated, it appears just in time to echo some older videos from C++ conferences and revisit some of their goodies, as more people are about to meet these features for the first time.

I planned something short at first: take a task that is one line in Python, show why every C++17 spelling of it comes out bulky, and show how `ranges` fix it. But then I benchmarked it, and... fell down a rabbit hole. The obvious ranges spelling turns out to cost twice what it should on two of the three major standard libraries.

So this is still a case for `std::ranges` — I use them, I recommend them, and I show that we can easily come up with a one-liner that is exactly as fast as the hand-written loop. But it is also a map of where the abstraction leaks today, which spelling to reach for, and which one to quietly avoid. (And I promise a small C++20 bonus you might not have heard of)


## Finding minimum with a twist

Let's take a small programming task of the kind we run into every day. Suppose we have a struct with a getter that does a little computation, and a container of those entities:

```cpp
struct Entry
{
    double angle;
    double length;

    double real() const { return std::cos(angle) * length; }
};

struct Meow
{
    // ... more stuff that justifies a struct ...
    // ...
    std::vector<Entry> entries;
};

Meow object_with_entries;
```

The task is simply to find the minimal value of `real()` in this container. The whole problem fits in one sentence, so we might reasonably expect to write it just as briefly. In Python, we can:

```python
result = min(map(Entry.real, object_with_entries.entries))
```

or, with a generator expression:

```python
result = min(entry.real() for entry in object_with_entries.entries)
```

Notice how closely that line mirrors the way we stated the task in plain English -- and that is definitely a virtue. So... how would we do it in C++? Many people reach for a sturdy "boomer" loop:

```cpp
double result = std::numeric_limits<double>::infinity();
for (const auto& entry : object_with_entries.entries)
{
    const double current_real = entry.real();
    if (current_real < result) result = current_real;
}
```

Not bad. Some would rely on [`std::min`](https://cppreference.com/cpp/algorithm/min) and write something more comprehensible:

```cpp
double result = std::numeric_limits<double>::infinity();
for (const auto& entry : object_with_entries.entries)
{
    result = std::min(result, entry.real());
}
```

Just in case, I tried both variants in the benchmark below. But as they perform identically on every compiler I tried, from here on I'll refer to either of them as the boomer loop.


## What's wrong with boomer loops: opinions

What is wrong with the above code? Nothing serious, as in "I would approve this PR for our codebase". However, some questions remain. We reimplemented the logic of the *minimal element* function, and that violates the so-called DRY principle[^dry]... No, three lines of code are not much logic, but maybe the real problem is that this loop, despite its charming simplicity, diverges from the natural language too much? Writing out a loop every time we want a simple action, as if we were solving a little programming exercise along the way, is more ceremony than the task deserves. It is as if, every time we wanted a cup of tea, we asked: "could you pour some tap water into a kettle and switch it on, please?" -- and let's hope the waiter doesn't take it literally when the kettle is already full.

[^dry]: [Don't repeat yourself](https://en.wikipedia.org/wiki/Don%27t_repeat_yourself) — "every piece of knowledge must have a single, unambiguous, authoritative representation within a system". Coined by Andy Hunt and Dave Thomas in *The Pragmatic Programmer* (1999).

Another nitpick is the infinity trick. It is fine for double, but it buys us one pointless comparison on the first iteration. Initialising `result` with `entries[0]` is cleaner, but it needs more boilerplate, we lose the range-for loop, and it is undefined behaviour on an empty container. To back up these observations, let's appeal to authority:

 - [Scott Meyers, "STL Algorithms vs. Hand-Written Loops"](https://jacobfilipp.com/DrDobbs/articles/CUJ/2001/0110/smeyers/smeyers.htm) (*C/C++ Users Journal*, October 2001) -- a lightly edited reprint of Item 43 of *Effective STL*[^meyers]. The C++ guru suggests that STL algorithms are often better implemented than what an average programmer writes ad hoc: they handle corner cases, and they may be faster.
 - [Sandor Dargo, "Loops vs algorithms"](https://www.sandordargo.com/blog/2020/05/13/loops-vs-algorithms) -- a balanced take that still leans towards the STL where possible, "unless squeezing the last bits of perf".
 - [C++ Core Guidelines ES.71](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es71-prefer-a-range-for-statement-to-a-for-statement-when-there-is-a-choice) -- while recommending range-for over a raw `for` (which we already did), the authors add: "Sometimes better still, use a named algorithm.".

[^meyers]: Scott Meyers, *Effective STL: 50 Specific Ways to Improve Your Use of the Standard Template Library*, Addison-Wesley, 2001, Item 43: "Prefer Algorithm Calls to Hand-Written Loops" (ISBN 978-0-201-74962-5). Meyers lists the article on his [publications page](https://www.aristeia.com/publications.html) and notes on his [blog](https://scottmeyers.blogspot.com/2001/09/updated-errata-lists-article-in-cuj.html) that the October CUJ carried "a slightly-modified version of *Effective STL*'s Item 43".

I think it would be fair if I say that there is an opposite opinion: there is a long tradition in graphics and games of writing C with a restricted subset of C++ and no STL at all. It is not ignorance -- it comes from compile times, debug-build performance, and wanting to see the cost of every line at the call site. There are famous people whom I admire, like Jonathan Blow, who was so frustrated with C++ that he wrote his own language [Jai](https://en.wikipedia.org/wiki/Jonathan_Blow#2017%E2%80%93present:_Jai_programming_language,_Order_of_the_Sinking_Star,_and_Braid,_Anniversary_Edition), and Casey Muratori with his [spicy takes about C++](https://cmuratori.spicytakes.org/post/2014-07-23-blog_0023#quote-0). The no STL sentiment and some criticism of the abstractions that can be confusing for both the programmer and the CPU even appear at CppCon itself as a part of Mike Acton's talk, which came out of real problems in game production:

- [Mike's talk: CppCon 2014 Data-Oriented Design and C++](https://isocpp.org/blog/2015/01/cppcon-2014-data-oriented-design-and-c-mike-acton)

Personally, I share some of their misgivings about object-oriented design. But I do rely on the STL, and well-designed abstractions are often a lifesaver for me. That may be because my field is not games or rendering, with their frame budgets and GPU interop, but computational geometry -- where a working day looks more like "run Dijkstra over ten different views of a graph". Or... maybe I just love abstractions.


## C++17 STL in action

Many of us, myself included, share the belief in DRY and standard tools. So let's try the STL. Most languages have a helper for finding a minimum, and C++ is no exception: it has [`std::min_element`](https://cppreference.com/cpp/algorithm/min_element), which sounds like a semantic fit to our task:

```cpp
const auto iter = std::min_element(
    object_with_entries.entries.cbegin(), object_with_entries.entries.cend(),
    [](const auto& e0, const auto& e1) { return e0.real() < e1.real(); });
const double result = iter->real();
```

It works, but the code is barely acceptable. The problem is that C++ is designed to be generic, so `std::min_element` is based on an abstract comparison of two objects. What we want initially is: "apply function, then find minimum", but we are obliged to reframe it as "find minimum by comparing objects as if the function were applied". This also explains the last annoying `->real()` to get the result: as if writing `real()` twice already was not enough! Execution-wise there are `2N - 1` `real()` calls in total, compared to the boomer loop above that has just `N`. While we may hope the optimiser will collapse those redundant calls into one, it does not -- I checked, and the performance section has the numbers. The readability problem stands regardless.

Wait, but C++ has [`transform`](https://cppreference.com/cpp/algorithm/transform). So why not pipe it into `min_element` and get the "apply function, then find minimum" that we actually want? Well, because O(N) of extra memory is not something we should pay for in a "performance" language. It is ironic that in the "slow" Python version we neatly avoided a list comprehension in favour of a generator or a map object.

In fairness, I benchmarked this later, and the penalty is smaller than the argument implies -- a couple of percent, and very nearly nothing if the buffer is reused instead of allocated on every call. Although reusing a buffer does require *having* one: a member to keep around, a parameter to thread through the call, or an arena to reach for. Somehow that is never as convenient as it sounds in a sentence. And there is a more durable reason to dislike this shape anyway: splitting one pass into two doubles the memory traffic, which is a habit worth avoiding whether or not the profiler complains about it today.

Either way, the transform-based version is worth writing down, for reference and for benchmarking:

```cpp
std::vector<double> entry_real_parts;
entry_real_parts.reserve(object_with_entries.entries.size());
std::transform(
    object_with_entries.entries.cbegin(), object_with_entries.entries.cend(),
    std::back_inserter(entry_real_parts), [](const auto& e) { return e.real(); });
const double result = *std::min_element(entry_real_parts.cend(), entry_real_parts.cend());
```

You have probably noticed that I made the variable names long, and that is on purpose. In my experience, data is often not available as a local variable, but only reachable through some compound expression -- typically as a member of another object. So my seemingly cunning move is not too artificial: it can realistically pop up in your codebase and make your eyes bleed, as the 27 characters of `object_with_entries.entries` have to appear three times: once to reserve (or resize) memory, and twice to transform the container. This forces us to introduce aliases more often than is warranted, and makes the code heavier. Sure, we programmers adapt: we get used to recognising familiar patterns, so in our minds they turn into something bearable in the blink of an eye. But frankly: how many of us would notice that I have seeded a bug there -- that one of the `cbegin`s has been sneakily replaced with `cend`?

Finally, just to be honest, there *is* a way to use the STL sensibly here without either overcomputing `real` or allocating memory. As we are cool coders, possibly with some college degrees, we know that `min(a, b)` is a bit like `a + b` in the sense that it can be "folded" in any order to get the desired minimum, just like `+` can be folded to get the total sum. The problem we are solving is actually a classic map-reduce. So we can use [`std::accumulate`](https://cppreference.com/cpp/algorithm/accumulate) for the reduction:

```cpp
const double value = std::accumulate(
    object_with_entries.entries.cbegin(), object_with_entries.entries.cend(),
    std::numeric_limits<double>::infinity(),
    [](const double acc, const auto& e) { return std::min(acc, e.real()); });
```

Now tell me, have you ever heard of [`std::transform_reduce`](https://cppreference.com/cpp/algorithm/transform_reduce)? Me, personally... well, I learned of it today, while writing this post! So here is the last C++17 solution, one you might never have come up with:

```cpp
const double value = std::transform_reduce(
    object_with_entries.entries.cbegin(), object_with_entries.entries.cend(),
    std::numeric_limits<double>::infinity(),
    [](const double a, const double b) { return std::min(a, b); },  // reduce
    [](const Entry& e) { return e.real(); });                       // transform
```

I browsed the docs and found no other way to compute a minimum. I would therefore be happy to conclude that the last snippet is peak C++17 and stop here in peace -- except there is a quirk. All this colossal work is about to be shattered by an innocent review comment: "you are clearly looking for a minimum, so let's just use `std::min_element`!"

It looks like the STL is surprisingly helpless -- or, more precisely, awkward -- for this simple task, and I assume this contributes to a widespread sentiment: if the STL can be used efficiently only for a subset of tasks, and for the remaining corner cases we have to write custom loops anyway, then why bother at all? We can simply go for old-school loops that are always efficient, earning consistency and simplicity at the cost of a little boilerplate. Yeah, it feels like we have been building a case against the standard library, and frankly, I understand people who want to ditch it completely. But if you still follow modern C++ ideas, and if the STL earns its keep in your code, then C++20 may refine that experience and close a number of the gaps we recap in the next section.


## A summary of STL problems

Let's sum up the pre-C++20 problems we encountered. I could distinguish three main problems, in decreasing order of importance:

### A. No appropriate STL algorithm to cover compound tasks

When we need map-reduce, we have [`transform_reduce`](https://en.cppreference.com/cpp/algorithm/transform_reduce) (although not many people have heard of it, I suspect). When we want to filter, then count values, there is [`std::count_if`](https://en.cppreference.com/cpp/algorithm/count). But such fusion exists only for a limited number of use cases.

First of all, `transform_reduce` does not work for more than two ranges without materialising the intermediate results into memory. There is not even a way to combine `transform` and `copy_if` into something like `transform_if` in C++17. There is obviously no `transform_min_element`, and the ways to achieve it require either writing a comparison function that costs more than the hand-written loop, or reaching for a differently named function. Moreover, the mere existence of more functions like `transform_if` or `transform_min_element` does not solve the problem in principle: it cannot cover the infinite combinations of possible subproblems. It does not scale, and programmers are unlikely to remember which functions actually exist in the standard and which do not.

Just for fun, I can list more examples "unsolvable" by the STL (without dealing with exotic things like mutating lambdas), but easily solvable by the standard tools of other languages:

 - add the index of each element to the element itself, in a container of integers.
 - merge two sorted sequences while skipping odd values.
 - given two sequences A and B, find the first element of A that equals the element of B at the same position.
 - given a container of 2D points (pairs of numbers) that define a polygon, find the polygon's smallest internal angle (i.e. the most acute).

I asked Claude if there was some STL algorithm I had missed that could crack these, and it found [`std::mismatch`](https://en.cppreference.com/cpp/algorithm/mismatch) for the third one. Impressive! Yet the first two are harder to solve. Let me know if you have an elegant pre-C++20 solution for these.

### B. Iterator interfaces of the STL are unnecessarily verbose

We need to write `<expression>` twice for `<expression>.begin()` and `<expression>.end()`, and this is a duplication that would be nice to avoid. But why does it exist in the first place? Well, because an iterator is historically an abstraction over a pointer, and a pointer carries no information about where to stop, so you need two of them to express something that in natural language is a single entity. As a side effect, the two-iterator form is also somewhat generic: it lets us pass a slice to algorithms. We can argue that this genericity is only partial, as we cannot pass strided data, for instance. But I would say the real problem is that this abstraction is stuck midway: it adds unnecessary duplication for the vast majority of use cases, is not generic enough to solve all of them, and for some reason covers a "slicing" case that is not that frequent in reality. To illustrate this, I made a small experiment: I took the C++17 project from my work and counted all the places where we pass an entire range to an STL function versus where we pass a slice like `f(std::next(a.begin()), a.end())` or `const auto iter = std::find(...); f(iter, a.end())`. Here are the results:

| Call shape | Count |
|---|---|
| Full-range iterator pairs | ~700 |
| Slices | ~50 |

It appeared surprisingly difficult to gather these stats. In a codebase that has a lot of templated functions that accept containers, with no standard template argument name reserved for this, and where third-party span is already used (albeit sparsely), tracking each input to every function is tedious. So I limited myself to a script that only inspects STL call sites, and we can extrapolate it to the rest of the use cases. I had to remove false positives like the remove/erase idiom or [`std::distance`](https://en.cppreference.com/cpp/iterator/distance), so this remains a fair estimate. There is also some survivorship bias: in C++17 the code calls the STL only when it is possible, and for loops are still popular alongside STL calls. Yet this does not seem to shift this statistic in any particular direction, but only highlights problem **A** we have discussed.

What do the statistics say? More than 90% of the time we need to pass an entire range to an algorithm, so the API should *not* treat slices as the first-class case. Rather, it would be more appropriate to add slicing as a second overload, especially since we humans usually don't think in terms of iterators, but in terms of whole entities or sets. Returning to our tea example: it is, again, too verbose to think of filling a kettle "from bottom to top". One may start thinking whether this detail is even important, and whether we could theoretically pour water "from the bottom of this kettle to the top of that mug". Yeah -- water spilled all over your API is a design issue.

### C. We need to write lambdas even if we already have the function

This is really just another side of problem **A**: in C++17, even when we can use some STL function, in order to conform to its API we have to write throwaway lambdas that wrap the function we already have. It is less of an issue, but I list it separately so I can share some good news later: C++20 has some nice improvements on top of ranges that further streamline using the standard library and overcome this last caveat.


## Solving min problem with C++20 and ranges

Let's look at what C++20 brings. First, consider the following snippet ([godbolt](https://godbolt.org/z/E5W53jWdr)):

```cpp
struct RealIter
{
    using Base = std::vector<Entry>::const_iterator;
    Base base{};

    using value_type        = double;
    using reference         = double;                               // a prvalue, not const double&
    // using iterator_category = std::random_access_iterator_tag;   // dishonest: reference is not a reference
    using iterator_category = std::input_iterator_tag;              // honest

    double operator*() const { return base->real(); }

    // ...and twenty-odd more lines forwarding ++, --, +=, -=, +, -, []
    // and the six comparisons to `base`. Nothing interesting in any of them.
};
```

Anyone familiar with Boost may recognise [`boost::transform_iterator`](https://www.boost.org/doc/libs/latest/libs/iterator/doc/html/iterator/specialized/transform.html) in this object. The struct defines a new "iterator" that behaves exactly like a vector iterator, except that dereferencing it computes `real()`. With `RealIter` we have crafted yet another way to find our minimum:

```cpp
const double result = *std::min_element(
    RealIter{ object_with_entries.entries.cbegin() },
    RealIter{ object_with_entries.entries.cend() });
```

Do you think this works? It somehow does -- in Microsoft STL and libstdc++. With libc++ it fails to compile. The problem is that this iterator is weird: its `operator*` returns a prvalue rather than a reference. Prior to C++20, an iterator that returns a prvalue on dereference was not a valid forward iterator. The [docs](https://en.cppreference.com/cpp/named_req/ForwardIterator) are blunt about it:

> If `X` is a mutable iterator, `Ref` is a reference to `T`.

> If `X` is a constant iterator, `Ref` is a reference to `const T`.

This forces us, if we are pedantic enough, to set `iterator_category` to `std::input_iterator_tag`, which is a weaker category than a forward iterator. But an input iterator is not enough for `std::min_element`, which is why compilation fails. Note the perverse incentive here: it is the *honest* tag that breaks the build. Uncomment the dishonest `random_access_iterator_tag` on the line above and everything compiles everywhere, standard requirements be damned.

Remember that an iterator used to be, first of all, an abstraction over a pointer -- and pointers, well, point to real memory. If you are an experienced C++ programmer, you may stop me here. There are some curious examples in the STL where this is definitely **not** the case: the infamous [vector of bools](https://en.cppreference.com/cpp/container/vector_bool), whose `std::vector<bool>::reference` is not `bool&`. Apparently -- and I discovered this while preparing the post -- `vector<bool>::iterator` declaring `random_access_iterator_tag` is a known defect that the committee has lived with since C++98.

So why are we discussing this? Because since C++20 these requirements have been declared legacy. The new range algorithms are constrained by named concepts (such as [`std::forward_iterator`](https://en.cppreference.com/cpp/iterator/forward_iterator)), and those do not expect an iterator to point at real memory[^contiter]: `*it` may simply compute its result and hand it back by value -- much closer to iterators in Python or Rust. In particular, the snippet above compiles if we switch to `std::ranges::min_element` and drop `iterator_category` altogether[^itercon]. And that is exactly what a C++20 range view is: [`views::transform`](https://en.cppreference.com/cpp/ranges/transform_view) builds the same computing iterator for us and carries its end along with it -- so we pass one object instead of two, and problems **A** and **B** fall at once.

[^contiter]: With exactly one exception. The new `std::contiguous_iterator` -- the strongest category, above random access -- still requires `*it` to be a true lvalue reference, and `std::to_address(it)` to hand back a pointer to it. That is what contiguity means: the elements really are laid out in memory and you may take their addresses. Conversely, in C++20 an iterator can be fully random-access while returning a prvalue; it simply cannot be contiguous. See [[iterator.concepts]](https://eel.is/c++draft/iterator.concepts).

[^itercon]: As far as I understand, dropping `iterator_category` is the clean move if you only care about C++20. If the iterator must also stay visible to the older algorithms, you can keep `iterator_category` and add `iterator_concept` beside it: the range concepts prefer `iterator_concept` when it is present, while the old algorithms go on reading `iterator_category`. This is precisely what `std::views::transform` does -- its iterator declares `input_iterator_tag` as its category and `random_access_iterator_tag` as its concept.

```cpp
using std::views::transform;

const auto reals_view = transform(object_with_entries.entries, [](const auto& e) { return e.real(); });
const double result = *std::ranges::min_element(reals_view);
```

`reals_view` behaves just like a random-access container of `real()` results, except that nothing is actually stored. It is then passed to the new `std::ranges::min_element`, which accepts these abstractions -- or, equally, containers themselves: you can hand it a plain vector of doubles. In our case it returns a transformed iterator, so dereferencing applies `real` as well.

We can also write this using the new [pipe operator](https://en.cppreference.com/cpp/ranges#Example) syntax:

```cpp
using std::views::transform;
const auto reals_view = object_with_entries.entries |
    transform([](const auto& e) { return e.real(); });
const double result = *std::ranges::min_element(reals_view);
```

Finally, `std::ranges` also provides new algorithms that depart from the existing interfaces: instead of returning an iterator, [they return the resulting value directly](https://en.cppreference.com/cpp/algorithm/ranges/min). This throws information away, but in exchange allows some optimisations that I will come back to later. And it is actually a better fit for our problem, since we never needed to locate the element in the first place:

```cpp
using std::views::transform;
const double result = std::ranges::min(object_with_entries.entries |
    transform([](const auto& e) { return e.real(); }));
```

It is impressively close to how we stated the task in English, and -- once we get rid of that lambda in a moment -- nearly as compact as the Python version. It clearly solves concerns **A** and **B**: one argument for one range; `real` used directly to transform the entries, rather than smuggled into a comparator; and `min_element` or `min` expressing our intent far better than `accumulate` ever did.

There are also no intermediate copies, because `reals_view` is never materialised: it is a lazy view over the entries, and `real` is called only on demand, when the view's iterator is dereferenced. A pragmatic C++20 mindset, I believe, lies in separating a view's *definition* from its *materialisation*: we first assemble an expression by nesting views, then evaluate it exactly where we need it, with the STL algorithms "compiling" the whole thing into something that looks very much like a for loop. That gives us full control over API design and over where the computation actually happens.

Before measuring what this abstraction costs, let me show a couple of C++20 bonuses I advertised in the introduction.

### Bonus one: `std::invoke` as the default way to call things

This one is my own little discovery -- I have not seen it advertised much, so I am delighted to share it, even if the impact is mostly cosmetic. You have probably never cared much about `std::invoke`, and neither have I. However, C++20 adopts this function as *the* primary way to call things passed to the standard library, which has a pleasant implication: `std::invoke` was designed to "work out of the box" with anything callable, pointers to members included. For instance, look how this simplifies the min example and solves problem **C** entirely:

```cpp
using std::views::transform;
const double result = std::ranges::min(object_with_entries.entries | transform(&Entry::real));
```

It is slightly more verbose than Python due to namespaces, but for once it is really a one-liner! We avoid writing a trivial lambda that just calls a member function, and pass a pointer to the member function directly instead. Check yourself: doing so would fail to compile with pre-C++20 `std::transform`.

### Bonus (or caveat?) two: the projection

Ironically, C++20 also upgrades algorithms in a way that makes range views not strictly necessary to cleanly solve our min case. It introduces a *projection* parameter, which behaves much like applying a transform view before the call:

```cpp
const Entry result = std::ranges::min(
    object_with_entries.entries,
    {},             // <== the good old comparison function, kept as the default std::less
    &Entry::real);  // <== the new "projection": applied to each element before comparing
```

One difference from the previous version is that it returns the entry itself, not its `real()` -- so we have to call `real()` once more to get the number we were after. Note also the return type: `std::ranges::min` hands back a `range_value_t`, a *value*, so the whole `Entry` is copied out[^mincopy]. For a fat struct that is a real cost, and one more reason `min_element` still exists.

[^mincopy]: `std::ranges::min` returning `range_value_t` is another small disappointment: I am not sure why the standard committee declined the idea of returning a range reference (that can be a prvalue conditionally).

It is nice syntactic sugar that gives us yet another way to find a minimum. But you need to be careful with it. After benching this version, I discovered one curious [requirement](https://timsong-cpp.github.io/cppwp/n4868/alg.min.max) of the projection-based interface, in [alg.min.max]/7:

> Complexity: Exactly `ranges::distance(r) - 1` comparisons and twice as many applications of the projection, if any.

If I understand it right, this means that if a projection is supplied, it **has** to be called `2N - 2` times, so our `real` computation is carried out nearly twice as many times as needed -- just like the C++17 `std::min_element` version with a custom comparator. Yes, the one that I called "barely acceptable"...

To be fair to projections, this doubling is not universal. It happens in algorithms that compare two elements *to each other* -- `min`, `max`, `minmax`, `sort`. Where the comparison is against something else, as in `ranges::find` or `count_if`, the projection is applied exactly once per element. It is pairwise comparison specifically that pays twice.

But let's actually bench it!

## Performance

Now it is the right moment to figure out if ranges are able to compete with "boomer loops" and the C++17 STL in terms of performance. "Zero-cost abstraction" became some sort of sarcastic joke online, so I am going to find out how much the abstraction really costs. We are going to measure two things: the number of `real` invocations, and the actual runtime under different optimisations. Here are the twelve variants:

| # | Standard | Variant |
|---|---|---|
| 1 | C++17 | boomer loop |
| 2 | C++17 | `std::min_element` with a comparison |
| 3 | C++17 | `std::transform` and `std::min_element` with an intermediate scratch buffer |
| 4 | C++17 | `std::transform_reduce`[^accum] |
| 5 | C++20 | `ranges::min_element` + `views::transform` |
| 6 | C++20 | `ranges::min_element` + projection |
| 7 | C++20 | `ranges::min` + `views::transform` |
| 8 | C++20 | `ranges::min` + projection |
| 9 | C++23 | `ranges::fold_left` + `views::transform` |
| 10 | C++17 | boomer loop with `std::min` -- does the extra function call cost anything? |
| 11 | C++17 | `std::accumulate` -- does it differ from `std::transform_reduce`? |
| 12 | C++17 | `std::transform` + `std::min_element`, buffer allocated every call -- how much is the allocation? |

[^accum]: `std::accumulate` gives timings indistinguishable from `std::transform_reduce` at every size, on every compiler I tried, so I left it out of the tables for brevity. It is variant 11 in the benchmark source if you want to check for yourself.

Variants 10 to 12 are not really separate approaches -- each isolates a single question -- so they live in the [appendix](#appendix-the-remaining-benchmarks) rather than in the tables below.

C++23 is even newer, and I include variant 9 not to show off but because [`ranges::fold_*`](https://en.cppreference.com/cpp/algorithm/ranges/fold_left) is, to some extent, a range-based analogue of `transform_reduce`:

```cpp
return std::ranges::fold_left(
    object_with_entries.entries | std::views::transform(&Entry::real),
    std::numeric_limits<double>::infinity(),
    [](const double a, const double b) { return std::min(a, b); });
```

### How this was measured

The benchmark source is [bench.cpp](bench.cpp). Each variant is a functor, invoked `inner` times per round over several rounds; the reported figure is the **best round**, divided by `inner`. Counts come from a separate build in which `real()` increments a global. That is an observable side effect, so no optimiser may drop a call[^counts].

[^counts]: According to my check, the count tables stay identical when computed by the same compiler with different optimisation flags, so I only provide counts per standard library.

MSVC figures come from my own machine (MSVC 19.51, `/O2`); GCC and Clang figures from Compiler Explorer, which is shared hardware. **Absolute times are therefore not comparable between compilers** -- only within a single column. That is why the multiplier tables exist: a ratio taken inside one run cancels the machine out.

Two sizes are reported throughout and never mixed. They measure genuinely different things: N=2048 is 32 KB and sits comfortably in cache, so the computation dominates; N=131072 is 2 MB and streams, so memory bandwidth starts to matter. The same code can look two and a half times worse in one regime than the other.

### How many times is `real()` actually called?

With N=8, so 8 means "once per element" and 15 means `2N - 1`:

| # | variant | MS STL | libstdc++ | libc++ |
|---|---|---|---|---|
| 1 | boomer loop | 8 | 8 | 8 |
| 2 | `min_element` + comparator | 15 | 15 | 15 |
| 3 | `transform` + `min_element` (scratch) | 8 | 8 | 8 |
| 4 | `transform_reduce` | 8 | 8 | 8 |
| 5 | `ranges::min_element` + transform | 15 | 15 | 15 |
| 6 | `ranges::min_element` + projection | 15 | 15 | 15 |
| **7** | **`ranges::min` + transform** | **15** | **8** | **8 -- 15**[^libcxx] |
| 8 | `ranges::min` + projection | 15 | 15 | 15 |
| 9 | `fold_left` + transform | 8 | 8 | 8 |

[^libcxx]: libc++'s count depends on the *data*, not just the implementation: it evaluates once per element, plus once more every time it finds a new best. So a range whose minimum comes first costs `N`, and one that improves at every step costs `2N - 1`. Random data averages `N + ln N`, which is what my benchmark measures and why libc++ looks optimal there -- at `N = 131072` the overhead is about twelve evaluations out of a hundred and thirty thousand. Hand it a descending-sorted range and it degrades to Microsoft's figure. libstdc++ evaluates once per element regardless of order, and MS STL evaluates `2N - 1` regardless of order.

As you can see, the results are a little disappointing: most C++20 implementations behave like `min_element` plus a comparator -- they do not cache the result of the transform. Row 7 is the surprise: the same conforming call, and three implementations give three different answers. Nothing in the standard forbids any of them, which is a story I will come back to.

We still have a hope that these `2N - 1` calls may be optimised out, so let's see the timings.

### What do you actually pay?

Multiplier against the boomer loop, measured inside each configuration:

**N = 2048 (32 KB, in cache)**

| # | variant | MSVC /O2 | gcc -O2 | gcc -O3 | gcc -O3 opaque | clang -O3 |
|---|---|---|---|---|---|---|
| 2 | `min_element` + comparator | 4.75 | 4.23 | **1.04** | 3.94 | 3.25 |
| 3 | `transform` + `min_element` | 1.02 | 1.44 | 1.17 | 1.18 | 1.13 |
| 4 | `transform_reduce` | 1.00 | 1.01 | 1.01 | 1.01 | 1.00 |
| 5 | `ranges::min_element` + transform | 4.77 | 1.52 | **1.06** | 4.15 | 2.31 |
| 6 | `ranges::min_element` + projection | 4.78 | 4.42 | **1.05** | 4.13 | 3.25 |
| 7 | `ranges::min` + transform | 4.77 | 1.06 | 1.05 | **1.03** | 0.99 |
| 8 | `ranges::min` + projection | 4.77 | 2.78 | **1.04** | 2.62 | 3.25 |
| 9 | `fold_left` + transform | 1.17 | 1.05 | 1.05 | **1.04** | 0.97 |

**N = 131072 (2 MB, streaming)**

| # | variant | MSVC /O2 | gcc -O2 | gcc -O3 | gcc -O3 opaque | clang -O3 |
|---|---|---|---|---|---|---|
| 2 | `min_element` + comparator | 2.09 | 1.78 | **1.01** | 1.69 | 1.77 |
| 3 | `transform` + `min_element` | 1.05 | 1.09 | 1.07 | 1.10 | 1.10 |
| 4 | `transform_reduce` | 1.00 | 1.00 | 0.99 | 1.02 | 1.00 |
| 5 | `ranges::min_element` + transform | 2.10 | 1.03 | 1.04 | 1.80 | 1.59 |
| 6 | `ranges::min_element` + projection | 2.09 | 1.88 | **1.02** | 1.79 | 1.77 |
| 7 | `ranges::min` + transform | 2.10 | 1.05 | 1.05 | **1.01** | 1.06 |
| 8 | `ranges::min` + projection | 2.09 | 1.57 | **1.02** | 1.49 | 1.77 |
| 9 | `fold_left` + transform | 1.08 | 1.04 | 1.04 | **1.01** | 0.99 |

### And in real time?

Multipliers are honest but abstract, so here is one column of absolute numbers -- MSVC `/O2`, my machine, where everything is comparable to everything else:

| # | variant | N = 2048 | N = 131072 |
|---|---|---|---|
| 1 | boomer loop | 0.0057 ms · 2.80 ns/elem | 0.837 ms · 6.38 ns/elem |
| 2 | `min_element` + comparator | 0.0271 ms · 13.23 | 1.749 ms · 13.34 |
| 4 | `transform_reduce` | 0.0057 ms · 2.79 | 0.840 ms · 6.41 |
| 7 | `ranges::min` + transform | 0.0272 ms · 13.29 | 1.753 ms · 13.38 |
| 9 | `fold_left` + transform | 0.0067 ms · 3.25 | 0.902 ms · 6.88 |

### Explaining the results

Three layers decide what you pay. The **standard** fixes how many comparisons and projections happen. The **library** decides how many times it dereferences -- and that is unconstrained, which is why three implementations disagree. The **optimiser** decides how many of those survive, but only while it can see inside your function. Your source code is one input out of four.

Three things stand out.

**The same call gives three different answers.** Row 7 -- `ranges::min` over a transform view -- costs 8 calls on libstdc++, between 8 and 15 on libc++ depending on the data, and 15 on Microsoft's STL whatever you feed it.

**The optimiser can hide the difference, sometimes.** Put the `gcc -O3` and `gcc -O3 opaque` columns side by side: the first is flat, the second is not. It is evidence that GCC was impressively capable of optimising two `real()` calls into one when it can assume that `real()` has no side effects. Unfortunately, other compilers were not that successful: with MSVC, for instance, we have to pay the cost of doubled `real()` invocation everywhere it happens.

**Two STL spellings are safe everywhere.** `fold_left` never exceeds 1.17 in any cell of either table -- five configurations, two cache regimes, opaque and inlinable alike. `transform_reduce` sits at 1.00 almost throughout. Everything else depends on which library you link against and which flags you pass.

Before writing this post I assumed that, as long as we do not hand `ranges::min`/`ranges::min_element` a comparator that evaluates `real` twice, these new versions would benefit from being able to cache the current minimal `real`. In theory the range versions could produce machine code resembling a boomer loop without any help from the optimiser at all. Roughly, I was expecting something like this from the STL implementation:

```cpp
template <ranges::forward_range R, class Proj, class Comp>
constexpr auto min_element(R&& r, Comp comp = {}, Proj proj = {})
{
    auto s = ranges::end(r);
    auto iter = ranges::begin(r);
    if (s == iter) return s;

    // `cached_proj_iterator` conditionally stores projection result alongside the iterator:
    cached_proj_iterator result{ iter, std::invoke(proj, *iter) };

    while (++iter != s)
    {
        decltype(auto) v = std::invoke(proj, *iter);
        if (std::invoke(comp, v, *result)) result = { iter, v };
    }
    return result.base();
}
```

Setting aside the minor issues (how to forward `v` into `result` properly, and the fact that `cached_proj_iterator` is my invention and would need real elaboration), this looks to me like a viable way to build foolproof range-based algorithms. Apparently it does not look that way to the implementers of the standard library.

Note that a compiler will happily perform exactly this transformation when you let it. The assembly GCC emits at `-O3` folds a running `double` with `minsd` and calls `cos` once per element, on an algorithm the library wrote entirely in terms of iterators. So the caching is not fantasy. It just happens one layer too late, and only when the optimiser can see through everything.

In an attempt to find out the actual reasons behind this choice, we could look into the standard itself. Docs for `std::ranges::min` with projection [[alg.min.max/7](https://eel.is/c++draft/alg.min.max#7)] state the following:

> Complexity: Exactly `ranges::distance(r) - 1` comparisons and twice as many applications of the projection, if any.

Almost the same thing is stated about `std::min_element` with projection: [[alg.min.max/27](https://timsong-cpp.github.io/cppwp/n4868/alg.min.max#27)] says that the projection is applied `2N - 2` times. So we have to call `real` exactly 14 times regardless (plus one to dereference the result) whenever these algorithms run with a non-trivial projection, and that explains why all three implementations agree on 15 in rows 6 and 8.

Now look closer at the two clauses. "If any" is present in the `ranges::min` wording and **suspiciously missing** from the `min_element` one -- and that tiny difference predicts the table:

| clause | wording | libstdc++ |
|---|---|---|
| `ranges::min` [alg.min.max/7] | "...applications of the projection, **if any**" | 8 |
| `ranges::min_element` [alg.min.max/27] | "...twice as many projections" | 15 |

libstdc++ optimises exactly the one that has the escape hatch, and libc++ partly does the same. I am not sure the committee intended these lines as anything more than an upper bound on runtime complexity, but a sufficiently pedantic reading forces us to give up not only rows 6 and 8, but row 5 as well. As far as I can tell this reading has not been written up before: every discussion of ranges performance I could find is about composing views, not about what the algorithms themselves are obliged to do.

This is only a guess, but it matches the results we see in the tables. Are there other reasons that could make the choice deliberate? I could not find any convincing ones. The best candidate is performance in the case where the range is simply a container, like `std::vector<T>` -- probably the most frequently encountered and important case of all. If we cache a `const T&` on every update to the minimum, that costs an extra pointer-sized copy and another variable or register to keep alive. Honestly, that does not feel like a big deal, especially since it looks trivial to optimise away, unlike calling the transform twice. But my expertise ends here. If you know the answer, I would genuinely like to hear it.

It is worth noting that my invented `min_element` caches values in a way that might resemble [`views::cache_latest`](https://en.cppreference.com/cpp/ranges/cache_latest_view). The difference is that `cache_latest` has to be reached for by hand -- unnecessary for our min example, I would argue. It also was designed to solve a different problem: an inherent limitation of C++ iterators that prevents certain traversals from being implemented efficiently at all, `views::filter` being the classic case. I had wanted to cover that subject here too, but this post is already long enough, so it will have to wait for another one. If that subject interests you, Barry Revzin's keynote [*Iterators and Ranges: Comparing C++ to D, Rust, and Others*](https://www.youtube.com/watch?v=95uT0RhMGwA) is the video I definitely recommend.


## The verdict

This post is finally coming to its end; it is time to summarise what we have found out.

### Design

Firstly, I hope that the benefits of C++20 ranges for the design were convincing. The STL before ranges posed too many obstacles that force us to resort to custom loops more often than in other languages. Writing clean interfaces like returning a view over a container was also impossible without inventing a wheel -- there were simply no tools for this in the library. C++20 turns things around, making the language more complete than ever before. For our study case -- all three complaints from the summary section are answered:

 - One argument for one range, instead of an expression repeated twice with `.cbegin()` and `.cend()` glued on.
 - The function we already had, used directly, instead of smuggled into a comparator that pretends to be about ordering.
 - A name that says what we wanted: `min`, not `accumulate` with a suspiciously asymmetric lambda.

The end result

```cpp
const double result = std::ranges::min(object_with_entries.entries | transform(&Entry::real));
```

reads almost exactly like the Python line we started with, which was the whole point.

### Performance

The performance case is messier than I wanted it to be. The idiomatic spelling costs about twice the hand-written loop on Microsoft's STL, and something between 1.0 and 4.4 times on GCC and Clang depending on the spelling, the flags, and whether your projection happens to inline. I cannot write it off with "zero-cost abstraction" and move on.

The choice of minimal element for my study was not deliberate, and I am glad that I have been lucky enough to discover that in the current state of things: **no library implementation is able to provide a foolproof experience when working with transforming views**. Moreover, **the standard itself seems unable to fully handle the undoubtedly complex problem of incorporating modern concepts into an extensive language and its legacy**.

Then fun stuff happens, an unexpected part of the study. When the standard, whether by accident or not, opens the door for library implementers to further optimise the C++20 experience, they start disagreeing: libstdc++ evaluates once per element; Microsoft's evaluates twice; libc++ lands somewhere in between depending on your data.

This study also yields some precise advice for writing performant code that involves ranges. Here they are:

 - Prefer the algorithm that returns a value over the one that returns an iterator, when the value is all you wanted.
 - Avoid passing an expensive projection to an algorithm that compares elements pairwise -- that is the one case the standard actively prevents from being optimised.
 - If you need a guarantee rather than a probability, use `ranges::fold_left` instead of the `ranges::min_element` and the like.

And I want to finish with a positive observation on performance: **`ranges` are never worse than their direct pre-ranges counterparts in the STL library.**

So the performance analysis does not change the recommendation I started with. I still use ranges, I still think they are the better way to write this code, and I would still rather read the one-liner than the loop. The general C++ guideline remains as brutal as always: you need to have clear knowledge of what happens under the hood if you write performance-critical code, be it the STL ranges, third-party libraries or your custom loops. Pick your tools carefully.

Ranges add new possibilities that simply have not existed before. However, they are not always as performant as custom loops, and it is worth knowing when.

### Last words

There are more performance stories in ranges that have nothing to do with any of the above. For instance:

 - A `views::transform` piped into a `views::filter` will evaluate the transform twice for every element that survives.
 - A `views::concat` is always slightly more expensive than nested loops, since it needs to dispatch for the type of the container at each operation.

These two are not library oversights -- they fall out of the external iteration model itself. C++26 adds `views::cache_latest` specifically to work around the first point, and the second one needs more elaborate solutions. That deserves its own post, and it will get one. Note that these are not important if you are choosing between the STL without ranges and the STL with ranges: the latter only adds possibilities on top of the former.

**And a reflection, because I did not expect to end up here.** I sat down to write a short piece arguing that ranges are nicer and no slower. The first half went to plan. The second half turned into three weeks of benchmarks, a disassembly, and a much better understanding of how little the standard actually promises about the work an algorithm does.

I wish new `std::min_element` and all the other modern C++20 algorithms were smarter about transforming views, but I admit that it is not a realistic desire: the standard already made a huge leap with C++20 and it cannot easily please everyone without breaking something else. So what I would like to see change now is small and specific. `ranges::min` and friends could evaluate each element once -- libstdc++ already proves it is possible, conforming, and free. I have filed [microsoft/STL#6404](https://github.com/microsoft/STL/issues/6404) asking for exactly that, and offered to write the patch. And I would gently suggest that "Exactly N-1 comparisons and twice as many projections" is a strange thing to promise: an upper bound would let implementations be clever, whereas an equality obliges them to be wasteful in the one case where waste is most expensive.

Finally, I believe that repeating transforms are going to be an increasing challenge to optimising compilers with C++20 and above, so I also opened this request for the MSVC compiler devs: (TODO).


## Appendix: the remaining benchmarks

Three variants were left out of the main tables because they are not really separate approaches -- each one isolates a single question. Multipliers against the boomer loop, as before.

**Variant 10 -- boomer loop with `std::min`.** Does routing the comparison through a function call cost anything?

| N | MSVC /O2 | gcc -O2 | gcc -O3 | gcc -O3 opaque | clang -O3 |
|---|---|---|---|---|---|
| 2048 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 |
| 131072 | 1.00 | 1.01 | 1.02 | 1.00 | 1.00 |

No. Not on any compiler, at any size, in any configuration. We can fancy a little STL usage here, if compile times allow.

**Variant 11 -- `std::accumulate`.** Does it differ from `std::transform_reduce`?

| N | MSVC /O2 | gcc -O2 | gcc -O3 | gcc -O3 opaque | clang -O3 |
|---|---|---|---|---|---|
| 2048 | 1.00 | 1.00 | 1.00 | 0.99 | 1.00 |
| 131072 | 1.00 | 1.01 | 1.00 | 1.00 | 1.00 |

Also no.

**Variant 12 -- `std::transform` into a buffer allocated on every call.** How much is the allocation itself?

| N | MSVC /O2 | gcc -O2 | gcc -O3 | gcc -O3 opaque | clang -O3 |
|---|---|---|---|---|---|
| 2048 | 1.15 | 1.46 | 1.18 | 1.17 | 1.11 |
| 131072 | 1.15 | 1.09 | 1.08 | 1.10 | 1.09 |

Between 8% and 46%, against 1--10% for the same code with the buffer reused. So the allocation is real but modest. For our study it cost about as much as choosing the wrong `min` spelling, and rather less than getting the projection wrong.
