module;

#include <shitnet/macros.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <expected>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

export module cli;

export import diagnostic;

export namespace cli {

// comptime strs

template <std::size_t N> struct fixed_string {
    char data[N];

    constexpr fixed_string(const char (&s)[N]) { std::copy_n(s, N, data); }

    constexpr operator std::string_view() const { return {data, N - 1}; }
};

template <std::size_t N> fixed_string(const char (&)[N]) -> fixed_string<N>;

// field metadata

template <fixed_string S> struct short_ {
    static constexpr std::string_view value = S;
};

template <fixed_string S> struct long_ {
    static constexpr std::string_view value = S;
};

template <fixed_string S> struct help {
    static constexpr std::string_view value = S;
};

struct required {};

template <let V> struct default_ {
    static constexpr let value = V;
};

template <fixed_string V> struct default_string {
    static constexpr std::string_view value = V;
};

template <fixed_string... Values> struct choices {};

enum class help_menu_style { fancy, vanilla };

// policies

namespace detail {

template <typename Wanted, typename... Policies>
inline constexpr bool has_policy = (std::same_as<Wanted, Policies> || ...);

template <typename... Policies> struct default_policy {
    static constexpr bool exists = false;
};

template <let V, typename... Rest> struct default_policy<default_<V>, Rest...> {
    static constexpr bool exists = true;
    static constexpr let value = V;
};

template <fixed_string V, typename... Rest>
struct default_policy<default_string<V>, Rest...> {
    static constexpr bool exists = true;
    static constexpr std::string_view value = V;
};

template <typename First, typename... Rest>
struct default_policy<First, Rest...> : default_policy<Rest...> {};

template <typename... Policies> struct choices_policy {
    static constexpr bool exists = false;
    static constexpr std::array<std::string_view, 0> values{};
};

template <fixed_string... Values, typename... Rest>
struct choices_policy<choices<Values...>, Rest...> {
    static constexpr bool exists = true;
    static constexpr std::array<std::string_view, sizeof...(Values)> values{
        std::string_view{Values}...};
};

template <typename First, typename... Rest>
struct choices_policy<First, Rest...> : choices_policy<Rest...> {};

} // namespace detail

// registered fields

template <let... Members> struct fields {};

template <fixed_string Name, fixed_string Description, typename Options>
struct command {
    static constexpr std::string_view name = Name;
    static constexpr std::string_view description = Description;
    using options_type = Options;
};

template <typename... Commands> struct commands {};

template <typename Command> struct parsed_command {
    using command_type = Command;
    typename Command::options_type options;
};

template <typename> struct command_result;

template <typename... Commands> struct command_result<commands<Commands...>> {
    using type = std::variant<parsed_command<Commands>...>;
};

template <typename Commands>
using command_result_t = typename command_result<Commands>::type;

// options

template <typename T, typename Short, typename Long, typename Help,
          typename... Policies>
struct option {
    using value_type = T;

    static constexpr let short_name = Short::value;
    static constexpr let long_name = Long::value;
    static constexpr let help_text = Help::value;

    static constexpr bool takes_value = true;

    static constexpr bool is_required =
        detail::has_policy<required, Policies...>;

    using default_info = detail::default_policy<Policies...>;
    using choice_info = detail::choices_policy<Policies...>;

    static constexpr bool has_default = default_info::exists;
    static constexpr bool has_choices = choice_info::exists;

    static_assert(!(is_required && has_default),
                  "cli::option cannot be both required and defaulted");

    T value{};
    bool supplied = false;

    constexpr option() {
        if constexpr (has_default) {
            value = default_info::value;
        }
    }

    constexpr operator T &() { return value; }

