module;

#include <shitnet/macros.h>
#include <shitnet/shitnet.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string>
#include <string_view>
#include <utility>

export module shitnet.cli.commands;

import cli;
import shitnet.tap;

namespace {

fn frame_protocol(std::span<const std::byte> frame) -> std::string_view {
    if (frame.size() < 14)
        return "FRAME";

    const let ether_type = static_cast<std::uint16_t>(
                               std::to_integer<std::uint16_t>(frame[12]) << 8) |
                           std::to_integer<std::uint16_t>(frame[13]);

    if (ether_type == 0x0806)
        return "ARP";

    if (ether_type == 0x0800) {
        if (frame.size() >= 24 && std::to_integer<std::uint8_t>(frame[23]) == 1)
            return "ICMP";

        return "IPv4";
    }

    return "OTHER";
}

fn parse_ipv4(std::string_view text, std::array<std::uint8_t, 4> &address)
    -> bool {
    unsigned octets[4]{};
    char trailing = '\0';
    const let matched =
        std::sscanf(std::string{text}.c_str(), "%u.%u.%u.%u%c", &octets[0],
                    &octets[1], &octets[2], &octets[3], &trailing);
    if (matched != 4)
        return false;

    for (std::size_t i = 0; i < address.size(); ++i) {
        if (octets[i] > 255)
            return false;

        address[i] = static_cast<std::uint8_t>(octets[i]);
    }

    return true;
}

} // namespace

