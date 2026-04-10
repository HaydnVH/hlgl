#ifndef HLGL_UTILS_FLAGS_H
#define HLGL_UTILS_FLAGS_H

#include <type_traits>

namespace hlgl {

template <typename BitType>
struct FlagsTraits {
  static constexpr bool isFlagBits = false;
};

template <typename BitType>
struct Flags {
  using MaskType = typename std::underlying_type<BitType>::type;

  // Constructors
  constexpr Flags() noexcept : mask_(0) {}
  constexpr Flags(BitType bit) noexcept : mask_(static_cast<MaskType>(bit)) {}
  constexpr Flags(const Flags<BitType>& rhs) noexcept = default;
  constexpr explicit Flags(MaskType flags) noexcept : mask_(flags) {}

  // Relational operators
#if (true) // TODO: Add a check for whether this version of C++ supports the spaceship operator.
  auto operator <=>(const Flags<BitType>&) const = default;
#else
  constexpr bool operator < (const Flags<BitType>& rhs) const noexcept { return mask_ <  rhs.mask_; }
  constexpr bool operator <=(const Flags<BitType>& rhs) const noexcept { return mask_ <= rhs.mask_; }
  constexpr bool operator > (const Flags<BitType>& rhs) const noexcept { return mask_ >  rhs.mask_; }
  constexpr bool operator >=(const Flags<BitType>& rhs) const noexcept { return mask_ >= rhs.mask_; }
  constexpr bool operator ==(const Flags<BitType>& rhs) const noexcept { return mask_ == rhs.mask_; }
  constexpr bool operator !=(const Flags<BitType>& rhs) const noexcept { return mask_ != rhs.mask_; }
#endif

  // Logical operator
  constexpr bool operator!() const noexcept { return !mask_; }

  // Bitwise operators
  constexpr Flags<BitType> operator |(const Flags<BitType>& rhs) const noexcept { return Flags<BitType>(mask_ | rhs.mask_); }
  constexpr Flags<BitType> operator &(const Flags<BitType>& rhs) const noexcept { return Flags<BitType>(mask_ & rhs.mask_); }
  constexpr Flags<BitType> operator ^(const Flags<BitType>& rhs) const noexcept { return Flags<BitType>(mask_ ^ rhs.mask_); }
  constexpr Flags<BitType> operator ~() const noexcept { return Flags<BitType>(mask_ ^ ((1 << FlagsTraits<BitType>::numBits) - 1)); }

  // Assignment operators
  constexpr Flags<BitType>& operator  =(const Flags<BitType>& rhs) noexcept = default;
  constexpr Flags<BitType>& operator |=(const Flags<BitType>& rhs) noexcept { mask_ |= rhs.mask_; return *this; }
  constexpr Flags<BitType>& operator &=(const Flags<BitType>& rhs) noexcept { mask_ &= rhs.mask_; return *this; }
  constexpr Flags<BitType>& operator ^=(const Flags<BitType>& rhs) noexcept { mask_ ^= rhs.mask_; return *this; }

  // Cast operators
  constexpr operator bool() const noexcept { return (mask_ != 0); }
  constexpr operator MaskType() const noexcept { return mask_; }

private:
  MaskType mask_;
};

// Bitwise operators
template <typename BitType>
constexpr Flags<BitType> operator |(BitType bit, const Flags<BitType>& rhs) noexcept { return rhs.operator|(bit); }
template <typename BitType>
constexpr Flags<BitType> operator &(BitType bit, const Flags<BitType>& rhs) noexcept { return rhs.operator&(bit); }
template <typename BitType>
constexpr Flags<BitType> operator ^(BitType bit, const Flags<BitType>& rhs) noexcept { return rhs.operator^(bit); }

// Bitwise operators on BitType
template <typename BitType, typename std::enable_if<FlagsTraits<BitType>::isFlags, bool>::type = true>
inline constexpr Flags<BitType> operator |(BitType lhs, BitType rhs) noexcept { return Flags<BitType>(lhs) | rhs; }
template <typename BitType, typename std::enable_if<FlagsTraits<BitType>::isFlags, bool>::type = true>
inline constexpr Flags<BitType> operator &(BitType lhs, BitType rhs) noexcept { return Flags<BitType>(lhs) & rhs; }
template <typename BitType, typename std::enable_if<FlagsTraits<BitType>::isFlags, bool>::type = true>
inline constexpr Flags<BitType> operator ^(BitType lhs, BitType rhs) noexcept { return Flags<BitType>(lhs) ^ rhs; }
template <typename BitType, typename std::enable_if<FlagsTraits<BitType>::isFlags, bool>::type = true>
inline constexpr Flags<BitType> operator ~(BitType bit) noexcept { return ~(Flags<BitType>(bit)); }

}

#endif // HLGL_UTILS_FLAGS_H