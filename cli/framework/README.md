# CLI framework

`cli` is a small declarative C++23 parser with typed options, multicall
commands, generated help, reusable diagnostics, and typo suggestions.

I built this framework to learn how typed command parsing could work in C++ and
to test shitnet before the Go bindings existed. It remains as the native test
CLI, but it is mostly irrelevant to the current Go API and REPL.

```cpp
import cli;
#include <shitnet/macros.h>

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

let parsed = cli::parse_command<Root>(argc, argv);

return match(*parsed)(
    case_(cli::parsed_command<GreetCommand>, command) {
        std::println("hello {}", command.options.name.value);
        return 0;
    });
```

Use `cli::help_menu_style::vanilla` when ANSI output is unwanted. A selected
command's style controls its help and diagnostics. An empty invocation displays
root help.

The project macro header provides `fn`, `let`, and `case_`; the
`shitnet.match` module provides `match`. See `../main.cpp` for the real
multicall.
