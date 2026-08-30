#include <shitnet/macros.h>

#include <cassert>
#include <string>
#include <string_view>
#include <variant>

import cli;
import shitnet.match;

struct RunOptions {
    static constexpr std::string_view command_name = "test run";
    static constexpr std::string_view command_description = "Run a test.";
    static constexpr cli::help_menu_style help_style =
        cli::help_menu_style::fancy;

    cli::option<std::string, cli::short_<"-n">, cli::long_<"--name">,
                cli::help<"Test name">, cli::required>
        name;
    cli::option<int, cli::short_<"-c">, cli::long_<"--count">,
                cli::help<"Run count">, cli::default_<2>>
        count;
    cli::option<std::string, cli::short_<"-f">, cli::long_<"--format">,
                cli::help<"Output format">, cli::default_string<"text">,
                cli::choices<"text", "json", "xml">>
        format;
    cli::flag<cli::short_<"-v">, cli::long_<"--verbose">,
              cli::help<"Verbose output">>
        verbose;

    using fields = cli::fields<&RunOptions::name, &RunOptions::count,
                               &RunOptions::format, &RunOptions::verbose>;
};

struct PlainOptions {
    static constexpr std::string_view command_name = "test plain";
    static constexpr cli::help_menu_style help_style =
        cli::help_menu_style::vanilla;

    cli::option<std::string, cli::short_<"-v">, cli::long_<"--value">,
                cli::help<"Plain value">, cli::required>
        value;

    using fields = cli::fields<&PlainOptions::value>;
};

using RunCommand = cli::command<"run", "Run a test", RunOptions>;
using PlainCommand = cli::command<"plain", "Use plain output", PlainOptions>;

struct TestRoot {
    static constexpr std::string_view command_name = "test";
    static constexpr std::string_view command_description =
        "Parser test multicall";
    static constexpr cli::help_menu_style help_style =
        cli::help_menu_style::fancy;
    using subcommands = cli::commands<RunCommand, PlainCommand>;
};

fn test_option_success() -> void {
    char executable[] = "test";
    char name_flag[] = "--name";
    char name[] = "alpha";
    char count_flag[] = "-c";
    char count[] = "5";
    char format_flag[] = "--format";
    char format[] = "json";
    char verbose[] = "-v";
    char *argv[]{executable, name_flag,   name,   count_flag,
                 count,      format_flag, format, verbose};

    let parsed = cli::parse<RunOptions>(8, argv);
    assert(parsed);
    assert(parsed->name.value == "alpha");
    assert(parsed->count.value == 5);
    assert(parsed->format.value == "json");
    assert(parsed->verbose.value);
}

fn test_option_errors() -> void {
    {
        char executable[] = "test";
        char *argv[]{executable};
        let parsed = cli::parse<RunOptions>(1, argv);
        assert(!parsed);
        assert(std::holds_alternative<cli::missing_required>(parsed.error()));
    }

    {
        char executable[] = "test";
        char typo[] = "--nmae";
        char value[] = "alpha";
        char *argv[]{executable, typo, value};
        let parsed = cli::parse<RunOptions>(3, argv);
        const let *error = std::get_if<cli::unknown_option>(&parsed.error());
        assert(error != nullptr);
        assert(error->suggestion == "--name");
    }

    {
        char executable[] = "test";
        char name[] = "--name";
        char *argv[]{executable, name};
        let parsed = cli::parse<RunOptions>(2, argv);
        assert(std::holds_alternative<cli::missing_value>(parsed.error()));
    }

    {
        char executable[] = "test";
        char name_flag[] = "--name";
        char name[] = "alpha";
        char count_flag[] = "--count";
        char count[] = "many";
        char *argv[]{executable, name_flag, name, count_flag, count};
        let parsed = cli::parse<RunOptions>(5, argv);
        assert(std::holds_alternative<cli::invalid_value>(parsed.error()));
    }

    {
        char executable[] = "test";
        char name_flag[] = "--name";
        char name[] = "alpha";
        char format_flag[] = "--format";
        char format[] = "jso";
        char *argv[]{executable, name_flag, name, format_flag, format};
        let parsed = cli::parse<RunOptions>(5, argv);
        const let *error = std::get_if<cli::invalid_choice>(&parsed.error());
        assert(error != nullptr);
        assert(error->suggestion == "json");
    }
}

fn test_multicall() -> void {
    {
        char executable[] = "test";
        char *argv[]{executable};
        let parsed = cli::parse_command<TestRoot>(1, argv);
        assert(std::holds_alternative<cli::help_requested>(parsed.error()));
    }

    {
        char executable[] = "test";
        char typo[] = "rn";
        char *argv[]{executable, typo};
        let parsed = cli::parse_command<TestRoot>(2, argv);
        const let *error = std::get_if<cli::unknown_command>(&parsed.error());
        assert(error != nullptr);
        assert(error->suggestion == "run");
    }

    {
        char executable[] = "test";
        char run[] = "run";
        char name_flag[] = "--name";
        char name[] = "alpha";
        char *argv[]{executable, run, name_flag, name};
        let parsed = cli::parse_command<TestRoot>(4, argv);
        assert(parsed);

        const let result = match(*parsed)(
            case_(cli::parsed_command<RunCommand>, command) {
                return command.options.name.value == "alpha" ? 17 : 0;
            },
            case_(cli::parsed_command<PlainCommand>) { return 0; });

        assert(result == 17);
    }
}

fn test_help_and_diagnostics() -> void {
    const let root_help = cli::command_help_text<TestRoot>();
    const let run_help = cli::command_help_text<TestRoot>("run");
    const let plain_help = cli::command_help_text<TestRoot>("plain");

    assert(root_help.contains("Available Commands"));
    assert(root_help.contains("run"));
    assert(root_help.contains("run     "));
    assert(root_help.contains("plain   "));
    assert(run_help.contains("Required arguments"));
    assert(run_help.contains("default: text"));
    assert(!plain_help.contains("\033["));

    cli::parse_error error = cli::unknown_option{
        .argument = "--nmae",
        .suggestion = "--name",
    };
    char executable[] = "test";
    char plain[] = "plain";
    char typo[] = "--nmae";
    char *argv[]{executable, plain, typo};

    const let rendered = cli::command_error_text<TestRoot>(error, 3, argv);
    assert(!rendered.contains("\033["));
    assert(rendered.contains("Did you mean '--name'?"));
    assert(rendered.contains("^^^^^^"));
}

fn main() -> int {
    test_option_success();
    test_option_errors();
    test_multicall();
    test_help_and_diagnostics();
    return 0;
}
