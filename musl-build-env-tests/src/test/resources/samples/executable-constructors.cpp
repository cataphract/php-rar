#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace {

int lifecycle_state;

void write_message(const char *message)
{
    std::size_t remaining = std::strlen(message);
    while (remaining != 0) {
        ssize_t written = write(STDOUT_FILENO, message, remaining);
        if (written <= 0) {
            std::_Exit(90);
        }
        message += written;
        remaining -= static_cast<std::size_t>(written);
    }
}

class lifecycle_probe {
public:
    lifecycle_probe(int expected_state, int next_state,
                    int destructor_state, int next_destructor_state,
                    const char *constructor_message,
                    const char *destructor_message)
        : destructor_state_(destructor_state),
          next_destructor_state_(next_destructor_state),
          destructor_message_(destructor_message)
    {
        if (lifecycle_state != expected_state) {
            std::_Exit(91);
        }
        lifecycle_state = next_state;
        write_message(constructor_message);
    }

    ~lifecycle_probe()
    {
        if (lifecycle_state != destructor_state_) {
            std::_Exit(92);
        }
        lifecycle_state = next_destructor_state_;
        write_message(destructor_message_);
    }

private:
    int destructor_state_;
    int next_destructor_state_;
    const char *destructor_message_;
};

// Static C++ object destructors are registered through __cxa_atexit while the
// constructors run. Their reverse-order output proves both initialization and
// the corresponding executable cleanup path occurred.
lifecycle_probe first(0, 1, 4, 5, "constructor one\n", "destructor one\n");
lifecycle_probe second(1, 2, 3, 4, "constructor two\n", "destructor two\n");

} // namespace

int main()
{
    if (lifecycle_state != 2) {
        write_message("constructors missing or duplicated\n");
        return 1;
    }
    lifecycle_state = 3;
    write_message("main\n");
    return 0;
}
