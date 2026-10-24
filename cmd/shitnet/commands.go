package main

import (
	"context"
	"os"

	"github.com/tethux/shitnet/console"
)

func executeCommand(ctx context.Context, controller *console.Controller, interrupts <-chan os.Signal, line string) (bool, error) {
	commandCtx, cancel := context.WithCancel(ctx)
	done := make(chan struct{})
	defer func() { cancel(); <-done }()
	go func() {
		defer close(done)
		select {
		case <-interrupts:
			cancel()
		case <-commandCtx.Done():
		}
	}()
	return controller.Execute(commandCtx, line, os.Stdout)
}

func drainInterrupt(interrupts <-chan os.Signal) {
	select {
	case <-interrupts:
	default:
	}
}
