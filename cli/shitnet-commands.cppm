module;

#include <string_view>

export module shitnet.cli.commands;

import cli;

export namespace shitnet_cli {

struct RequestOptions {
    static constexpr std::string_view command_name = "shitnet arp-request";
    static constexpr std::string_view command_description =
        "Send an ARP request.";
    static constexpr cli::help_menu_style help_style =
        cli::help_menu_style::fancy;

    cli::option<int, cli::short_<"-t">, cli::long_<"--target">,
                cli::help<"Last octet of the target IPv4 address">,
                cli::required>
        target;

    using fields = cli::fields<&RequestOptions::target>;
};

struct NoOptions {
    static constexpr std::string_view command_name = "shitnet";
    static constexpr cli::help_menu_style help_style =
        cli::help_menu_style::fancy;
    using fields = cli::fields<>;
};

using ArpRequestCommand =
    cli::command<"arp-request", "Send an ARP request", RequestOptions>;
using ArpReplyCommand =
    cli::command<"arp-reply", "Send an ARP reply", NoOptions>;
using ArpLearnCommand =
    cli::command<"arp-learn", "Learn an ARP entry", NoOptions>;
using BadHardwareCommand =
    cli::command<"arp-bad-hardware", "Send an unsupported hardware type",
                 NoOptions>;
using BadProtocolCommand =
    cli::command<"arp-bad-protocol", "Send an unsupported protocol type",
                 NoOptions>;
using BadLengthCommand =
    cli::command<"arp-bad-length", "Send invalid address lengths", NoOptions>;
using BadOperationCommand =
    cli::command<"arp-bad-operation", "Send an unknown ARP operation",
                 NoOptions>;

struct Root {
    static constexpr std::string_view command_name = "shitnet";
    static constexpr std::string_view command_description =
        "Exercise the shitnet packet stack.";
    static constexpr cli::help_menu_style help_style =
        cli::help_menu_style::fancy;
    using subcommands =
        cli::commands<ArpRequestCommand, ArpReplyCommand, ArpLearnCommand,
                      BadHardwareCommand, BadProtocolCommand, BadLengthCommand,
                      BadOperationCommand>;
};

} // namespace shitnet_cli
