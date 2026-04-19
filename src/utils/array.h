#ifndef HLGL_UTILS_ARRAY_H
#define HLGL_UTILS_ARRAY_H

#include <algorithm>
#include <array>
#include <string_view>

namespace hlgl {

template <typename T, size_t N>
class Array : public std::array<T,N> {
public:

  auto back() { return std::array<T,N>::at(size_-1); }

  auto begin() { return std::array<T,N>::begin(); }
  auto begin() const { return std::array<T,N>::begin(); }
  auto cbegin() const { return std::array<T,N>::cbegin(); }
  auto end() { return std::array<T,N>::begin() + size_; }
  auto end() const { return std::array<T,N>::begin() + size_; }
  auto cend() const { return std::array<T,N>::cbegin() + size_; }

  constexpr bool empty() const noexcept { return (size_ == 0); }
  constexpr size_t size() const noexcept { return size_; }
  constexpr size_t max_size() const noexcept { return N; }
  constexpr size_t capacity() const noexcept { return N; }

  constexpr void clear() noexcept { size_ = 0; }
  constexpr bool insert(size_t pos, const T& value) noexcept {
    if (size_ == N || pos > size_)
      return false;

    for (size_t i {N-1}; i >= (pos+1); --i) {
      at(i) = at(i-1);
    }
    at(pos) = value;
    ++size_;
    return true;
  }

  constexpr bool erase(size_t pos) noexcept {
    if (pos >= size_)
      return false;
    
    for (size_t i {pos}; i < size_; ++i) {
      this->at(i) = this->at(i+1);
    }
    --size_;
    return true;
  }

  constexpr bool push_back(const T& value) noexcept {
    if (size_ == N)
      return false;
    this->at(size_++) = value;
    return true;
  }

  constexpr bool pop_back() noexcept {
    if (size_ == 0)
      return false;
    --size_;
    return true;
  }

  constexpr bool merge(const hlgl::Array<T,N>& other) noexcept {
    if (size_ + other.size_ > N)
      return false;
    for (auto& entry : other) {
      push_back(entry);
    }
    return true;
  }

  constexpr size_t findStr(const char* str) const noexcept {
    for (size_t i {0}; i < size_; ++i) {
      if (std::string_view(this->at(i)) == std::string_view(str))
        return i;
    }
    return SIZE_MAX;
  }

  constexpr size_t findItem(const T& item) const noexcept {
    for (size_t i {0}; i < size_; ++i) {
      if (this->at(i) == item)
        return i;
    }
    return SIZE_MAX;
  }

private:
  size_t size_ {0};
};

} // namespace hlgl

#endif // HLGL_UTILS_ARRAY_H