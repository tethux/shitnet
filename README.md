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

The API is still experimental. Linux amd64 Go consumers use the bundled native
archive and headers:

```sh
go get github.com/tethux/shitnet@latest
```

Build with cgo enabled and a C compiler and C++ runtime development files
installed. Consumers do not need Xmake, pkg-config, or a separate Shitnet
installation on Linux amd64. Other platforms use the native pkg-config package.

After native changes, run `mise run package:go` and commit the updated archive
before tagging a Go module release. `mise run test:go` exercises the bundled
archive; `mise run test:go:local` exercises the installed development library
through the `shitnet_local` build tag.

`mise run release:snapshot` checks the Go bindings and packages the Linux amd64
CLI, native library, headers, pkg-config file, and checksums without publishing.
Pushing a new `v*` tag runs GoReleaser and publishes the GitHub release. Go
consumers download the tagged module, including its bundled archive.

The file `pkgconfig/shitnet.pc` tells `pkg-config` where the installed headers
and archive are and which libraries to link. Install a release build for an
external Go consumer with:

```sh
mise exec -- xmake f -m release -y
mise exec -- xmake build -y shitnet
mise exec -- xmake install -o /path/to/prefix shitnet
PKG_CONFIG_PATH=/path/to/prefix/lib/pkgconfig pkg-config --cflags --libs shitnet
```

Set `PKG_CONFIG_PATH` to that directory when building the consumer.

## Reusable Go console

`github.com/tethux/shitnet/console` exposes the same networking commands as the
REPL, without a terminal, TAP, shell, or service. Create one controller per live
endpoint, borrowing the stack that the application already owns:

```go
controller := console.New(stack, config, "node1")
// From a session goroutine, using a context canceled on detach:
exit, err := controller.Execute(sessionCtx, "ping -c 3 10.0.0.1", output)
```

`Execute` accepts one whitespace-separated command line and writes text to an
`io.Writer`. `exit` and `quit` return `true` so the caller can detach that session.
They do not close the controller or endpoint. Independent sessions may call
`Execute` concurrently with separate contexts and writers; one ping may be
active per endpoint, otherwise `console.ErrPingPending` is returned. Errors
support `errors.Is` and `errors.As` through `console.Operation`.

The existing endpoint owner must service these cases alongside packet input:

```go
case request := <-controller.Requests():
    controller.Handle(request)
case <-controller.NextPing():
    controller.Tick()
case <-controller.Canceled():
    controller.Cancel()
```

Serialize these owner hooks, `Event`, and `Close` with all native stack calls.
Forward each event already drained by the owner to `controller.Event(event)`.
Drain and transmit queued frames after `Handle`, `Tick`, and `Event` through the
existing TX path. The controller never receives frames, polls TX/events, starts
packet workers, or allocates another stack. See `cmd/shitnet/vpc.go` for the
complete owner loop and `console/controller_test.go` for two native endpoints
exchanging Ethernet frames without privileged networking.

Cancel the session context on detach. Call `controller.Close()` on the owner
when ending console access, before freeing the stack; it stops timers and wakes
waiting commands with `console.ErrClosed`, retaining the endpoint. Transport
writers must return promptly or be unblocked when their session ends: context
cancellation cannot interrupt an arbitrary `Write`. Packet handling never waits
for session output. Each command has a bounded output queue; a ping that fills
it ends with `console.ErrOutputFull`.

Ping retains the REPL's reply-paced behavior: it resolves ARP, sends the next
request one second after a reply, and waits for context cancellation when an
address or reply does not arrive. It does not implement loss timeouts or
retransmissions. This API supplies commands only; transport authentication,
line editing, session input, and endpoint lifetime belong to the caller.

Run the focused native console and cancellation check:

```nu
mise exec -- go test -race ./console ./cmd/shitnet
```

## Go REPL

Build and install the local native library used by cgo:

```sh
mise run build:native
```

Create and configure a TAP owned by your user:

```sh
pkexec ip tuntap add dev shitnet0 mode tap user "$(id -un)"
pkexec ip addr replace 10.0.0.1/24 dev shitnet0
pkexec ip link set shitnet0 up
```

Start the REPL without privilege escalation:

```sh
PKG_CONFIG_PATH="$PWD/build/install/lib/pkgconfig" mise exec -- go run ./cmd/shitnet
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
- `shitnet.go` exposes the public Go API; `internal/binding` contains its native bindings.
- `errs/` contains Go error categories and typed operation errors.
- `console/` contains reusable commands for an externally owned native endpoint.
- `cmd/shitnet/` contains the Go REPL and its single-owner packet pump.
- `internal/tap/` owns the Go Linux TAP boundary.
- `cli/framework/` contains the declarative parser and diagnostic modules.
- `cli/shitnet-commands.cppm` declares the real multicall command tree.
- `cli/shitnet-tap.cppm` owns the Linux TAP device boundary.

## Architecture checks

`mise run lint:architecture` checks `.repolint-architecture.toml` using the sibling
`../repolint` checkout. Set `REPOLINT_SOURCE` for another checkout or `REPOLINT_BIN`
for a built checker containing the new rules. `mise run check` combines these
contracts with the existing Go policy, lint, and binding test tasks. Required
rules cannot be disabled through the configuration.

The published Go tool still reads `.repolint.toml`. The architecture checker
needs a repolint release before its rules can join the published-tool CI gate.