    constexpr operator const T &() const { return value; }
};

// flags

template <typename Short, typename Long, typename Help> struct flag {
    using value_type = bool;

    bool value = false;

    static constexpr let short_name = Short::value;
    static constexpr let long_name = Long::value;
    static constexpr let help_text = Help::value;

    static constexpr bool takes_value = false;
    static constexpr bool is_required = false;
    static constexpr bool has_default = true;

    constexpr operator bool() const { return value; }
};

// errors / parser control

struct unknown_option {
    std::string argument;
    std::string suggestion;
};

struct missing_value {
    std::string argument;
};

struct invalid_value {
    std::string argument;
    std::string value;
    std::string expected;
};

struct invalid_choice {
    std::string argument;
    std::string value;
    std::string suggestion;
};

struct missing_required {
    std::string argument;
};

struct unknown_command {
    std::string argument;
    std::string suggestion;
};

struct missing_subcommand {
    std::string command;
};

struct help_requested {
    std::string command;
};

using parse_error =
    std::variant<unknown_option, missing_value, invalid_value, invalid_choice,
                 missing_required, unknown_command, missing_subcommand,
                 help_requested>;

template <typename T> using result = std::expected<T, parse_error>;

// implementation

namespace detail {

template <typename> inline constexpr bool always_false = false;

inline fn edit_distance(std::string_view left, std::string_view right)
    -> std::size_t {
    std::vector<std::size_t> previous(right.size() + 1);
    std::vector<std::size_t> current(right.size() + 1);

    for (std::size_t column = 0; column <= right.size(); ++column) {
        previous[column] = column;
    }

    for (std::size_t row = 1; row <= left.size(); ++row) {
        current[0] = row;

        for (std::size_t column = 1; column <= right.size(); ++column) {
            const let substitution =
                previous[column - 1] + (left[row - 1] != right[column - 1]);
            const let insertion = current[column - 1] + 1;
            const let deletion = previous[column] + 1;

            current[column] = std::min({substitution, insertion, deletion});
        }

        std::swap(previous, current);
    }

    return previous[right.size()];
}

inline fn consider_suggestion(std::string_view argument,
                              std::string_view candidate,
                              std::string &suggestion,
                              std::size_t &best_distance) -> void {
    const let distance = edit_distance(argument, candidate);

    if (distance < best_distance) {
        best_distance = distance;
        suggestion = candidate;
    }
}

template <typename Command>
fn option_suggestion(std::string_view, fields<>) -> std::string {
    return {};
}

template <typename Command, let... Members>
fn option_suggestion(std::string_view argument, fields<Members...>)
    -> std::string {
    std::string suggestion;
    std::size_t best_distance = std::numeric_limits<std::size_t>::max();

    (consider_suggestion(argument,
                         std::remove_cvref_t<decltype(std::declval<Command>().*
                                                      Members)>::long_name,
                         suggestion, best_distance),
     ...);
    consider_suggestion(argument, "--help", suggestion, best_distance);

    if (best_distance > 2) {
        return {};
    }

    return suggestion;
}

// value parsing

template <typename T>
fn parse_value(std::string_view, std::string_view) -> result<T> {
    static_assert(always_false<T>,
                  "cli::parse_value does not support this type");
}

template <>
inline fn parse_value<int>(std::string_view argument, std::string_view text)
    -> result<int> {
    int value{};

    const let[ptr, ec] =
        std::from_chars(text.data(), text.data() + text.size(), value);

    if (ec != std::errc{} || ptr != text.data() + text.size()) {
        return std::unexpected{parse_error{invalid_value{
            .argument = std::string{argument},
            .value = std::string{text},
            .expected = "integer",
        }}};
    }

    return value;
}

template <>
inline fn parse_value<std::string>(std::string_view, std::string_view text)
    -> result<std::string> {
    return std::string{text};
}

// argv parsing

template <typename Command, let Member>
fn try_field(Command &command, std::string_view argument, int argc, char **argv,
             int &index) -> result<bool> {
    let &field = command.*Member;

    using field_type = std::remove_cvref_t<decltype(field)>;

    if (argument != field_type::short_name &&
        argument != field_type::long_name) {
        return false;
    }

    if constexpr (field_type::takes_value) {
        if (index + 1 >= argc) {
            return std::unexpected{parse_error{missing_value{
                .argument = std::string{argument},
            }}};
        }

        const std::string_view text = argv[++index];

        if constexpr (field_type::has_choices) {
            const let &choices = field_type::choice_info::values;
            const bool valid =
                std::ranges::find(choices, text) != choices.end();

            if (!valid) {
                std::string suggestion;
                std::size_t best_distance =
                    std::numeric_limits<std::size_t>::max();

                for (const let choice : choices) {
                    consider_suggestion(text, choice, suggestion,
                                        best_distance);
                }

                if (best_distance > 2) {
                    suggestion.clear();
                }

                return std::unexpected{parse_error{invalid_choice{
                    .argument = std::string{argument},
                    .value = std::string{text},
                    .suggestion = std::move(suggestion),
                }}};
            }
        }

        let parsed =
            parse_value<typename field_type::value_type>(argument, text);

        if (!parsed) {
            return std::unexpected{std::move(parsed.error())};
        }

        field.value = std::move(*parsed);
        field.supplied = true;
    } else {
        field.value = true;
    }

    return true;
}

template <typename Command>
fn try_fields(Command &, fields<>, std::string_view, int, char **, int &)
    -> result<bool> {
    return false;
}

template <typename Command, let Member, let... Rest>
fn try_fields(Command &command, fields<Member, Rest...>,
              std::string_view argument, int argc, char **argv, int &index)
    -> result<bool> {
    let matched =
        try_field<Command, Member>(command, argument, argc, argv, index);

    if (!matched) {
        return std::unexpected{std::move(matched.error())};
    }

    if (*matched) {
        return true;
    }

    return try_fields(command, fields<Rest...>{}, argument, argc, argv, index);
}

// required validation

template <typename Command, let Member>
fn validate_field(Command &command) -> result<bool> {
    let &field = command.*Member;

    using field_type = std::remove_cvref_t<decltype(field)>;

    if constexpr (field_type::is_required) {
        if (!field.supplied) {
            return std::unexpected{parse_error{missing_required{
                .argument = std::string{field_type::long_name},
            }}};
        }
    }

    return true;
}

template <typename Command>
fn validate_fields(Command &, fields<>) -> result<bool> {
    return true;
}

template <typename Command, let Member, let... Rest>
fn validate_fields(Command &command, fields<Member, Rest...>) -> result<bool> {
    let validated = validate_field<Command, Member>(command);

    if (!validated) {
        return std::unexpected{std::move(validated.error())};
    }

    return validate_fields(command, fields<Rest...>{});
}

// help generation

template <typename T> constexpr fn value_name() -> std::string_view {
    if constexpr (std::same_as<T, int>) {
        return "INT";
    } else if constexpr (std::same_as<T, std::string>) {
        return "TEXT";
    } else {
        return "VALUE";
    }
}

inline fn append_padding(std::string &output, std::size_t current,
                         std::size_t target) -> void {
    if (current >= target) {
        output += ' ';
        return;
    }

    output.append(target - current, ' ');
}

template <typename Command, let Member>
fn append_help_field(std::string &output) -> void {
    using field_type =
        std::remove_cvref_t<decltype(std::declval<Command>().*Member)>;

    std::string names = "  ";

    names += field_type::short_name;
    names += ", ";
    names += field_type::long_name;

    if constexpr (field_type::takes_value) {
        names += " <";
        names += value_name<typename field_type::value_type>();
        names += ">";
    }

    output += names;

    append_padding(output, names.size(), 30);

    output += field_type::help_text;

    if constexpr (field_type::is_required) {
        output += " [required]";
    }

    if constexpr (field_type::takes_value && field_type::has_default) {
        output += " [default: ";

        if constexpr (std::same_as<typename field_type::value_type, int>) {
            output += std::to_string(field_type::default_info::value);
        } else if constexpr (std::same_as<typename field_type::value_type,
                                          std::string>) {
            output += field_type::default_info::value;
        }

        output += "]";
    }

    output += '\n';
}

template <typename Command, let Member>
fn append_fancy_help_field(std::string &output, bool required) -> void {
    using field_type =
        std::remove_cvref_t<decltype(std::declval<Command>().*Member)>;

    if (field_type::is_required != required) {
        return;
    }

    constexpr std::string_view blue = "\033[1;34m";
    constexpr std::string_view cyan = "\033[1;36m";
    constexpr std::string_view reset = "\033[0m";

    output += " ";
    output += blue;
    output += "│";
    output += reset;
    output += "  ";
    output += cyan;
    output += field_type::short_name;
    output += ", ";
    output += field_type::long_name;

    if constexpr (field_type::takes_value) {
        output += " <";
        output += value_name<typename field_type::value_type>();
        output += ">";
    }

    output += reset;
    output += '\n';

    if constexpr (!field_type::help_text.empty()) {
        output += " ";
        output += blue;
        output += "│";
        output += reset;
        output += "      ";
        output += field_type::help_text;
        output += '\n';
    }

    if constexpr (field_type::takes_value && field_type::has_default) {
        output += " ";
        output += blue;
        output += "│";
        output += reset;
        output += "      default: ";

        if constexpr (std::same_as<typename field_type::value_type, int>) {
            output += std::to_string(field_type::default_info::value);
        } else if constexpr (std::same_as<typename field_type::value_type,
                                          std::string>) {
            output += field_type::default_info::value;
        }

        output += '\n';
    }

    output += " ";
    output += blue;
    output += "│";
    output += reset;
    output += '\n';
}

template <typename Command>
fn append_help_fields(std::string &, fields<>) -> void {}

template <typename Command, let Member, let... Rest>
fn append_help_fields(std::string &output, fields<Member, Rest...>) -> void {
    append_help_field<Command, Member>(output);

    append_help_fields<Command>(output, fields<Rest...>{});
}

template <typename Command>
fn append_fancy_help_fields(std::string &, fields<>, bool) -> void {}

template <typename Command, let Member, let... Rest>
fn append_fancy_help_fields(std::string &output, fields<Member, Rest...>,
                            bool required) -> void {
    append_fancy_help_field<Command, Member>(output, required);

    append_fancy_help_fields<Command>(output, fields<Rest...>{}, required);
}

template <typename Command> fn command_name() -> std::string_view {
    if constexpr (requires { Command::command_name; }) {
        return Command::command_name;
    } else {
        return "program";
    }
}

template <typename Command> fn vanilla_help_text() -> std::string {
    std::string output = "Usage: ";

    output += command_name<Command>();
    output += " [options]\n";

    if constexpr (requires { Command::command_description; }) {
        output += '\n';
        output += Command::command_description;
        output += '\n';
    }

    output += "\nOptions:\n";

    append_help_fields<Command>(output, typename Command::fields{});

    output += "  -h, --help";

    append_padding(output, 12, 30);

    output += "Show this help message\n";

    return output;
}

template <typename Command> fn fancy_help_text() -> std::string {
    constexpr std::string_view bold = "\033[1m";
    constexpr std::string_view blue = "\033[1;34m";
    constexpr std::string_view cyan = "\033[1;36m";
    constexpr std::string_view reset = "\033[0m";

    std::string output;

    output += bold;
    output += command_name<Command>();
    output += "::help";
    output += reset;
    output += "\n\n";

    if constexpr (requires { Command::command_description; }) {
        output += Command::command_description;
        output += "\n\n";
    }

    output += " ";
    output += blue;
    output += "╭─";
    output += reset;
    output += " Usage\n ";
    output += blue;
    output += "│";
    output += reset;
    output += "\n ";
    output += blue;
    output += "│";
    output += reset;
    output += "  ";
    output += command_name<Command>();
    output += " [options]\n ";
    output += blue;
    output += "│";
    output += reset;
    output += "\n ";
    output += blue;
    output += "╰─▶";
    output += reset;
    output += " Required arguments\n\n";

    append_fancy_help_fields<Command>(output, typename Command::fields{}, true);

    output += "\n ";
    output += blue;
    output += "╭─";
    output += reset;
    output += " Optional arguments\n ";
    output += blue;
    output += "│";
    output += reset;
    output += "\n";

    append_fancy_help_fields<Command>(output, typename Command::fields{},
                                      false);

    output += " ";
    output += blue;
    output += "╰─▶";
    output += reset;
    output += " ";
    output += cyan;
    output += "-h, --help";
    output += reset;
    output += "\n     Show this help message\n";

    return output;
}

template <typename... Commands>
fn append_commands(std::string &output, commands<Commands...>) -> void {
    constexpr std::string_view cyan = "\033[1;36m";
    constexpr std::string_view reset = "\033[0m";
    std::size_t width = 0;
    ((width = std::max(width, Commands::name.size())), ...);
    width += 3;

    ((output += "     ", output += cyan, output += Commands::name,
      append_padding(output, Commands::name.size(), width), output += reset,
      output += Commands::description, output += '\n'),
     ...);
}

template <typename... Commands>
fn append_vanilla_commands(std::string &output, commands<Commands...>) -> void {
    std::size_t width = 0;
    ((width = std::max(width, Commands::name.size())), ...);
    width += 3;

    ((output += "  ", output += Commands::name,
      append_padding(output, Commands::name.size(), width),
      output += Commands::description, output += '\n'),
     ...);
}

template <typename... Commands>
fn command_column_width(commands<Commands...>) -> std::size_t {
    std::size_t width = 0;
    ((width = std::max(width, Commands::name.size())), ...);
    return width + 3;
}

template <typename... Commands>
fn command_suggestion(std::string_view argument, commands<Commands...>)
    -> std::string {
    std::string suggestion;
    std::size_t best_distance = std::numeric_limits<std::size_t>::max();

    (consider_suggestion(argument, Commands::name, suggestion, best_distance),
     ...);

    if (best_distance > 2) {
        return {};
    }

    return suggestion;
}

inline fn command_line(int argc, char **argv) -> std::string {
    std::string output;

    for (int index = 1; index < argc; ++index) {
        if (!output.empty()) {
            output += ' ';
        }

        output += argv[index];
    }

    return output;
}

} // namespace detail

// public api

template <typename Command> fn help_text() -> std::string {
    if constexpr (requires { Command::help_style; }) {
        if constexpr (Command::help_style == help_menu_style::vanilla) {
            return detail::vanilla_help_text<Command>();
        }
    }

    return detail::fancy_help_text<Command>();
}

template <typename Command> fn parse(int argc, char **argv) -> result<Command> {
    Command output{};

    for (int i = 1; i < argc; ++i) {
        const std::string_view argument = argv[i];

        if (argument == "-h" || argument == "--help") {
            return std::unexpected{parse_error{help_requested{}}};
        }

        let matched = detail::try_fields(output, typename Command::fields{},
                                         argument, argc, argv, i);

        if (!matched) {
            return std::unexpected{std::move(matched.error())};
        }

        if (!*matched) {
            return std::unexpected{parse_error{unknown_option{
                .argument = std::string{argument},
                .suggestion = detail::option_suggestion<Command>(
                    argument, typename Command::fields{}),
            }}};
        }
    }

    let validated = detail::validate_fields(output, typename Command::fields{});

    if (!validated) {
        return std::unexpected{std::move(validated.error())};
    }

    return output;
}

template <typename Root> fn command_help_text() -> std::string {
    constexpr std::string_view bold = "\033[1m";
    constexpr std::string_view blue = "\033[1;34m";
    constexpr std::string_view cyan = "\033[1;36m";
    constexpr std::string_view reset = "\033[0m";

    if constexpr (requires { Root::help_style; }) {
        if constexpr (Root::help_style == help_menu_style::vanilla) {
            std::string output = "Usage: ";
            output += detail::command_name<Root>();
            output += " <command>\n";

            if constexpr (requires { Root::command_description; }) {
                output += '\n';
                output += Root::command_description;
                output += "\n";
            }

            output += "\nCommands:\n";
            detail::append_vanilla_commands(output,
                                            typename Root::subcommands{});
            output += "\n  --help";
            detail::append_padding(
                output, std::string_view{"--help"}.size(),
                detail::command_column_width(typename Root::subcommands{}));
            output += "Show this help message\n";
            return output;
        }
    }

    std::string output;

    output += bold;
    output += detail::command_name<Root>();
    output += "::help";
    output += reset;
    output += "\n\n";

    if constexpr (requires { Root::command_description; }) {
        output += " ";
        output += Root::command_description;
        output += "\n\n";
    }

    output += " ";
    output += blue;
    output += "╭─";
    output += reset;
    output += " Usage\n ";
    output += blue;
    output += "│\n │";
    output += reset;
    output += "  ";
    output += detail::command_name<Root>();
    output += " [command]\n ";
    output += blue;
    output += "│\n ╰─▶";
    output += reset;
    output += " Available Commands\n\n";

    detail::append_commands(output, typename Root::subcommands{});

    output += "\n     ";
    output += cyan;
    output += "--help";
    detail::append_padding(
        output, std::string_view{"--help"}.size(),
        detail::command_column_width(typename Root::subcommands{}));
    output += reset;
    output += "Show this help message\n";

    return output;
}

namespace detail {

template <typename Root>
fn selected_command_help_text(std::string_view, commands<>) -> std::string {
    return command_help_text<Root>();
}

template <typename Root, typename Command, typename... Rest>
fn selected_command_help_text(std::string_view selected,
                              commands<Command, Rest...>) -> std::string {
    if (selected == Command::name) {
        return help_text<typename Command::options_type>();
    }

    return selected_command_help_text<Root>(selected, commands<Rest...>{});
}

} // namespace detail

template <typename Root>
fn command_help_text(std::string_view selected) -> std::string {
    return detail::selected_command_help_text<Root>(
        selected, typename Root::subcommands{});
}

namespace detail {

template <typename Root, typename Variant>
fn parse_selected_command(std::string_view argument, int, char **, commands<>)
    -> result<Variant> {
    return std::unexpected{parse_error{unknown_command{
        .argument = std::string{argument},
        .suggestion =
            command_suggestion(argument, typename Root::subcommands{}),
    }}};
}

template <typename Root, typename Variant, typename Command, typename... Rest>
fn parse_selected_command(std::string_view argument, int argc, char **argv,
                          commands<Command, Rest...>) -> result<Variant> {
    if (argument == Command::name) {
        let parsed = parse<typename Command::options_type>(argc - 1, argv + 1);

        if (!parsed) {
            if (let *help = std::get_if<help_requested>(&parsed.error())) {
                help->command = Command::name;
            }

            return std::unexpected{std::move(parsed.error())};
        }

        return Variant{std::in_place_type<parsed_command<Command>>,
                       std::move(*parsed)};
    }

    return parse_selected_command<Root, Variant>(argument, argc, argv,
                                                 commands<Rest...>{});
}

} // namespace detail

template <typename Root>
fn parse_command(int argc, char **argv)
    -> result<command_result_t<typename Root::subcommands>> {
    using variant_type = command_result_t<typename Root::subcommands>;

    if (argc < 2) {
        return std::unexpected{parse_error{help_requested{}}};
    }

    const std::string_view argument = argv[1];

    if (argument == "-h" || argument == "--help") {
        return std::unexpected{parse_error{help_requested{}}};
    }

    return detail::parse_selected_command<Root, variant_type>(
        argument, argc, argv, typename Root::subcommands{});
}

template <typename Command>
fn error_text(const parse_error &error, int argc, char **argv) -> std::string {
    const let line = detail::command_line(argc, argv);
    std::string token;
    std::string message;

    std::visit(
        [&](const let &value) {
            using error_type = std::remove_cvref_t<decltype(value)>;

            if constexpr (std::same_as<error_type, unknown_option>) {
                token = value.argument;
                message = "Unknown argument: " + value.argument;
                if (!value.suggestion.empty()) {
                    message += ". Did you mean '" + value.suggestion + "'?";
                }
            } else if constexpr (std::same_as<error_type, missing_value>) {
                token = value.argument;
                message = "Option " + value.argument + " requires an argument";
            } else if constexpr (std::same_as<error_type, invalid_value>) {
                token = value.value;
                message = "Invalid value '" + value.value + "' for " +
                          value.argument + "; expected " + value.expected;
            } else if constexpr (std::same_as<error_type, invalid_choice>) {
                token = value.value;
                message = "Invalid choice '" + value.value + "' for " +
                          value.argument;
                if (!value.suggestion.empty()) {
                    message += ". Did you mean '" + value.suggestion + "'?";
                }
            } else if constexpr (std::same_as<error_type, missing_required>) {
                message = "Missing required argument: " + value.argument;
            } else if constexpr (std::same_as<error_type, unknown_command>) {
                token = value.argument;
                message = "Unknown command: " + value.argument;
                if (!value.suggestion.empty()) {
                    message += ". Did you mean '" + value.suggestion + "'?";
                }
            } else if constexpr (std::same_as<error_type, missing_subcommand>) {
                message =
                    "Command '" + value.command + "' requires a subcommand.";
            }
        },
        error);

    const let column = token.empty() ? std::size_t{0} : line.find(token);
    const let start = column == std::string::npos ? std::size_t{0} : column;
    const let width =
        token.empty() ? std::max(line.size(), std::size_t{1}) : token.size();

    constexpr let selected_style = [] {
        if constexpr (requires { Command::help_style; }) {
            if constexpr (Command::help_style == help_menu_style::vanilla) {
                return diagnostic::style::vanilla;
            }
        }

        return diagnostic::style::fancy;
    }();

    return diagnostic::render(
        diagnostic::message{
            .namespace_name = "cli::parser",
            .summary = "Command line syntax analysis failed",
            .filename = argc > 0 ? std::string_view{argv[0]}
                                 : detail::command_name<Command>(),
            .line_number = 1,
            .line_text = line,
            .column_start = start,
            .column_length = width,
            .help = message,
        },
        selected_style);
}

namespace detail {

template <typename Root>
fn selected_command_error_text(std::string_view, const parse_error &error,
                               int argc, char **argv, commands<>)
    -> std::string {
    return error_text<Root>(error, argc, argv);
}

template <typename Root, typename Command, typename... Rest>
fn selected_command_error_text(std::string_view selected,
                               const parse_error &error, int argc, char **argv,
                               commands<Command, Rest...>) -> std::string {
    if (selected == Command::name) {
        return error_text<typename Command::options_type>(error, argc, argv);
    }

    return selected_command_error_text<Root>(selected, error, argc, argv,
                                             commands<Rest...>{});
}

} // namespace detail

template <typename Root>
fn command_error_text(const parse_error &error, int argc, char **argv)
    -> std::string {
    const std::string_view selected = argc > 1 ? argv[1] : "";

    return detail::selected_command_error_text<Root>(
        selected, error, argc, argv, typename Root::subcommands{});
}

} // namespace cli
