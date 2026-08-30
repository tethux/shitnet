package main

import (
	"context"
	"errors"
	"fmt"
	"net/netip"
	"strings"
	"text/tabwriter"
	"time"

	"github.com/tethux/shitnet"
	"github.com/tethux/shitnet/internal/tap"
)

type VPC struct {
	config shitnet.Config
	stack  *shitnet.Shitnet
	tap    *tap.Device

	frames   chan []byte
	requests chan Request

	pendingPing *PendingPing
}

type PendingPing struct {
	Target     netip.Addr
	Identifier uint16
	Sequence   uint16
	Count      uint16
	Received   uint16
	Response   chan Response
	Context    context.Context
	timer      *time.Timer
}

type RequestType uint8

const (
	RequestShow RequestType = iota
	RequestARPTable
	RequestARP
	RequestPing
	RequestCancelPing
)

type Request struct {
	Type     RequestType
	IP       netip.Addr
	Count    uint16
	Response chan Response
	Context  context.Context
}

type Response struct {
	Message string
	Error   error
	Done    bool
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
		config: config,
		stack:  stack,
		tap:    device,

		frames:   make(chan []byte),
		requests: make(chan Request),
	}, nil
}

func (v *VPC) Close() error {
	if v == nil {
		return nil
	}

	if v.stack != nil {
		v.stack.Close()
		v.stack = nil
	}
	if v.pendingPing != nil && v.pendingPing.timer != nil {
		v.pendingPing.timer.Stop()
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

func (v *VPC) Run(ctx context.Context) error {
	readerErrors := make(chan error, 1)
	go func() {
		readerErrors <- v.readFrames(ctx)
	}()

	for {
		select {
		case frame := <-v.frames:
			if receiveErr := v.receive(frame); receiveErr != nil {
				return receiveErr
			}
		case request := <-v.requests:
			v.handleRequest(request)
		case <-v.nextPing():
			v.pendingPing.timer = nil
			sendErr := v.sendPing()
			if sendErr != nil {
				v.failPing(sendErr)
			}
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

		switch poll.Event.Type {
		case shitnet.EventARPLearned:
			if v.pendingPing == nil || poll.Event.ARP == nil ||
				poll.Event.ARP.IP != v.pendingPing.Target {
				continue
			}

			sendErr := v.sendPing()
			if sendErr != nil {
				v.failPing(sendErr)
			}

		case shitnet.EventICMPEchoRequest:
			continue

		case shitnet.EventICMPEchoReply:
			if v.pendingPing == nil || poll.Event.ICMP == nil {
				continue
			}

			reply := poll.Event.ICMP
			ping := v.pendingPing
			if reply.Source != ping.Target ||
				reply.Identifier != ping.Identifier ||
				reply.Sequence != ping.Sequence {
				continue
			}

			v.emitPing(fmt.Sprintf(
				"reply from %s: icmp_seq=%d",
				reply.Source,
				reply.Sequence,
			))
			ping.Received++
			if ping.Count == 0 || ping.Received < ping.Count {
				ping.Sequence++
				ping.timer = time.NewTimer(time.Second)
				continue
			}
			v.finishPing()
		}
	}
}

func (v *VPC) nextPing() <-chan time.Time {
	if v.pendingPing == nil || v.pendingPing.timer == nil {
		return nil
	}
	return v.pendingPing.timer.C
}

func (v *VPC) handleRequest(request Request) {
	switch request.Type {
	case RequestShow:
		v.respond(request, v.handleShow())

	case RequestARPTable:
		v.respond(request, v.handleARPTable())

	case RequestARP:
		v.respond(request, v.handleARP(request.IP))

	case RequestPing:
		v.handlePing(request)

	case RequestCancelPing:
		v.cancelPing()
		v.respond(request, Response{})

	default:
		v.respond(request, Response{Error: wrapError(
			"handle request",
			fmt.Sprint(request.Type),
			errUnknownRequest,
			nil,
		)})
	}
}

func (v *VPC) respond(request Request, response Response) {
	if request.Response == nil {
		return
	}
	response.Done = true
	if request.Context == nil {
		request.Response <- response
		return
	}
	select {
	case request.Response <- response:
	case <-request.Context.Done():
	}
}

func (v *VPC) handleShow() Response {
	var b strings.Builder
	w := tabwriter.NewWriter(&b, 0, 0, 2, ' ', 0)

	if _, writeErr := fmt.Fprintf(w, "interface\t%s\n", v.tap.Name()); writeErr != nil {
		return Response{Error: wrapError("show", "interface", nil, writeErr)}
	}
	if _, writeErr := fmt.Fprintf(w, "address\t%s\n", v.config.IP); writeErr != nil {
		return Response{Error: wrapError("show", "address", nil, writeErr)}
	}
	if _, writeErr := fmt.Fprintf(w, "mac\t%s\n", v.config.MAC); writeErr != nil {
		return Response{Error: wrapError("show", "mac", nil, writeErr)}
	}
	if flushErr := w.Flush(); flushErr != nil {
		return Response{Error: wrapError("show", "", nil, flushErr)}
	}

	return Response{
		Message: b.String(),
	}
}

func (v *VPC) handleARP(ip netip.Addr) Response {
	if !ip.Is4() {
		return Response{
			Error: wrapError("ARP", ip.String(), errInvalidIPv4, nil),
		}
	}

	lookup, lookupErr := v.stack.LookupARP(ip)
	if lookupErr != nil {
		return Response{
			Error: lookupErr,
		}
	}

	if lookup.Found {
		return Response{
			Message: fmt.Sprintf(
				"%s is %s",
				ip,
				lookup.MAC,
			),
		}
	}

	requestErr := v.stack.ARPRequest(ip)
	if requestErr != nil {
		return Response{
			Error: requestErr,
		}
	}

	transmitErr := v.drainTX()
	if transmitErr != nil {
		return Response{
			Error: transmitErr,
		}
	}

	return Response{
		Message: fmt.Sprintf(
			"ARP request sent for %s",
			ip,
		),
	}
}

func (v *VPC) handleARPTable() Response {
	entries, listErr := v.stack.ARPEntries()
	if listErr != nil {
		return Response{Error: listErr}
	}
	if len(entries) == 0 {
		return Response{Message: "ARP table is empty"}
	}

	var b strings.Builder
	w := tabwriter.NewWriter(&b, 0, 0, 2, ' ', 0)
	if _, writeErr := fmt.Fprintln(w, "ADDRESS\tMAC"); writeErr != nil {
		return Response{Error: wrapError("show ARP", "header", nil, writeErr)}
	}
	for _, entry := range entries {
		if _, writeErr := fmt.Fprintf(w, "%s\t%s\n", entry.IP, entry.MAC); writeErr != nil {
			return Response{Error: wrapError("show ARP", entry.IP.String(), nil, writeErr)}
		}
	}
	if flushErr := w.Flush(); flushErr != nil {
		return Response{Error: wrapError("show ARP", "", nil, flushErr)}
	}
	return Response{Message: b.String()}
}

func (v *VPC) handlePing(request Request) {
	if !request.IP.Is4() {
		v.respond(request, Response{
			Error: wrapError("ping", request.IP.String(), errInvalidIPv4, nil),
		})
		return
	}
	if v.pendingPing != nil {
		v.respond(request, Response{
			Error: wrapError("ping", request.IP.String(), errPingPending, nil),
		})
		return
	}

	v.pendingPing = &PendingPing{
		Target:     request.IP,
		Identifier: 0x1234,
		Sequence:   1,
		Count:      request.Count,
		Response:   request.Response,
		Context:    request.Context,
	}

	lookup, lookupErr := v.stack.LookupARP(request.IP)
	if lookupErr != nil {
		v.failPing(lookupErr)
		return
	}
	if lookup.Found {
		sendErr := v.sendPing()
		if sendErr != nil {
			v.failPing(sendErr)
		}
		return
	}

	arpErr := v.stack.ARPRequest(request.IP)
	if arpErr != nil {
		v.failPing(arpErr)
		return
	}
	transmitErr := v.drainTX()
	if transmitErr != nil {
		v.failPing(transmitErr)
	}
}

func (v *VPC) sendPing() error {
	if v.pendingPing == nil {
		return nil
	}

	ping := v.pendingPing
	result, requestErr := v.stack.ICMPEchoRequest(
		ping.Target,
		ping.Identifier,
		ping.Sequence,
		[]byte("01234567"),
	)
	if requestErr != nil {
		return requestErr
	}
	if result != shitnet.QueueQueued {
		return wrapError("ping", ping.Target.String(), errUnresolved, nil)
	}

	return v.drainTX()
}

func (v *VPC) emitPing(message string) {
	if v.pendingPing == nil {
		return
	}
	v.sendPingResponse(Response{Message: message})
}

func (v *VPC) finishPing() {
	if v.pendingPing == nil {
		return
	}

	ping := v.pendingPing
	if ping.timer != nil {
		ping.timer.Stop()
	}
	v.pendingPing = nil
	if ping.Response != nil {
		response := Response{
			Message: fmt.Sprintf(
				"%d packets transmitted, %d received",
				ping.Count,
				ping.Received,
			),
			Done: true,
		}
		if ping.Context == nil {
			ping.Response <- response
		} else {
			select {
			case ping.Response <- response:
			case <-ping.Context.Done():
			}
		}
	}
}

func (v *VPC) failPing(cause error) {
	if v.pendingPing == nil {
		return
	}

	ping := v.pendingPing
	if ping.timer != nil {
		ping.timer.Stop()
	}
	v.pendingPing = nil
	if ping.Response != nil {
		response := Response{
			Error: wrapError("ping", ping.Target.String(), nil, cause),
			Done:  true,
		}
		if ping.Context == nil {
			ping.Response <- response
		} else {
			select {
			case ping.Response <- response:
			case <-ping.Context.Done():
			}
		}
	}
}

func (v *VPC) sendPingResponse(response Response) {
	if v.pendingPing == nil || v.pendingPing.Response == nil {
		return
	}
	if v.pendingPing.Context == nil {
		v.pendingPing.Response <- response
		return
	}
	select {
	case v.pendingPing.Response <- response:
	case <-v.pendingPing.Context.Done():
	}
}

func (v *VPC) cancelPing() {
	if v.pendingPing == nil {
		return
	}
	if v.pendingPing.timer != nil {
		v.pendingPing.timer.Stop()
	}
	v.pendingPing = nil
}
