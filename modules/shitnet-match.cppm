module;

#include <type_traits>
#include <utility>
#include <variant>

export module shitnet.match;

template <typename... Functions> struct overloaded : Functions... {
    using Functions::operator()...;
};

template <typename... Functions>
overloaded(Functions...) -> overloaded<Functions...>;

export template <typename Type, typename Function> struct match_case {
    Function function;

    auto operator()(const Type &value) const -> decltype(auto) {
        return function(value);
    }
};

export template <typename Type, typename Function>
auto case_of(Function &&function) -> match_case<Type, std::decay_t<Function>> {
    return {std::forward<Function>(function)};
}

export template <typename Variant, typename... Cases>
auto match(Variant &&value, Cases &&...cases) -> decltype(auto) {
    return std::visit(overloaded{std::forward<Cases>(cases)...},
                      std::forward<Variant>(value));
}
