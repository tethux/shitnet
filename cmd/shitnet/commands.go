package main

import (
	"context"
	"fmt"
	"net/netip"
	"os"
	"strconv"
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
	{command: "exit", description: "exit shitnet"},
}

func executeCommand(
	ctx context.Context,
	vpc *VPC,
	interrupts <-chan os.Signal,
	args []string,
) (bool, error) {
	if len(args) == 0 {
		return false, nil
	}

	switch args[0] {
	case "exit", "quit":
		return true, nil
	case "help":
		return false, printHelp()
	case "show":
		if len(args) != 1 {
			return false, wrapError("show", "usage: show", errUsage, nil)
		}
		return false, printResponse(ctx, vpc, interrupts, Request{Type: RequestShow})
	case "arp":
		if len(args) == 1 {
			return false, printResponse(ctx, vpc, interrupts, Request{Type: RequestARPTable})
		}
		address, parseErr := commandAddress("arp", args)
		if parseErr != nil {
			return false, parseErr
		}
		return false, printResponse(ctx, vpc, interrupts, Request{Type: RequestARP, IP: address})
	case "ping":
		options, parseErr := parsePing(args)
		if parseErr != nil {
			return false, parseErr
		}
		return false, printResponse(ctx, vpc, interrupts, Request{
			Type:  RequestPing,
			IP:    options.Address,
			Count: options.Count,
		})
	default:
		return false, wrapError("command", args[0], errUnknownCommand, nil)
	}
}

func printHelp() error {
	w := tabwriter.NewWriter(os.Stdout, 0, 0, 2, ' ', 0)
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
			errUsage,
			nil,
		)
	}

	address, parseErr := netip.ParseAddr(args[1])
	if parseErr != nil || !address.Is4() {
		return netip.Addr{}, wrapError(command, args[1], errInvalidIPv4, parseErr)
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
			return pingOptions{}, wrapError("ping", args[2], errInvalidCount, countErr)
		}
		count = parsed
	case len(args) == 4 && args[2] == "-c":
		addressText = args[1]
		parsed, countErr := strconv.ParseUint(args[3], 10, 16)
		if countErr != nil || parsed == 0 {
			return pingOptions{}, wrapError("ping", args[3], errInvalidCount, countErr)
		}
		count = parsed
	default:
		return pingOptions{}, wrapError(
			"ping",
			"usage: ping [-c count] <ipv4>",
			errUsage,
			nil,
		)
	}

	address, addressErr := netip.ParseAddr(addressText)
	if addressErr != nil || !address.Is4() {
		return pingOptions{}, wrapError("ping", addressText, errInvalidIPv4, addressErr)
	}
	return pingOptions{Address: address, Count: uint16(count)}, nil
}

func printResponse(
	ctx context.Context,
	vpc *VPC,
	interrupts <-chan os.Signal,
	request Request,
) error {
	commandCtx, cancel := context.WithCancel(ctx)
	defer cancel()

	responses, requestErr := submit(commandCtx, vpc, request)
	if requestErr != nil {
		return requestErr
	}

	for {
		select {
		case response := <-responses:
			if response.Error != nil {
				return response.Error
			}
			if response.Message != "" {
				fmt.Println(response.Message)
			}
			if response.Done {
				return nil
			}
		case <-ctx.Done():
			return ctx.Err()
		case <-interrupts:
			cancel()
			cancelErr := cancelPendingPing(ctx, vpc)
			if cancelErr != nil {
				return cancelErr
			}
			return context.Canceled
		}
	}
}

func submit(ctx context.Context, vpc *VPC, request Request) (<-chan Response, error) {
	response := make(chan Response, 1)
	request.Response = response
	request.Context = ctx

	select {
	case vpc.requests <- request:
	case <-ctx.Done():
		return nil, ctx.Err()
	}
	return response, nil
}

func cancelPendingPing(ctx context.Context, vpc *VPC) error {
	responses, requestErr := submit(ctx, vpc, Request{Type: RequestCancelPing})
	if requestErr != nil {
		return requestErr
	}
	select {
	case response := <-responses:
		return response.Error
	case <-ctx.Done():
		return ctx.Err()
	}
}

func drainInterrupt(interrupts <-chan os.Signal) {
	select {
	case <-interrupts:
	default:
	}
}
