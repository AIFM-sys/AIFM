#pragma once

extern "C" {
#include <base/time.h>
#include <runtime/preempt.h>
#include <runtime/timer.h>
}

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <signal.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>

namespace helpers {

static FORCE_INLINE uint64_t round_to_hugepage_size(uint64_t size) {
  return ((size - 1) / helpers::kHugepageSize + 1) * helpers::kHugepageSize;
}

static FORCE_INLINE void *allocate_hugepage(uint64_t size) {
  size = round_to_hugepage_size(size);
  void *ptr = nullptr;
  preempt_disable();
  int fail = posix_memalign(&ptr, kHugepageSize, size);
  BUG_ON(fail);
  BUG_ON(madvise(ptr, size, MADV_HUGEPAGE) != 0);
  preempt_enable();
  return ptr;
}

static FORCE_INLINE int get_num_cores() { return get_nprocs(); }

static FORCE_INLINE void timer_start(unsigned *cycles_high_start,
                                     unsigned *cycles_low_start) {
  asm volatile("xorl %%eax, %%eax\n\t"
               "CPUID\n\t"
               "RDTSC\n\t"
               "mov %%edx, %0\n\t"
               "mov %%eax, %1\n\t"
               : "=r"(*cycles_high_start), "=r"(*cycles_low_start)::"%rax",
                 "%rbx", "%rcx", "%rdx");
}

static FORCE_INLINE void timer_end(unsigned *cycles_high_end,
                                   unsigned *cycles_low_end) {
  asm volatile("RDTSCP\n\t"
               "mov %%edx, %0\n\t"
               "mov %%eax, %1\n\t"
               "xorl %%eax, %%eax\n\t"
               "CPUID\n\t"
               : "=r"(*cycles_high_end), "=r"(*cycles_low_end)::"%rax", "%rbx",
                 "%rcx", "%rdx");
}

static FORCE_INLINE uint64_t get_elapsed_cycles(unsigned cycles_high_start,
                                                unsigned cycles_low_start,
                                                unsigned cycles_high_end,
                                                unsigned cycles_low_end) {
  uint64_t start, end;
  start = ((static_cast<uint64_t>(cycles_high_start) << 32) | cycles_low_start);
  end = ((static_cast<uint64_t>(cycles_high_end) << 32) | cycles_low_end);
  return end - start;
}

template <size_t n>
static FORCE_INLINE void small_memcpy(void *_dest, const void *_src) {
  static_assert(n <= 8, "helpers::small_memcpy is only suitable for n <= 8.");

  uint8_t *dest = reinterpret_cast<uint8_t *>(_dest);
  const uint8_t *src = reinterpret_cast<const uint8_t *>(_src);
  if constexpr (n == 8) {
    ACCESS_ONCE(*reinterpret_cast<uint64_t *>(dest)) =
        *reinterpret_cast<const uint64_t *>(src);
    helpers::small_memcpy<n - 8>(dest + 8, src + 8);
  } else if constexpr (n >= 4) {
    ACCESS_ONCE(*reinterpret_cast<uint32_t *>(dest)) =
        *reinterpret_cast<const uint32_t *>(src);
    helpers::small_memcpy<n - 4>(dest + 4, src + 4);
  } else if constexpr (n >= 2) {
    ACCESS_ONCE(*reinterpret_cast<uint16_t *>(dest)) =
        *reinterpret_cast<const uint16_t *>(src);
    helpers::small_memcpy<n - 2>(dest + 2, src + 2);
  } else if constexpr (n >= 1) {
    ACCESS_ONCE(*reinterpret_cast<uint8_t *>(dest)) =
        *reinterpret_cast<const uint8_t *>(src);
    helpers::small_memcpy<n - 1>(dest + 1, src + 1);
  }
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
template <size_t n>
static FORCE_INLINE void small_memset(void *_dest, uint8_t data) {
  static_assert(n <= 8, "helpers::small_memset is only suitable for n <= 8.");
  uint8_t *dest = reinterpret_cast<uint8_t *>(_dest);
  uint16_t data_16 = (static_cast<uint16_t>(data) << 8) | data;
  uint32_t data_32 = (static_cast<uint32_t>(data_16) << 16) | data_16;
  uint64_t data_64 = (static_cast<uint64_t>(data_32) << 32) | data_32;
  if constexpr (n == 8) {
    ACCESS_ONCE(*reinterpret_cast<uint64_t *>(dest)) = data_64;
    helpers::small_memset<n - 8>(dest + 8, data);
  } else if constexpr (n >= 4) {
    ACCESS_ONCE(*reinterpret_cast<uint32_t *>(dest)) = data_32;
    helpers::small_memset<n - 4>(dest + 4, data);
  } else if constexpr (n >= 2) {
    ACCESS_ONCE(*reinterpret_cast<uint16_t *>(dest)) = data_16;
    helpers::small_memset<n - 2>(dest + 2, data);
  } else if constexpr (n >= 1) {
    ACCESS_ONCE(*reinterpret_cast<uint8_t *>(dest)) = data;
    helpers::small_memset<n - 1>(dest + 1, data);
  }
}
#pragma GCC diagnostic pop

template <int I, class... Ts>
static FORCE_INLINE constexpr decltype(auto) variadic_get(Ts &&... ts) {
  static_assert(I < sizeof...(ts));
  return std::get<I>(std::forward_as_tuple(ts...));
}

template <typename F>
static FORCE_INLINE void execute_until(F &&f, uint64_t latency_us) {
  auto start_us = latency_us ? microtime() : 0;
  f();
  auto end_us = latency_us ? microtime() : 0;
  if (latency_us > (end_us - start_us)) {
    timer_sleep(latency_us - (end_us - start_us));
  }
}

template <class F>
static FORCE_INLINE auto finally(F f) noexcept(noexcept(F(std::move(f)))) {
  auto x = [f = std::move(f)](void *) { f(); };
  return std::unique_ptr<void, decltype(x)>(reinterpret_cast<void *>(1),
                                            std::move(x));
}

// For debugging.
static FORCE_INLINE void dump_core() { raise(SIGABRT); }

static FORCE_INLINE void *memcpy_ermsb(void *dst, const void *src, size_t n) {
  asm volatile("rep movsb" : "+D"(dst), "+S"(src), "+c"(n)::"memory");
  return dst;
}

static FORCE_INLINE unsigned long long chrono_to_timestamp(const auto &tp) {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             tp.time_since_epoch())
      .count();
}

static FORCE_INLINE uint32_t bsr_32(uint32_t a) {
  uint32_t ret;
  asm("BSR %k1, %k0   \n" : "=r"(ret) : "rm"(a));
  return ret;
}

static FORCE_INLINE uint64_t bsr_64(uint64_t a) {
  uint64_t ret;
  asm("BSR %q1, %q0   \n" : "=r"(ret) : "rm"(a));
  return ret;
}

static FORCE_INLINE constexpr uint32_t round_up_power_of_two(uint32_t a) {
  return a == 1 ? 1 : 1 << (32 - __builtin_clz(a - 1));
}

static FORCE_INLINE uint32_t bsf_32(uint32_t a) {
  uint32_t ret;
  asm("BSF %k1, %k0   \n" : "=r"(ret) : "rm"(a));
  return ret;
}

static FORCE_INLINE uint64_t bsf_64(uint64_t a) {
  uint64_t ret;
  asm("BSF %q1, %q0   \n" : "=r"(ret) : "rm"(a));
  return ret;
}

static FORCE_INLINE netaddr str_to_netaddr(std::string ip_addr_port) {
  auto pos0 = ip_addr_port.find('.');
  BUG_ON(pos0 == std::string::npos);
  auto pos1 = ip_addr_port.find('.', pos0 + 1);
  BUG_ON(pos1 == std::string::npos);
  auto pos2 = ip_addr_port.find('.', pos1 + 1);
  BUG_ON(pos2 == std::string::npos);
  auto pos3 = ip_addr_port.find(':', pos2 + 1);
  BUG_ON(pos3 == std::string::npos);
  auto addr0 = stoi(ip_addr_port.substr(0, pos0));
  auto addr1 = stoi(ip_addr_port.substr(pos0 + 1, pos1 - pos0));
  auto addr2 = stoi(ip_addr_port.substr(pos1 + 1, pos2 - pos1));
  auto addr3 = stoi(ip_addr_port.substr(pos2 + 1, pos3 - pos2));
  auto port = stoi(ip_addr_port.substr(pos3 + 1));
  return netaddr{.ip = MAKE_IP_ADDR(addr0, addr1, addr2, addr3),
                 .port = static_cast<uint16_t>(port)};
}

static FORCE_INLINE void tcp_read_until(tcpconn_t *c, void *buf,
                                        size_t expect) {
  size_t real = tcp_read(c, reinterpret_cast<uint8_t *>(buf), expect);
  if (unlikely(real != expect)) {
    // Slow path.
    do {
      real +=
          tcp_read(c, reinterpret_cast<uint8_t *>(buf) + real, expect - real);
    } while (real < expect);
  }
}

static FORCE_INLINE void tcp_write_until(tcpconn_t *c, const void *buf,
                                         size_t expect) {
  size_t real = tcp_write(c, reinterpret_cast<const uint8_t *>(buf), expect);
  if (unlikely(real != expect)) {
    // Slow path.
    do {
      real += tcp_write(c, reinterpret_cast<const uint8_t *>(buf) + real,
                        expect - real);
    } while (real < expect);
  }
}

static FORCE_INLINE void tcp_write2_until(tcpconn_t *c, const void *buf_0,
                                          size_t expect_0, const void *buf_1,
                                          size_t expect_1) {
  iovec iovecs[2];
  iovecs[0] = {.iov_base = const_cast<void *>(buf_0), .iov_len = expect_0};
  iovecs[1] = {.iov_base = const_cast<void *>(buf_1), .iov_len = expect_1};
  size_t real = tcp_writev(c, iovecs, 2);
  if (unlikely(real != expect_0 + expect_1)) {
    // Slow path.
    do {
      if (likely(real >= expect_0)) {
        real += tcp_write(
            c, reinterpret_cast<const uint8_t *>(buf_1) + (real - expect_0),
            expect_1 - (real - expect_0));
      } else {
        iovecs[0].iov_base = reinterpret_cast<void *>(
            reinterpret_cast<uint8_t *>(iovecs[0].iov_base) + real);
        iovecs[0].iov_len -= real;
        real += tcp_writev(c, iovecs, 2);
      }
    } while (real < expect_0 + expect_1);
  }
}

static FORCE_INLINE constexpr size_t static_log(uint64_t b, uint64_t n) {
  return ((n < b) ? 1 : 1 + static_log(b, n / b));
}

template <typename ClassType, typename ReturnType, typename... Args>
struct FunctionTraits<ReturnType (ClassType::*)(Args...) const>
// We specialize for pointers to member function
{
  enum { Arity = sizeof...(Args) };
  // Arity is the number of arguments.

  using ResultType = ReturnType;

  template <size_t N> struct Arg {
    using Type =
        typename std::tuple_element<N, std::tuple<Args..., void>>::type;
    // The N-th argument is equivalent to the N-th tuple element of a tuple
    // composed of those arguments.
  };
};

static FORCE_INLINE uint32_t align_to(uint32_t n, uint32_t factor) {
  return n + (factor - 1) - (n - 1) % (factor);
}

static FORCE_INLINE uint64_t align_to(uint64_t n, uint64_t factor) {
  return n + (factor - 1) - (n - 1) % (factor);
}

} // namespace helpers
