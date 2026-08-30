# shitnet

A small C++23 userspace network stack with a C API and Go bindings. It
classifies Ethernet, ARP, IPv4, and ICMP packets, replies to ARP and ICMP echo
requests, generates outbound ARP and ICMP echo requests, maintains an ARP
table, and exposes received packet events.

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

## Try it

Build the debug CLI and show its commands:

```sh
mise run dev
xmake run shitnet-cli --help
```

Create a TAP owned by your user:

```sh
pkexec ip tuntap add dev shitnet0 mode tap user "$(id -un)"
pkexec ip addr replace 10.0.0.1/24 dev shitnet0
pkexec ip link set shitnet0 up
```

Run the stack without privilege escalation:

```sh
xmake run shitnet-cli run
```

In another terminal, ping it:

```sh
ping 10.0.0.2
```

To test one outbound ARP request instead of running the stack:

```sh
xmake run shitnet-cli arp-request --target 10.0.0.1
```

Remove the TAP when finished:

```sh
pkexec ip link delete shitnet0
```

## Checks

Run the packet-stack and CLI-framework tests with:

```sh
mise run test
mise run test:parser
mise run test:go
mise run lint:go
```

`mise run dev` builds every default target with debug checks. `mise run build`
does the same in release mode.

## Layout

- `modules/` contains the packet types, classifiers, and reusable match module.
- `src/shitnet/stack.cppm` contains the typed C++ dataplane and stack state.
- `src/shitnet/api.cpp` is the C ABI boundary used by the CLI and future
  language bindings.
- `include/shitnet/shitnet.h` is the binding-friendly public C interface.
- `shitnet.go` exposes stack lifecycle, RX/TX, ARP, ICMP, and events to Go.
- `errs/` contains Go error categories and typed operation errors.
- `cmd/shitnet/` contains the Go command entry point.
- `cli/framework/` contains the declarative parser and diagnostic modules.
- `cli/shitnet-commands.cppm` declares the real multicall command tree.
- `cli/shitnet-tap.cppm` owns the Linux TAP device boundary.
