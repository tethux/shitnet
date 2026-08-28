# CLI framework

`cli` is a small declarative C++23 parser with typed options, multicall
commands, generated help, reusable diagnostics, and typo suggestions.

```cpp
import cli;
import shitnet.match;

struct GreetOptions {
    static constexpr std::string_view command_name = "tool greet";
    static constexpr cli::help_menu_style help_style =
        cli::help_menu_style::fancy;

    cli::option<std::string, cli::short_<"-n">, cli::long_<"--name">,
                cli::help<"Person to greet">, cli::required>
        name;

    using fields = cli::fields<&GreetOptions::name>;
};

using GreetCommand =
    cli::command<"greet", "Greet someone", GreetOptions>;

struct Root {
    static constexpr std::string_view command_name = "tool";
    static constexpr cli::help_menu_style help_style =
        cli::help_menu_style::fancy;
    using subcommands = cli::commands<GreetCommand>;
};

auto parsed = cli::parse_command<Root>(argc, argv);

return match(
    *parsed,
    case_of<cli::parsed_command<GreetCommand>>([](const auto &command) {
        std::println("hello {}", command.options.name.value);
        return 0;
    }));
```

Use `cli::help_menu_style::vanilla` when ANSI output is unwanted. A selected
command's style controls its help and diagnostics. An empty invocation displays
root help.

`shitnet.match` replaces the old macro header. C++ modules cannot export
preprocessor macros, so matching is provided through typed `match` and
`case_of` functions and normal C++ uses `auto` directly. See `../main.cpp` for
the real multicall.
