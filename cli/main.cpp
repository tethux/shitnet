#include <shitnet/shitnet.h>

#include <cstdio>
#include <variant>

import cli;
import shitnet.cli.arp;
import shitnet.cli.commands;
import shitnet.match;

auto main(int argc, char **argv) -> int {
    auto parsed = cli::parse_command<shitnet_cli::Root>(argc, argv);

    if (!parsed) {
        if (const auto *help =
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

    shitnet *net = shitnet_create();
    if (net == nullptr) {
        std::fprintf(stderr, "failed to create shitnet\n");
        return 1;
    }

    using namespace shitnet_cli;

    const auto result =
        match(*parsed,
              case_of<cli::parsed_command<ArpRequestCommand>>(
                  [&](const auto &command) {
                      return arp::request(net, command.options.target.value);
                  }),
              case_of<cli::parsed_command<ArpReplyCommand>>(
                  [&](const auto &) { return arp::reply(net); }),
              case_of<cli::parsed_command<ArpLearnCommand>>(
                  [&](const auto &) { return arp::learn(net); }),
              case_of<cli::parsed_command<BadHardwareCommand>>(
                  [&](const auto &) { return arp::bad_hardware(net); }),
              case_of<cli::parsed_command<BadProtocolCommand>>(
                  [&](const auto &) { return arp::bad_protocol(net); }),
              case_of<cli::parsed_command<BadLengthCommand>>(
                  [&](const auto &) { return arp::bad_length(net); }),
              case_of<cli::parsed_command<BadOperationCommand>>(
                  [&](const auto &) { return arp::bad_operation(net); }));

    shitnet_destroy(net);
    return result;
}
