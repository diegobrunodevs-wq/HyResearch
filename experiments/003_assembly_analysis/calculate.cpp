#include "calculate.hpp"

#if defined(_MSC_VER)
#define NO_INLINE __declspec(noinline)
#else
#define NO_INLINE __attribute__((noinline))
#endif

NO_INLINE std::int64_t calculate(std::int64_t a, std::int64_t b)
{
    return (a * 3) + (b * 2);
}
