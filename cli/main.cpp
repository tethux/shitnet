#include <cstdio>
#include <shitnet/macros.h>
#include <variant>

import cli;
import shitnet.cli.commands;
import shitnet.match;

fn main(int argc, char **argv) -> int {
    let parsed = cli::parse_command<shitnet_cli::Root>(argc, argv);

    if (!parsed) {
        if (const let *help =
                std::get_if<cli::help_requested>(&parsed.error())) {
            std::printf("%s",
                        cli::command_help_text<shitnet_cli::Root>(help->command)
                            .c_str());
            return 0;
        }

        std::fprintf(stderr, "%s",
                     cli::command_error_text<shitnet_cli::Root>(parsed.error(),
                                                                argc, argv)
                         .c_str());
        return 1;
    }

    using namespace shitnet_cli;

    const let result = match(*parsed)(
        case_(cli::parsed_command<RunCommand>) { return run_shitnet(); },
        case_(cli::parsed_command<ArpRequestCommand>, command) {
            return run_arp_request(command.options.target.value);
        });

    return result;
}
