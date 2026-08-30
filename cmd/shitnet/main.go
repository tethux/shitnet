package main

import (
	"context"
	"errors"
	"fmt"
	"io"
	"net/netip"
	"os"
	"os/signal"
	"strings"
	"syscall"

	"github.com/0xveya/shitnet"
	"github.com/chzyer/readline"
)

func main() {
	if runErr := run(); runErr != nil {
		fmt.Fprintln(os.Stderr, runErr)
		os.Exit(1)
	}
}

func run() error {
	ctx, cancel := signal.NotifyContext(context.Background(), syscall.SIGTERM)
	defer cancel()
	interrupts := make(chan os.Signal, 1)
	signal.Notify(interrupts, os.Interrupt)
	defer signal.Stop(interrupts)

	config := shitnet.Config{
		MAC: shitnet.MAC{0x02, 0x00, 0x00, 0x00, 0x00, 0x02},
		IP:  netip.MustParseAddr("10.0.0.2"),
	}

	vpc, createErr := NewVPC(config, "shitnet0")
	if createErr != nil {
		return createErr
	}

	runErrors := make(chan error, 1)
	go func() {
		runErrors <- vpc.Run(ctx)
		cancel()
	}()

	replErr := runREPL(ctx, vpc, interrupts)
	cancel()
	ownerErr := <-runErrors
	closeErr := vpc.Close()

	if errors.Is(replErr, context.Canceled) {
		replErr = nil
	}
	return errors.Join(replErr, ownerErr, closeErr)
}

func runREPL(
	ctx context.Context,
	vpc *VPC,
	interrupts <-chan os.Signal,
) (resultErr error) {
	reader, openErr := readline.New("shitnet> ")
	if openErr != nil {
		return wrapError("open REPL", "", nil, openErr)
	}
	defer func() {
		resultErr = errors.Join(resultErr, reader.Close())
	}()

	for {
		line, readErr := reader.Readline()
		if errors.Is(readErr, readline.ErrInterrupt) {
			drainInterrupt(interrupts)
			continue
		}
		if errors.Is(readErr, io.EOF) {
			fmt.Println()
			return nil
		}
		if readErr != nil {
			return wrapError("read REPL", "", nil, readErr)
		}

		exit, commandErr := executeCommand(ctx, vpc, interrupts, strings.Fields(line))
		if commandErr != nil {
			if errors.Is(commandErr, context.Canceled) && ctx.Err() == nil {
				continue
			}
			fmt.Fprintln(os.Stderr, commandErr)
			continue
		}
		if exit {
			return nil
		}
	}
}
