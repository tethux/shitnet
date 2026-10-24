package main

import (
	"context"
	"errors"

	"github.com/tethux/shitnet"
	"github.com/tethux/shitnet/console"
	"github.com/tethux/shitnet/internal/tap"
)

type VPC struct {
	stack   *shitnet.Shitnet
	tap     *tap.Device
	frames  chan []byte
	console *console.Controller
}

func NewVPC(
	config shitnet.Config,
	interfaceName string,
) (*VPC, error) {
	device, openErr := tap.Open(interfaceName)
	if openErr != nil {
		return nil, wrapError("create VPC", interfaceName, errCreateVPC, openErr)
	}

	stack, stackErr := shitnet.New(config)
	if stackErr != nil {
		closeErr := device.Close()
		return nil, wrapError(
			"create VPC",
			interfaceName,
			errCreateVPC,
			errors.Join(stackErr, closeErr),
		)
	}

	return &VPC{
		console: console.New(stack, config, device.Name()),
		stack:   stack,
		tap:     device,

		frames: make(chan []byte),
	}, nil
}

func (v *VPC) Close() error {
	if v == nil {
		return nil
	}

	if v.console != nil {
		v.console.Close()
	}
	if v.stack != nil {
		v.stack.Close()
		v.stack = nil
	}

	if v.tap != nil {
		closeErr := v.tap.Close()
		v.tap = nil
		if closeErr != nil {
			return closeErr
		}
	}
	return nil
}

func (v *VPC) Run(ctx context.Context) (resultErr error) {
	ctx, cancel := context.WithCancel(ctx)
	defer v.console.Close()
	readerErrors := make(chan error, 1)
	readerDone := make(chan struct{})
	go func() {
		defer close(readerDone)
		readerErrors <- v.readFrames(ctx)
	}()

	defer func() {
		cancel()
		closeErr := v.tap.Close()
		<-readerDone
		resultErr = errors.Join(resultErr, closeErr)
	}()
	for {
		select {
		case frame := <-v.frames:
			if receiveErr := v.receive(frame); receiveErr != nil {
				return receiveErr
			}
		case request := <-v.console.Requests():
			v.console.Handle(request)
			if transmitErr := v.drainTX(); transmitErr != nil {
				return transmitErr
			}
		case <-v.console.NextPing():
			v.console.Tick()
			if transmitErr := v.drainTX(); transmitErr != nil {
				return transmitErr
			}
		case <-v.console.Canceled():
			v.console.Cancel()
		case readErr := <-readerErrors:
			if ctx.Err() != nil {
				return nil
			}
			return readErr
		case <-ctx.Done():
			return nil
		}
	}
}

func (v *VPC) readFrames(ctx context.Context) error {
	buffer := make([]byte, 65536)

	for {
		read, readErr := v.tap.Read(buffer)
		if readErr != nil {
			return readErr
		}
		frame := make([]byte, read)
		copy(frame, buffer[:read])
		select {
		case v.frames <- frame:
		case <-ctx.Done():
			return ctx.Err()
		}

	}
}

func (v *VPC) receive(frame []byte) error {
	receiveErr := v.stack.Receive(frame)
	if receiveErr != nil {
		return receiveErr
	}

	eventErr := v.drainEvents()
	if eventErr != nil {
		return eventErr
	}

	transmitErr := v.drainTX()
	if transmitErr != nil {
		return transmitErr
	}

	return nil
}

func (v *VPC) drainTX() error {
	for {
		poll, pollErr := v.stack.PollTX()
		if pollErr != nil {
			return pollErr
		}

		if !poll.Available {
			return nil
		}

		writeErr := v.tap.Write(poll.Frame)
		if writeErr != nil {
			return writeErr
		}
	}
}

func (v *VPC) drainEvents() error {
	for {
		poll, pollErr := v.stack.PollEvent()
		if pollErr != nil {
			return pollErr
		}

		if !poll.Available {
			return nil
		}

		v.console.Event(poll.Event)
	}
}
