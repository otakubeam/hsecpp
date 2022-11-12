#include <cstdint>
#include <fmt/core.h>
#include <iostream>
#include <span>
#include <stdexcept>
#include <stdio.h>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace BigArithmetic {

using BigDigit = uint64_t;

class BigUint {
public:
  BigUint(std::string_view n) {
    n = SkipZeroes(n);

    while (n.size()) {
      char digit = ExtractDigit(n);

      if (CreatesOverflow(digit)) {
        ExtendNumberRight();
      }

      PushBackDigit(digit);
    }
  }

  //////////////////////////////////////////////////////////////////////////////

  struct ComparisonProxy {
    std::span<BigDigit> rest;

    bool operator==(const ComparisonProxy &other) const {
      if (rest.size() != other.rest.size()) {
        return false;
      }
      auto a = rest.subspan(0), b = other.rest.subspan(0);

      while (a.size() && a.front() == b.front()) {
        b = b.subspan(1), a = a.subspan(1);
      }
      return a.size() ? false : true;
    }

    bool operator<(const ComparisonProxy &other) const {
      auto a = rest.subspan(0), b = other.rest.subspan(0);

      while (a.size() && a.front() == b.front()) {
        b = b.subspan(1), a = a.subspan(1);
      }
      return a.size() ? false : a.front() < b.front();
    }
  };

  //////////////////////////////////////////////////////////////////////////////

  bool operator<(const BigUint &other) const {
    if (Size() == other.Size()) {
      return ComparisonProxy{data} < ComparisonProxy{other.data};
    }
    return Size() < other.Size();
  }

  bool operator==(const BigUint &other) const {
    if (Size() != other.Size()) {
      return false;
    }
    return ComparisonProxy{data} == ComparisonProxy{other.data};
  }

  //////////////////////////////////////////////////////////////////////////////

  auto OrderedRefs(const BigUint &other) const
      -> std::tuple<const BigUint &, const BigUint &> {
    return (*this < other) ? std::tie(*this, other) : std::tie(other, *this);
  }

  BigUint &operator+=(const BigUint &other) { return *this = *this + other; }

  BigUint operator+(const BigUint &other) {
    const auto &[smaller, bigger] = OrderedRefs(other);

    BigUint result{bigger};

    auto span_small = smaller.SpanFromData();
    auto span_res = result.MutSpanFromData();

    bool overflow = false;

    while (span_small.size()) {
      span_res.back() += span_small.back() + std::exchange(overflow, false);

      if (span_res.back() >= BIGDIGIT_BASE) {
        span_res.back() -= BIGDIGIT_BASE;
        overflow = true;
      }

      ShrinkSpan(span_res);
      ShrinkSpan(span_small);
    }

    if (overflow) {
      if (span_res.size()) {
        span_res.back() += 1;
      } else {
        // Could not have predicted this!
        result.ExtendNumberLeft();
        result.data.front() = 1;
      }
    }

    return result;
  }

  //////////////////////////////////////////////////////////////////////////////

  /// Subtract bigger BigUint from smaller BigUint
  BigUint operator-(const BigUint &other) {
    const auto &[smaller, bigger] = OrderedRefs(other);

    BigUint result{bigger};

    auto span_small = smaller.SpanFromData();
    auto span_res = result.MutSpanFromData();

    while (span_small.size()) {
      if (span_res.back() < span_small.back()) {
        result.LoanOne(&span_res.back());
      }
      span_res.back() -= span_small.back();
      CarryOverflow();

      ShrinkSpan(span_res);
      ShrinkSpan(span_small);
    }

    return result;
  }

  //////////////////////////////////////////////////////////////////////////////

  friend std::ostream &operator<<(std::ostream &out, const BigUint &n) {
    auto iter = n.data;
    while (iter.size()) {
      out << iter.front();
      iter = iter.subspan(1);
    }
    return out;
  }

private:
  friend std::ostream &operator<<(std::ostream &out, BigDigit digit) {
    if (digit == 0) {
      return out << 0;
    }

    while (digit) {
      out << digit % 10;
      digit /= 10;
    }

    return out;
  }

  BigDigit *FindNonZeroToLeft(BigDigit *iterate) {
    while (iterate != &data[0]) {
      iterate -= 1;
      if (*iterate > 0) {
        return iterate;
      }
    }
    return nullptr;
  }

  void CarryOverflow() {
    auto a = data.subspan(0);
    while (a.back() > BIGDIGIT_BASE) {
      a.back() -= BIGDIGIT_BASE;
      ShrinkSpan(a);
      a.back() += 1;
    }
  }

  void LoanOne(BigDigit *iterate) {
    auto find = FindNonZeroToLeft(iterate);
    *find -= 1; // Take one from the higher power
    for (find += 1; find <= &data.back(); find++) {
      *find += BIGDIGIT_BASE;
    }
  }

  template <typename T> //
  static void ShrinkSpan(std::span<T> &span) {
    span = span.first(span.size() - 1);
  }

  std::size_t Size() const { return data.size(); }

  std::span<const uint64_t> SpanFromData() const {
    return std::span{data.begin(), data.end()};
  }

  std::span<uint64_t> MutSpanFromData() {
    return std::span{data.begin(), data.end()};
  }

  void PushBackDigit(char digit) {
    data.back() *= 10;
    data.back() += digit;
  }

  bool CreatesOverflow(char digit) {
    return data.back() * DECIMAL_BASE + digit > BIGDIGIT_BASE;
  }

  std::string_view SkipZeroes(std::string_view n) {
    while (n.size() && n.front() == '0') {
      n = n.substr(1);
    }
    return n;
  }

  char ExtractDigit(std::string_view &v) {
    Assert(v.size());
    Assert(v.at(0) != '0');
    char result = v.at(0);
    v.remove_prefix(1);
    return result - '0';
  }

  void Assert(bool cond) {
    if (!cond) {
      throw std::runtime_error{"Assertion failed"};
    }
  }

  // For adding a new digit (during input)
  // 13123132 <<<---- 1
  void ExtendNumberRight() {
    if (storage.end().base() != data.end().base()) {
      data = std::span{data.begin(), data.size() + 1};
    } else {
      ReallocateStorage();
    }
  }

  // For overflowing (during operations)
  // 1 ------->>> 123141
  void ExtendNumberLeft() {
    if (storage.begin().base() != data.begin().base()) {
      data = std::span{data.begin() - 1, data.size() + 1};
    } else {
      ReallocateStorage();
    }
  }

  void ReallocateStorage() {
    std::abort(); // Unimplemented!
  }

private:
  static constexpr uint8_t DECIMAL_BASE = 10;

  // Release/Debug
  // static constexpr size_t BIGDIGIT_BASE = 1e18;
  static constexpr size_t BIGDIGIT_BASE = 1e1;

  std::vector<BigDigit> storage = std::vector<BigDigit>(1000, 0);
  std::span<BigDigit> data{storage.begin() + 500, 1};
};

} // namespace BigArithmetic

int main() {
  std::string number;

  std::cin >> number;
  BigArithmetic::BigUint a{std::string_view{number}};

  std::cin >> number;
  BigArithmetic::BigUint b{std::string_view{number}};

  std::cout << a + b << std::endl;
  std::cout << a - b << std::endl;
}
