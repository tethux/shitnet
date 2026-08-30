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

## Go API

The binding API is documented with GoDoc. Its public reference will be available
at [pkg.go.dev/github.com/tethux/shitnet](https://pkg.go.dev/github.com/tethux/shitnet)
after pkg.go.dev indexes the module. Inspect it locally with:

```sh
mise exec -- go doc github.com/tethux/shitnet
mise exec -- go doc github.com/tethux/shitnet.Shitnet
mise exec -- go doc github.com/tethux/shitnet/errs
```

The API is still experimental. Tethux will use this package once the stack and
its integration contracts are ready.

## Go REPL

Build the native library used by cgo:

```sh
mise run dev
```

Create and configure a TAP owned by your user:

```sh
pkexec ip tuntap add dev shitnet0 mode tap user "$(id -un)"
pkexec ip addr replace 10.0.0.1/24 dev shitnet0
pkexec ip link set shitnet0 up
```

Start the REPL without privilege escalation:

```sh
mise exec -- go run ./cmd/shitnet
```

Available commands:

```text
help
show
arp
arp <ipv4>
ping <ipv4>
ping -c <count> <ipv4>
exit
quit
```

Try the outbound path from the REPL:

```text
shitnet> show
interface  shitnet0
address    10.0.0.2
mac        02:00:00:00:00:02

shitnet> arp
ARP table is empty

shitnet> ping -c 3 10.0.0.1
reply from 10.0.0.1: icmp_seq=1
reply from 10.0.0.1: icmp_seq=2
reply from 10.0.0.1: icmp_seq=3
3 packets transmitted, 3 received

shitnet> arp
ADDRESS   MAC
10.0.0.1  <host-mac>
```

`arp` reads the native C++ stack's learned table. `arp <ipv4>` looks up one
entry and sends an ARP request when it is missing. `ping <ipv4>` continues
until Ctrl-C; `-c` selects a positive count. Ping performs ARP resolution
itself when the entry is not already known and spaces requests one second
apart.

While the REPL is running, another terminal can test the inbound path:

```sh
ping 10.0.0.2
```

Exit the REPL, then remove the TAP:

```sh
pkexec ip link delete shitnet0
```

## C++ CLI

Show the native CLI commands:

```sh
xmake run shitnet-cli --help
```

Run the native stack on the configured TAP:

```sh
xmake run shitnet-cli run
```

Or test one outbound ARP request:

```sh
xmake run shitnet-cli arp-request --target 10.0.0.1
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
- `cmd/shitnet/` contains the Go REPL and its single-owner packet pump.
- `internal/tap/` owns the Go Linux TAP boundary.
- `cli/framework/` contains the declarative parser and diagnostic modules.
- `cli/shitnet-commands.cppm` declares the real multicall command tree.
- `cli/shitnet-tap.cppm` owns the Linux TAP device boundary.
