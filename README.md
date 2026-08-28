# shitnet

bla bla bal codex yipp yapp, i am just goofing arround with cpp and its surprisingly nice to use

A small C++23 userspace network stack. It currently receives, classifies, and
replies to ARP packets, and learns sender addresses from ARP replies.

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

## CLI

Build the CLI once, then use xmake to run its ARP scenarios:

```sh
xmake build -y shitnet-cli
xmake run shitnet-cli arp request 2
```

A request for the stack's `10.0.0.2` address produces one 42-byte ARP reply.
A request for another address produces no outgoing frame:

```sh
xmake run shitnet-cli arp request 67
```

Send an ARP reply and verify that its sender was learned:

```sh
xmake run shitnet-cli arp learn
# 10.0.0.1 -> aa:bb:cc:dd:ee:ff
```

The remaining commands exercise reply handling and invalid or unsupported ARP
fields:

```sh
xmake run shitnet-cli arp reply
xmake run shitnet-cli arp bad-hardware
xmake run shitnet-cli arp bad-protocol
xmake run shitnet-cli arp bad-length
xmake run shitnet-cli arp bad-operation
```

Run the current test binary with:

```sh
mise run test
```
