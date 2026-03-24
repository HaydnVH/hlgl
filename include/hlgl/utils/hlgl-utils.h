/* hlgl-utils.h
* This file contains a handful of classes, structs, and functions which are useful for HLGL.
* None of this code is specific to HLGL, any rendering API, or any windowing API, so it could be copied and used in any project.
* MIT license, (C) Haydn V. Harach 2025
**/
#pragma once

namespace hlgl::util {


/// Helper struct for managing flags for custom bitfield enums.
template <typename BitT>
struct Flags {
public:
  using MaskT = std::underlying_type_t<BitT>;

  constexpr Flags(): mask_(0) {}
  constexpr Flags(BitT bit): mask_(static_cast<MaskT>(bit)) {}
  constexpr Flags(const Flags<BitT>& other) = default;
  constexpr Flags(MaskT mask): mask_(mask) {}

  constexpr auto operator<=>(const Flags<BitT>&) const = default;
  constexpr bool operator!() const { return !mask_; }
  constexpr Flags<BitT> operator&(const Flags<BitT>& rhs) const { return Flags<BitT>(mask_ & rhs.mask_); }
  constexpr Flags<BitT> operator|(const Flags<BitT>& rhs) const { return Flags<BitT>(mask_ | rhs.mask_); }
  constexpr Flags<BitT> operator^(const Flags<BitT>& rhs) const { return Flags<BitT>(mask_ ^ rhs.mask_); }
  constexpr Flags<BitT> operator~() const { return Flags<BitT>(~mask_); }
  constexpr Flags<BitT>& operator=(const Flags<BitT>& rhs) = default;
  constexpr Flags<BitT>& operator&=(const Flags<BitT>& rhs) { return (*this = (*this & rhs)); }
  constexpr Flags<BitT>& operator|=(const Flags<BitT>& rhs) { return (*this = (*this | rhs)); }
  constexpr Flags<BitT>& operator^=(const Flags<BitT>& rhs) { return (*this = (*this ^ rhs)); }

  friend constexpr Flags<BitT> operator&(BitT lhs, Flags<BitT> rhs) { return rhs & lhs; }
  friend constexpr Flags<BitT> operator|(BitT lhs, Flags<BitT> rhs) { return rhs | lhs; }
  friend constexpr Flags<BitT> operator^(BitT lhs, Flags<BitT> rhs) { return rhs ^ lhs; }

  constexpr operator bool() const { return !!mask_; }
  constexpr operator MaskT() const { return mask_; }

  constexpr uint32_t bitsInCommon(const Flags<BitT>& other) const {
    uint32_t result {0};
    for (MaskT bit {0}; bit < sizeof(BitT)*8; ++bit) {
      if ((mask_ & MaskT(1 << bit)) && (other & BitT(1 << bit)))
        ++result;
    }
    return result;
  }

private:
  MaskT mask_ {0};
};

// Template specialization to determine whether a type should be considered a bitfield.
// To make a flag enum usable as a bitfield, you would use the following:
//   template <> struct isBitfield<MyEnumFlag> : public std::true_type {};
//   using MyEnumFlags = Flags<MyEnumFlag>;
template <typename BitT> struct isBitfield : public std::false_type {};

// Providing the bitwise and operator for custom bitfield enums.
template <typename BitT> requires (isBitfield<BitT>::value)
constexpr Flags<BitT> operator &(BitT lhs, BitT rhs) {
  return Flags<BitT>(static_cast<std::underlying_type_t<BitT>>(lhs) & static_cast<std::underlying_type_t<BitT>>(rhs));
}

// Providing the bitwise or operator for custom bitfield enums.
template <typename BitT> requires (isBitfield<BitT>::value)
constexpr Flags<BitT> operator |(BitT lhs, BitT rhs) {
  return Flags<BitT>(static_cast<std::underlying_type_t<BitT>>(lhs) | static_cast<std::underlying_type_t<BitT>>(rhs));
}

// Providing the bitwise xor operator for custom bitfield enums.
template <typename BitT> requires (isBitfield<BitT>::value)
constexpr Flags<BitT> operator ^(BitT lhs, BitT rhs) {
  return Flags<BitT>(static_cast<std::underlying_type_t<BitT>>(lhs) ^ static_cast<std::underlying_type_t<BitT>>(rhs));
}

// Providing the bitwise not operator for custom bitfield enums.
template <typename BitT> requires (isBitfield<BitT>::value)
constexpr Flags<BitT> operator ~(BitT lhs) {
  return Flags<BitT>(~static_cast<std::underlying_type_t<BitT>>(lhs));
}


/// Required is designed to be used in designated initializer structs to indicate that a field is required.
/// The compiler will issue an error if no value is provided for a Required field.
template <typename T>
class Required {
public:
  Required() { static_assert(!"A required field has not been given a value."); }
  constexpr Required(const Required<T>& other): value_(other.value_) {}
  constexpr Required(Required<T>&& other): value_(other.value_) {}
  template <typename U>
  Required(const Required<U>& other): value_(other.value_) {}
  template <typename U>
  Required(Required<U>&& other): value_(other.value_) {}
  template <typename... Args>
  Required(Args&&... args): value_(args...) {}

        T* operator ->()       { return &value_; }
  const T* operator ->() const { return &value_; }
        T& operator *()        { return value_;  }
  const T& operator *()  const { return value_;  }
        T& value()             { return value_;  }
  const T& value()       const { return value_;  }

private:
  T value_;
};

} // namespace hlgl::util