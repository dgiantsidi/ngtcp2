
#pragma once
#if CXX20
#include <span>
#else
#include <cstddef> // for std::size_t
#include <iterator> // for std::begin, std::end
#include <array>

namespace detail
{
    template<class T, std::size_t N, std::size_t... I>
    constexpr std::array<std::remove_cv_t<T>, N>
        to_array_impl(T (&&a)[N], std::index_sequence<I...>)
    {
        return {{std::move(a[I])...}};
    }
}
 
template<class T, std::size_t N>
constexpr std::array<std::remove_cv_t<T>, N> to_array(T (&&a)[N])
{
    return detail::to_array_impl(std::move(a), std::make_index_sequence<N>{});
}

template <typename T>
class Span {
public:
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using iterator = T*;
    using const_iterator = const T*;

    Span() noexcept : data_(nullptr), size_(0) {}

    Span(T* ptr, size_type count) noexcept : data_(ptr), size_(count) {}

    Span(T* first, T* last) noexcept : data_(first), size_(last - first) {}

    template <std::size_t N>
    Span(T (&arr)[N]) noexcept : data_(arr), size_(N) {}

    template <typename Container>
    Span(Container& cont) noexcept : data_(custom_data(cont)), size_(custom_size(cont)) {}

    iterator begin() const noexcept { return data_; }
    iterator end() const noexcept { return data_ + size_; }
    const_iterator cbegin() const noexcept { return data_; }
    const_iterator cend() const noexcept { return data_ + size_; }

    reference operator[] (size_t idx) const { return data_[idx]; }
    reference front() const { return data_; }
    reference back() const { return data_[size_ - 1]; }
    pointer data() const noexcept { return data_; }
    size_type size() const noexcept { return size_; }

    Span<T> first(size_t N) const {
        return Span<T>(data_, N);
    }

    Span<T> last(size_t N) const {
        size_t offset = size_ - N;
        return Span<T>(data_+offset, N);
    }

    Span<T> subspan(size_t offset) const {
        return Span<T>(data_+offset, size_ - offset);
    }

    Span<T> subspan(size_t offset, size_t N) const {
        return Span<T>(data_+offset, N);
    }
    

    bool empty() const noexcept { return size_ == 0; }

    template <typename Container>
    static auto custom_data(Container& cont) -> decltype(cont.data()) {
        return cont.data();
    }

    template <typename U, std::size_t N>
    static U* custom_data(U (&arr)[N]) noexcept {
        return arr;
    }

private:
    T* data_;
    size_type size_;


    template <typename Container>
    static auto custom_size(const Container& cont) -> decltype(cont.size()) {
        return cont.size();
    }

    template <typename U, std::size_t N>
    static std::size_t custom_size(const U (&arr)[N]) noexcept {
        return N;
    }
};

#endif