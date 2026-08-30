module;

#include <shitnet/macros.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>

export module diagnostic;

export namespace diagnostic {

enum class style { fancy, vanilla };

struct message {
    std::string_view namespace_name;
    std::string_view summary;
    std::string_view filename;
    std::size_t line_number;
    std::string_view line_text;
    std::size_t column_start;
    std::size_t column_length;
    std::string_view help;
};

namespace detail {

fn render_vanilla(const message &value) -> std::string {
    std::string output = "Error: ";
    output += value.namespace_name;
    output += '\n';
    output += value.summary;
    output += '\n';
    output += value.filename;
    output += ':';
    output += std::to_string(value.line_number);
    output += ':';
    output += std::to_string(value.column_start + 1);
    output += '\n';
    output += value.line_text;
    output += '\n';
    output.append(value.column_start, ' ');
    output.append(std::max(value.column_length, std::size_t{1}), '^');

    if (!value.help.empty()) {
        output += "\nhelp: ";
        output += value.help;
    }

    output += '\n';
    return output;
}

fn render_fancy(const message &value) -> std::string {
    constexpr std::string_view bold = "\033[1m";
    constexpr std::string_view red = "\033[1;31m";
    constexpr std::string_view blue = "\033[1;34m";
    constexpr std::string_view cyan = "\033[1;36m";
    constexpr std::string_view magenta = "\033[1;35m";
    constexpr std::string_view reset = "\033[0m";

    std::string output;
    output += bold;
    output += "Error:";
    output += reset;
    output += " ";
    output += magenta;
    output += value.namespace_name;
    output += reset;
    output += "\n\n ";
    output += red;
    output += "×";
    output += reset;
    output += " ";
    output += bold;
    output += value.summary;
    output += reset;
    output += "\n   ";
    output += blue;
    output += "╭─[";
    output += reset;
    output += bold;
    output += value.filename;
    output += ':';
    output += std::to_string(value.line_number);
    output += ':';
    output += std::to_string(value.column_start + 1);
    output += reset;
    output += blue;
    output += "]";
    output += reset;
    output += "\n ";
    output += std::to_string(value.line_number);
    output += " ";
    output += blue;
    output += "│";
    output += reset;
    output += " ";
    output += value.line_text;
    output += "\n   ";
    output += blue;
    output += "·";
    output += reset;
    output.append(value.column_start + 1, ' ');
    output += red;
    output.append(std::max(value.column_length, std::size_t{1}), '^');
    output += reset;

    if (!value.help.empty()) {
        output += "\n\n   ";
        output += cyan;
        output += "help:";
        output += reset;
        output += " ";
        output += value.help;
    }

    output += '\n';
    return output;
}

} // namespace detail

fn render(const message &value, style selected_style = style::fancy)
    -> std::string {
    if (selected_style == style::vanilla) {
        return detail::render_vanilla(value);
    }

    return detail::render_fancy(value);
}

} // namespace diagnostic
