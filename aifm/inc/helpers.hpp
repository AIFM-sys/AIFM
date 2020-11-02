#pragma once

extern "C" {
#include <net/ip.h>
#include <runtime/tcp.h>
}

#include <string>

#define NOT_COPYABLE(TypeName)                                                 \
  TypeName(TypeName const &) = delete;                                         \
  TypeName &operator=(TypeName const &) = delete;

#define NOT_MOVEABLE(TypeName)                                                 \
  TypeName(TypeName &&) = delete;                                              \
  TypeName &operator=(TypeName &&) = delete;

#define ABS(x) ((x) > 0 ? (x) : -(x))
#define DONT_OPTIMIZE(var) __asm__ __volatile__("" ::"m"(var));

#ifdef DEBUG
#define FORCE_INLINE inline
#else
#define FORCE_INLINE inline __attribute__((always_inline))
#endif

#define LOG_PRINTF(...)                                                        \
  {                                                                            \
    preempt_disable();                                                         \
    fprintf(stderr, __VA_ARGS__);                                              \
    preempt_enable();                                                          \
  }

#ifdef DEBUG
#define LOG_PRINTF_DBG(...) LOG_PRINTF(__VA_ARGS__)
#else
#define LOG_PRINTF_DBG(...)
#endif

#define TEST_ASSERT(x)                                                         \
  if (!(x)) {                                                                  \
    std::cout << "Assert failure on line " << __LINE__ << std::endl;           \
    exit(-1);                                                                  \
  }

#define FOR_ALL_SOCKET0_CORES(core_id)                                         \
  for (uint32_t core_id = 0; core_id < helpers::kNumCPUs; core_id++)

#define CachelineAligned(type)                                                 \
  struct alignas(64) CachelineAligned_##type {                                 \
    type data;                                                                 \
  }

namespace helpers {

template <typename T> concept Addable = requires(T x) { x + x; };
template <typename T> concept DividableByInt = requires(T x) { x / 2; };
template <typename T>
struct FunctionTraits : public FunctionTraits<decltype(&T::operator())> {};
template <typename ClassType, typename ReturnType, typename... Args>
struct FunctionTraits<ReturnType (ClassType::*)(Args...) const>;

constexpr uint8_t kPageShift = 12;
constexpr uint32_t kPageSize = (1 << kPageShift);
constexpr uint8_t kHugepageShift = 21;
constexpr uint32_t kHugepageSize = (1 << kHugepageShift);
constexpr uint8_t kNumCPUs = 20;
constexpr uint8_t kNumSocket1CPUs = 24;

static uint64_t round_to_hugepage_size(uint64_t size);
static void *allocate_hugepage(uint64_t size);
static int get_num_cores();
static void timer_start(unsigned *cycles_high_start,
                        unsigned *cycles_low_start);
static void timer_end(unsigned *cycles_high_end, unsigned *cycles_low_end);
static uint64_t get_elapsed_cycles(unsigned cycles_high_start,
                                   unsigned cycles_low_start,
                                   unsigned cycles_high_end,
                                   unsigned cycles_low_end);
template <size_t n> static void small_memcpy(void *_dest, const void *_src);
template <size_t n> static void small_memset(void *_dest, uint8_t data);
template <int I, class... Ts>
static constexpr decltype(auto) variadic_get(Ts &&... ts);
template <typename F> static void execute_until(F &&f, uint64_t latency_us);
template <class F> static auto finally(F f) noexcept(noexcept(F(std::move(f))));
static void dump_core();
static void *memcpy_ermsb(void *dst, const void *src, size_t n);
static unsigned long long chrono_to_timestamp(const auto &tp);
static uint32_t bsr_32(uint32_t a);
static uint64_t bsr_64(uint64_t a);
static constexpr uint32_t round_up_power_of_two(uint32_t a);
static uint32_t bsf_32(uint32_t a);
static uint64_t bsf_64(uint64_t a);
static netaddr str_to_netaddr(std::string ip_addr_port);
static void tcp_read_until(tcpconn_t *c, void *buf, size_t expect);
static void tcp_write_until(tcpconn_t *c, const void *buf, size_t expect);
static void tcp_write2_until(tcpconn_t *c, const void *buf_0, size_t expect_0,
                             const void *buf_1, size_t expect_1);
static constexpr size_t static_log(uint64_t b, uint64_t n);
static uint32_t align_to(uint32_t n, uint32_t factor);
static uint64_t align_to(uint64_t n, uint64_t factor);
void breakpoint();
}; // namespace helpers

#include "internal/helpers.ipp"
