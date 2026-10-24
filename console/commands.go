package console

import (
	"context"
	"fmt"
	"io"
	"net/netip"
	"strconv"
	"strings"
	"text/tabwriter"
)

type helpEntry struct {
	command     string
	arguments   string
	description string
}

var helpEntries = []helpEntry{
	{command: "show", description: "show local interface configuration"},
	{command: "arp", description: "show the learned ARP table"},
	{command: "arp", arguments: "<ipv4>", description: "look up or request an ARP entry"},
	{command: "ping", arguments: "<ipv4>", description: "send ICMP until interrupted"},
	{command: "ping", arguments: "-c N <ipv4>", description: "send N ICMP echo requests"},
	{command: "help", description: "show this message"},
	{command: "exit", description: "detach this console"},
}

// Execute runs one command and returns true for exit/quit.
// Calls may run concurrently with independent writers and contexts. Cancel a session's
// context on detach. Writers must return promptly or be unblocked by their transport;
// cancellation cannot interrupt an arbitrary io.Writer.Write call.
func (vpc *Controller) Execute(
	ctx context.Context,
	line string,
	output io.Writer,
) (bool, error) {
	args := strings.Fields(line)
	select {
	case <-vpc.done:
		return false, wrapError("execute", "", ErrClosed, nil)
	default:
	}
	if ctx.Err() != nil {
		return false, ctx.Err()
	}
	if len(args) == 0 {
		return false, nil
	}

	switch args[0] {
	case "exit", "quit":
		return true, nil
	case "help":
		return false, printHelp(output)
	case "show":
		if len(args) != 1 {
			return false, wrapError("show", "usage: show", ErrUsage, nil)
		}
		return false, printResponse(ctx, vpc, output, Request{kind: requestShow})
	case "arp":
		if len(args) == 1 {
			return false, printResponse(ctx, vpc, output, Request{kind: requestARPTable})
		}
		address, parseErr := commandAddress("arp", args)
		if parseErr != nil {
			return false, parseErr
		}
		return false, printResponse(ctx, vpc, output, Request{kind: requestARP, ip: address})
	case "ping":
		options, parseErr := parsePing(args)
		if parseErr != nil {
			return false, parseErr
		}
		return false, printResponse(ctx, vpc, output, Request{
			kind:  requestPing,
			ip:    options.Address,
			count: options.Count,
		})
	default:
		return false, wrapError("command", args[0], ErrUnknownCommand, nil)
	}
}

func printHelp(output io.Writer) error {
	w := tabwriter.NewWriter(output, 0, 0, 2, ' ', 0)
	if _, writeErr := fmt.Fprintln(w, "commands:"); writeErr != nil {
		return wrapError("help", "header", nil, writeErr)
	}
	for _, entry := range helpEntries {
		if _, writeErr := fmt.Fprintf(
			w,
			"  %s\t%s\t%s\n",
			entry.command,
			entry.arguments,
			entry.description,
		); writeErr != nil {
			return wrapError("help", entry.command, nil, writeErr)
		}
	}
	if flushErr := w.Flush(); flushErr != nil {
		return wrapError("help", "", nil, flushErr)
	}
	return nil
}

func commandAddress(command string, args []string) (netip.Addr, error) {
	if len(args) != 2 {
		return netip.Addr{}, wrapError(
			command,
			"usage: "+command+" <ipv4>",
			ErrUsage,
			nil,
		)
	}

	address, parseErr := netip.ParseAddr(args[1])
	if parseErr != nil || !address.Is4() {
		return netip.Addr{}, wrapError(command, args[1], ErrInvalidIPv4, parseErr)
	}
	return address, nil
}

type pingOptions struct {
	Address netip.Addr
	Count   uint16
}

func parsePing(args []string) (pingOptions, error) {
	var addressText string
	var count uint64
	switch {
	case len(args) == 2:
		addressText = args[1]
	case len(args) == 4 && args[1] == "-c":
		addressText = args[3]
		parsed, countErr := strconv.ParseUint(args[2], 10, 16)
		if countErr != nil || parsed == 0 {
			return pingOptions{}, wrapError("ping", args[2], ErrInvalidCount, countErr)
		}
		count = parsed
	case len(args) == 4 && args[2] == "-c":
		addressText = args[1]
		parsed, countErr := strconv.ParseUint(args[3], 10, 16)
		if countErr != nil || parsed == 0 {
			return pingOptions{}, wrapError("ping", args[3], ErrInvalidCount, countErr)
		}
		count = parsed
	default:
		return pingOptions{}, wrapError(
			"ping",
			"usage: ping [-c count] <ipv4>",
			ErrUsage,
			nil,
		)
	}

	address, addressErr := netip.ParseAddr(addressText)
	if addressErr != nil || !address.Is4() {
		return pingOptions{}, wrapError("ping", addressText, ErrInvalidIPv4, addressErr)
	}
	return pingOptions{Address: address, Count: uint16(count)}, nil
}

func printResponse(ctx context.Context, vpc *Controller, output io.Writer, request Request) error {
	ctx, cancel := context.WithCancel(ctx)
	defer cancel()
	responses := make(chan commandResponse, 64)
	request.response, request.context = responses, ctx
	select {
	case vpc.requests <- request:
	case <-vpc.done:
		return wrapError("execute", "", ErrClosed, nil)
	case <-ctx.Done():
		return ctx.Err()
	}
	for {
		select {
		case response := <-responses:
			if response.Error != nil {
				return response.Error
			}
			if response.Message != "" {
				if _, writeErr := fmt.Fprintln(output, response.Message); writeErr != nil {
					return wrapError("output", "", nil, writeErr)
				}
			}
			if response.Done {
				return nil
			}
		case <-vpc.done:
			return wrapError("execute", "", ErrClosed, nil)
		case <-ctx.Done():
			return ctx.Err()
		}
	}
}
