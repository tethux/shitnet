module;

#include <shitnet/macros.h>

#include <utility>
#include <variant>

export module shitnet.match;

template <class... Fs> struct overloaded : Fs... {
    using Fs::operator()...;
};

template <class... Fs> overloaded(Fs...) -> overloaded<Fs...>;

export template <class V> struct matcher {
    V &&value;

    template <class... Cases> decltype(auto) operator()(Cases &&...cases) {
        return std::visit(overloaded{std::forward<Cases>(cases)...},
                          std::forward<V>(value));
    }
};

export template <class V> fn match(V &&value) {
    return matcher<V>{std::forward<V>(value)};
}
