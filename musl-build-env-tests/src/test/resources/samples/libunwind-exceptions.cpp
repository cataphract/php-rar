#include <cstdio>
#include <stdexcept>

namespace {

constexpr int frame_count = 8;

struct frame_guard {
    explicit frame_guard(int *destroyed) : destroyed_(destroyed) {}
    ~frame_guard() { ++*destroyed_; }

    int *destroyed_;
};

[[gnu::noinline]] void unwind_frame(int depth, int *destroyed)
{
    frame_guard guard(destroyed);
    if (depth == 1) {
        throw std::runtime_error("expected exception");
    }
    unwind_frame(depth - 1, destroyed);
}

} // namespace

int main()
{
    int destroyed = 0;
    try {
        unwind_frame(frame_count, &destroyed);
    } catch (const std::runtime_error &error) {
        if (destroyed != frame_count) {
            std::fprintf(stderr, "destroyed %d of %d frames\n", destroyed,
                         frame_count);
            return 1;
        }
        if (error.what() == nullptr) {
            return 2;
        }
        std::printf("exception unwound through %d frames\n", destroyed);
        return 0;
    } catch (...) {
        std::fputs("caught an unexpected exception type\n", stderr);
        return 3;
    }

    std::fputs("exception was not thrown\n", stderr);
    return 4;
}