export namespace shitnet_cli {

inline constexpr shitnet_config default_config{
    .mac = {0x02, 0x00, 0x00, 0x00, 0x00, 0x02},
    .ip = {10, 0, 0, 2},
};

fn run_shitnet() -> int {
    let tap_result = Tap::open("shitnet0");
    if (!tap_result) {
        std::fprintf(stderr, "failed to open TAP: %s\n",
                     tap_result.error().message.c_str());
        return 1;
    }

    let tap = std::move(*tap_result);
    shitnet *net = shitnet_create(&default_config);
    if (net == nullptr) {
        std::fprintf(stderr, "failed to create shitnet\n");
        return 1;
    }

    constexpr std::string_view bold = "\033[1m";
    constexpr std::string_view blue = "\033[1;34m";
    constexpr std::string_view cyan = "\033[1;36m";
    constexpr std::string_view reset = "\033[0m";

    std::printf("\n%s shitnet is listening%s\n\n", bold.data(), reset.data());
    std::printf(" %s╭─%s Stack\n", blue.data(), reset.data());
    std::printf(" %s│%s  interface  %s%s%s\n", blue.data(), reset.data(),
                cyan.data(), tap.name().c_str(), reset.data());
    std::printf(" %s│%s  address    %u.%u.%u.%u\n", blue.data(), reset.data(),
                default_config.ip[0], default_config.ip[1],
                default_config.ip[2], default_config.ip[3]);
    std::printf(" %s│%s  mac        %02x:%02x:%02x:%02x:%02x:%02x\n",
                blue.data(), reset.data(), default_config.mac[0],
                default_config.mac[1], default_config.mac[2],
                default_config.mac[3], default_config.mac[4],
                default_config.mac[5]);
    std::printf(" %s│%s\n", blue.data(), reset.data());
    std::printf(" %s╰─▶%s In another terminal\n\n", blue.data(), reset.data());
    std::printf("     %spkexec ip addr replace 10.0.0.1/24 dev %s%s\n",
                cyan.data(), tap.name().c_str(), reset.data());
    std::printf("     %spkexec ip link set %s up%s\n", cyan.data(),
                tap.name().c_str(), reset.data());
    std::printf("     %sping 10.0.0.2%s\n\n", cyan.data(), reset.data());

    std::array<std::byte, 65536> rx{};
    std::array<std::uint8_t, 65536> tx{};

    for (;;) {
        let received = tap.read(rx);
        if (!received) {
            std::fprintf(stderr, "read: %s\n",
                         received.error().message.c_str());
            shitnet_destroy(net);
            return 1;
        }

        const let received_frame =
            std::span<const std::byte>{rx.data(), *received};
        const let receive_result = shitnet_receive(
            net, reinterpret_cast<const std::uint8_t *>(rx.data()), *received);
        if (receive_result != SHITNET_OK) {
            std::fprintf(stderr, "%-5s rx %5zu B  error %d\n",
                         frame_protocol(received_frame).data(), *received,
                         receive_result);
            continue;
        }

        while (shitnet_tx_size(net) > 0) {
            std::size_t written = 0;
            const let poll_result =
                shitnet_poll_tx(net, tx.data(), tx.size(), &written);
            if (poll_result != 1) {
                if (poll_result < 0)
                    std::fprintf(stderr, "tx queue error: %d\n", poll_result);
                break;
            }

            const let frame = std::span<const std::byte>{
                reinterpret_cast<const std::byte *>(tx.data()),
                written,
            };

            let sent = tap.write(frame);
            if (!sent) {
                std::fprintf(stderr, "write: %s\n",
                             sent.error().message.c_str());
                shitnet_destroy(net);
                return 1;
            }

            std::printf("%-5s rx %5zu B  ->  tx %5zu B\n",
                        frame_protocol(received_frame).data(), *received,
                        written);
        }
    }
}

fn run_arp_request(std::string_view target_text) -> int {
    std::array<std::uint8_t, 4> target{};
    if (!parse_ipv4(target_text, target)) {
        std::fprintf(stderr, "invalid IPv4 address: %.*s\n",
                     static_cast<int>(target_text.size()), target_text.data());
        return 1;
    }

    let tap_result = Tap::open("shitnet0");
    if (!tap_result) {
        std::fprintf(stderr, "failed to open TAP: %s\n",
                     tap_result.error().message.c_str());
        return 1;
    }

    let tap = std::move(*tap_result);
    shitnet *net = shitnet_create(&default_config);
    if (net == nullptr) {
        std::fprintf(stderr, "failed to create shitnet\n");
        return 1;
    }

    std::printf("sending ARP request for %s\n", target_text.data());
    if (shitnet_arp_request(net, target.data()) != SHITNET_QUEUE_QUEUED) {
        std::fprintf(stderr, "failed to queue ARP request\n");
        shitnet_destroy(net);
        return 1;
    }

    std::array<std::uint8_t, 65536> tx{};
    std::size_t written = 0;
    if (shitnet_poll_tx(net, tx.data(), tx.size(), &written) != 1) {
        std::fprintf(stderr, "failed to poll ARP request\n");
        shitnet_destroy(net);
        return 1;
    }

    const let frame = std::span<const std::byte>{
        reinterpret_cast<const std::byte *>(tx.data()),
        written,
    };
    let sent = tap.write(frame);
    if (!sent) {
        std::fprintf(stderr, "write: %s\n", sent.error().message.c_str());
        shitnet_destroy(net);
        return 1;
    }

    std::array<std::byte, 65536> rx{};
    for (;;) {
        let received = tap.read(rx);
        if (!received) {
            std::fprintf(stderr, "read: %s\n",
                         received.error().message.c_str());
            shitnet_destroy(net);
            return 1;
        }

        const let result = shitnet_receive(
            net, reinterpret_cast<const std::uint8_t *>(rx.data()), *received);
        if (result != SHITNET_OK)
            continue;

        std::array<std::uint8_t, 6> mac{};
        if (shitnet_arp_lookup(net, target.data(), mac.data()) !=
            SHITNET_LOOKUP_FOUND)
            continue;

        std::printf("learned %s -> %02x:%02x:%02x:%02x:%02x:%02x\n",
                    target_text.data(), mac[0], mac[1], mac[2], mac[3], mac[4],
                    mac[5]);
        shitnet_destroy(net);
        return 0;
    }
}

struct ArpRequestOptions {
    static constexpr std::string_view command_name = "shitnet arp-request";
    static constexpr cli::help_menu_style help_style =
        cli::help_menu_style::fancy;

    cli::option<std::string, cli::short_<"-t">, cli::long_<"--target">,
                cli::help<"Target IPv4 address">, cli::required>
        target;

    using fields = cli::fields<&ArpRequestOptions::target>;
};

struct NoOptions {
    static constexpr std::string_view command_name = "shitnet run";
    static constexpr cli::help_menu_style help_style =
        cli::help_menu_style::fancy;
    using fields = cli::fields<>;
};

using RunCommand = cli::command<"run", "Run shitnet on a Linux TAP", NoOptions>;
using ArpRequestCommand =
    cli::command<"arp-request", "Request and learn a host over TAP",
                 ArpRequestOptions>;

struct Root {
    static constexpr std::string_view command_name = "shitnet";
    static constexpr std::string_view command_description =
        "Exercise the shitnet packet stack.";
    static constexpr cli::help_menu_style help_style =
        cli::help_menu_style::fancy;
    using subcommands = cli::commands<RunCommand, ArpRequestCommand>;
};

} // namespace shitnet_cli
