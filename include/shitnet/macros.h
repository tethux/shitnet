#pragma once

#include <utility>
#include <variant>

#define fn auto
#define cfn extern "C" auto
#define let auto

template <class... Fs> struct overloaded : Fs... {
    using Fs::operator()...;
};

template <class... Fs> overloaded(Fs...) -> overloaded<Fs...>;

template <class V> struct matcher {
    V &&value;

    template <class... Cases> decltype(auto) operator()(Cases &&...cases) {
        return std::visit(overloaded{std::forward<Cases>(cases)...},
                          std::forward<V>(value));
    }
};

template <class V> let match(V &&value) {
    return matcher<V>{std::forward<V>(value)};
}

#define CPP_MATCH_CASE_1(T) [&](T const &)

#define CPP_MATCH_CASE_2(T, name) [&](T const &name)

#define CPP_MATCH_GET(_1, _2, NAME, ...) NAME

#define case_(...)                                                             \
    CPP_MATCH_GET(__VA_ARGS__, CPP_MATCH_CASE_2, CPP_MATCH_CASE_1)(__VA_ARGS__)
