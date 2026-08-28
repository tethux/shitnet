# shitnet

bla bla bal codex yipp yapp, i am just goofing arround with cpp and its surprisingly nice to use

A small C++23 userspace network stack. It classifies Ethernet, ARP, IPv4, and
ICMP packets, replies to ARP requests and ICMP echo requests, and maintains a
small ARP table.

## Build

The project uses [xmake](https://xmake.io/) and pins its development tools with
[mise](https://mise.jdx.dev/).

```sh
mise run dev
```

For an optimized build:

```sh
mise run build
```

## Commands

The CLI is a typed multicall built with the local framework in `cli/framework`.
Running it without a command prints the generated menu:

```sh
mise run run --
```

Send an ARP request to the stack's `10.0.0.2` address:

```sh
mise run run -- arp-request --target 2
```

This produces one 42-byte ARP reply. Another target produces no outgoing frame:

```sh
mise run run -- arp-request --target 67
```

Send an ARP reply and verify that its sender is learned:

```sh
mise run run -- arp-learn
# 10.0.0.1 -> aa:bb:cc:dd:ee:ff
```

The remaining commands exercise reply handling and invalid or unsupported ARP
fields:

```sh
mise run run -- arp-reply
mise run run -- arp-bad-hardware
mise run run -- arp-bad-protocol
mise run run -- arp-bad-length
mise run run -- arp-bad-operation
```

## Checks

Run the packet-stack and CLI-framework tests with:

```sh
mise run test
mise run test:parser
```

`mise run dev` builds every default target with debug checks. `mise run build`
does the same in release mode.

## Layout

- `modules/` contains the packet types, classifiers, and reusable match module.
- `src/` contains the C API implementation and packet handling.
- `cli/framework/` contains the declarative parser and diagnostic modules.
- `cli/shitnet-commands.cppm` declares the real multicall command tree.
- `cli/shitnet-arp.cppm` contains the CLI's ARP scenarios.
