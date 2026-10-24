package console

import (
	"bytes"
	"context"
	"errors"
	"net/netip"
	"strings"
	"sync"
	"testing"
	"time"

	"github.com/tethux/shitnet"
)

type stalledWriter struct {
	started chan struct{}
	release chan struct{}
}

func (w *stalledWriter) Write(value []byte) (int, error) {
	select {
	case <-w.started:
	default:
		close(w.started)
	}
	<-w.release
	return len(value), nil
}

// A single owner drives commands and both real native endpoints over an in-memory wire.
func TestLiveConsole(t *testing.T) {
	config := shitnet.Config{IP: netip.MustParseAddr("10.0.0.2"), MAC: shitnet.MAC{2, 0, 0, 0, 0, 2}}
	peerConfig := shitnet.Config{IP: netip.MustParseAddr("10.0.0.1"), MAC: shitnet.MAC{2, 0, 0, 0, 0, 1}}
	stack, createErr := shitnet.New(config)
	if createErr != nil {
		t.Fatal(createErr)
	}
	defer stack.Close()
	peer, peerErr := shitnet.New(peerConfig)
	if peerErr != nil {
		t.Fatal(peerErr)
	}
	defer peer.Close()
	controller := New(stack, config, "live")
	defer controller.Close()
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()
	exchange := func() {
		for {
			progress := false
			for _, pair := range [][2]*shitnet.Shitnet{{stack, peer}, {peer, stack}} {
				for {
					poll, pollErr := pair[0].PollEvent()
					if pollErr != nil {
						t.Fatal(pollErr)
					}
					if !poll.Available {
						break
					}
					if pair[0] == stack {
						controller.Event(poll.Event)
					}
				}
				for {
					poll, pollErr := pair[0].PollTX()
					if pollErr != nil {
						t.Fatal(pollErr)
					}
					if !poll.Available {
						break
					}
					progress = true
					if receiveErr := pair[1].Receive(poll.Frame); receiveErr != nil {
						t.Fatal(receiveErr)
					}
				}
			}
			if !progress {
				return
			}
		}
	}
	execute := func(command string, want string) {
		t.Helper()
		var output bytes.Buffer
		completed := make(chan error, 1)
		go func() { _, executeErr := controller.Execute(ctx, command, &output); completed <- executeErr }()
		for {
			select {
			case request := <-controller.Requests():
				controller.Handle(request)
				exchange()
			case <-controller.NextPing():
				controller.Tick()
				exchange()
			case <-controller.Canceled():
				controller.Cancel()
			case executeErr := <-completed:
				if executeErr != nil {
					t.Fatal(executeErr)
				}
				if !strings.Contains(output.String(), want) {
					t.Fatalf("%s: %q does not contain %q", command, output.String(), want)
				}
				return
			case <-ctx.Done():
				t.Fatal(ctx.Err())
			}
		}
	}
	execute("show", "10.0.0.2")
	execute("help", "ping")
	execute("arp", "empty")
	execute("ping -c 2 10.0.0.1", "2 packets transmitted, 2 received")
	execute("arp", "10.0.0.1")
	execute("arp 10.0.0.1", "02:00:00:00:00:01")
	exit, exitErr := controller.Execute(ctx, "exit", &bytes.Buffer{})
	if exitErr != nil || !exit {
		t.Fatalf("exit: %t, %v", exit, exitErr)
	}
	execute("show", "live")

	// A stalled session cannot hold up another command or the packet owner.
	writer := &stalledWriter{started: make(chan struct{}), release: make(chan struct{})}
	var releaseOnce sync.Once
	releaseWriter := func() { releaseOnce.Do(func() { close(writer.release) }) }
	defer releaseWriter()
	stalledDone := make(chan error, 1)
	go func() {
		_, executeErr := controller.Execute(ctx, "ping -c 1 10.0.0.1", writer)
		stalledDone <- executeErr
	}()
	select {
	case request := <-controller.Requests():
		controller.Handle(request)
		exchange()
	case <-ctx.Done():
		t.Fatal(ctx.Err())
	}
	select {
	case <-writer.started:
	case <-ctx.Done():
		t.Fatal(ctx.Err())
	}
	execute("show", "live")
	releaseWriter()
	select {
	case executeErr := <-stalledDone:
		if executeErr != nil {
			t.Fatal(executeErr)
		}
	case <-ctx.Done():
		t.Fatal(ctx.Err())
	}

	// Detaching an unanswered command clears only its own pending work.
	pingCtx, stopPing := context.WithCancel(ctx)
	completed := make(chan error, 1)
	go func() {
		_, executeErr := controller.Execute(pingCtx, "ping 10.0.0.99", &bytes.Buffer{})
		completed <- executeErr
	}()
	select {
	case request := <-controller.Requests():
		controller.Handle(request)
		exchange()
	case <-ctx.Done():
		t.Fatal(ctx.Err())
	}
	stopPing()
	select {
	case executeErr := <-completed:
		if !errors.Is(executeErr, context.Canceled) {
			t.Fatal(executeErr)
		}
	case <-ctx.Done():
		t.Fatal(ctx.Err())
	}
	select {
	case <-controller.Canceled():
		controller.Cancel()
	default:
		t.Fatal("missing cancellation wakeup")
	}
	execute("ping -c 1 10.0.0.1", "1 packets transmitted, 1 received")

	// Closing the controller wakes a submitted command and preserves native state.
	go func() {
		_, executeErr := controller.Execute(ctx, "ping 10.0.0.99", &bytes.Buffer{})
		completed <- executeErr
	}()
	select {
	case request := <-controller.Requests():
		controller.Handle(request)
	case <-ctx.Done():
		t.Fatal(ctx.Err())
	}
	controller.Close()
	select {
	case executeErr := <-completed:
		if !errors.Is(executeErr, ErrClosed) {
			t.Fatal(executeErr)
		}
	case <-ctx.Done():
		t.Fatal(ctx.Err())
	}
	lookup, lookupErr := stack.LookupARP(peerConfig.IP)
	if lookupErr != nil || !lookup.Found {
		t.Fatalf("endpoint lost after detach: %+v, %v", lookup, lookupErr)
	}
}
