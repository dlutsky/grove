#ifndef RIBBON_FILTER_H
#define RIBBON_FILTER_H

#include <cstdint>
#include <memory>
#include <vector>
#include <fastfilter/ribbon/ribbon_impl.h>

template <typename CoeffType, bool kHomog, uint32_t kNumColumns, bool kSmash = false>
struct RibbonTS {
  static constexpr bool kIsFilter = true;
  static constexpr bool kHomogeneous = kHomog;
  static constexpr bool kFirstCoeffAlwaysOne = true;
  static constexpr bool kUseSmash = kSmash;
  using CoeffRow = CoeffType;
  using Hash = uint64_t;
  using Key = uint64_t;
  using Seed = uint32_t;
  using Index = size_t;
  using ResultRow = uint32_t;
  static constexpr bool kAllowZeroStarts = false;
  static constexpr uint32_t kFixedNumColumns = kNumColumns;

  static Hash HashFn(const Hash& input, Seed raw_seed) {
    uint64_t h = input + raw_seed;
    h ^= h >> 33;
    h *= UINT64_C(0xff51afd7ed558ccd);
    h ^= h >> 33;
    h *= UINT64_C(0xc4ceb9fe1a85ec53);
    h ^= h >> 33;
    return h;
  }
};

template <typename T>
class RibbonFilter {
public:
  RibbonFilter(size_t count);
  ~RibbonFilter();

  void addAll(const std::vector<T>& keys);
  bool contain(T key);

private:
  using TS = RibbonTS<T, false, 7, false>;
  IMPORT_RIBBON_IMPL_TYPES(TS);

  static double getNumSlots(size_t count) {
    double overhead = -0.0176 + std::log(1.0 * count) * 1.4427 * 0.0038;
    overhead = std::max(overhead, 0.2);
    return InterleavedSoln::RoundUpNumSlots((size_t)(count + overhead * count));
  }

  size_t num_slots;
  size_t bytes;
  std::unique_ptr<char[]> ptr;
  InterleavedSoln soln;
  Hasher hasher;
};


template <typename T>
RibbonFilter<T>::RibbonFilter(size_t count)
  : num_slots(getNumSlots(count)), bytes(static_cast<size_t>((num_slots * 7 + 7) / 8)), ptr(new char[bytes]), soln(InterleavedSoln(ptr.get(), bytes)) {}

template <typename T>
RibbonFilter<T>::~RibbonFilter() {}

template <typename T>
void RibbonFilter<T>::addAll(const std::vector<T>& keys) {
  Banding b(num_slots);
  if (!b.AddRange(keys.begin(), keys.end())) {
    return;
  }
  soln.BackSubstFrom(b);
}

template <typename T>
bool RibbonFilter<T>::contain(T key) {
  return soln.FilterQuery(key, hasher);
}




#endif