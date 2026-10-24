// Package console provides networking commands for an externally owned live stack.
package console

import (
	"context"
	"fmt"
	"net/netip"
	"strings"
	"text/tabwriter"
	"time"

	"github.com/tethux/shitnet"
)

// Controller borrows a stack. All owner hooks must be serialized with native calls.
// It never polls TX/events, starts packet workers, or closes the stack.
type Controller struct {
	config      shitnet.Config
	stack       *shitnet.Shitnet
	name        string
	requests    chan Request
	pendingPing *pendingPing
	identifier  uint16
	done        chan struct{}
}

// New borrows the endpoint and its displayed configuration for its owner's lifetime.
func New(stack *shitnet.Shitnet, config shitnet.Config, name string) *Controller {
	return &Controller{stack: stack, config: config, name: name, requests: make(chan Request), done: make(chan struct{})}
}

// Requests supplies commands to the endpoint owner. Call Handle then drain TX normally.
func (v *Controller) Requests() <-chan Request { return v.requests }

// Canceled wakes the owner when the active ping's command context ends.
func (v *Controller) Canceled() <-chan struct{} {
	if v.pendingPing == nil {
		return nil
	}
	return v.pendingPing.Context.Done()
}

// Cancel releases pending command state without closing the endpoint.
func (v *Controller) Cancel() { v.failPing(context.Canceled) }

// Tick sends the next scheduled ping. The owner must drain TX afterwards.
func (v *Controller) Tick() {
	if v.pendingPing != nil && v.pendingPing.Context.Err() != nil {
		v.cancelPing()
	}
	if v.pendingPing == nil {
		return
	}
	if v.pendingPing.timer != nil {
		v.pendingPing.timer.Stop()
	}
	v.pendingPing.timer = nil
	if sendErr := v.sendPing(); sendErr != nil {
		v.failPing(sendErr)
	}
}

type pendingPing struct {
	Target      netip.Addr
	Identifier  uint16
	Sequence    uint16
	Count       uint16
	Received    uint16
	Response    chan commandResponse
	Context     context.Context
	timer       *time.Timer
	awaitingARP bool
}

type requestType uint8

const (
	requestShow requestType = iota
	requestARPTable
	requestARP
	requestPing
)

// Request is an opaque command submitted by Execute.
type Request struct {
	kind     requestType
	ip       netip.Addr
	count    uint16
	response chan commandResponse
	context  context.Context
}

type commandResponse struct {
	Message string
	Error   error
	Done    bool
}

// Event observes an event drained by the owner; it may queue TX.
func (v *Controller) Event(event shitnet.Event) {
	if v.pendingPing != nil && v.pendingPing.Context.Err() != nil {
		v.cancelPing()
	}
	switch event.Type {
	case shitnet.EventARPLearned:
		if v.pendingPing == nil || event.ARP == nil ||
			event.ARP.IP != v.pendingPing.Target || !v.pendingPing.awaitingARP {
			return
		}

		sendErr := v.sendPing()
		if sendErr != nil {
			v.failPing(sendErr)
		}

	case shitnet.EventICMPEchoRequest:
		return

	case shitnet.EventICMPEchoReply:
		if v.pendingPing == nil || event.ICMP == nil {
			return
		}

		reply := event.ICMP
		ping := v.pendingPing
		if reply.Source != ping.Target ||
			reply.Identifier != ping.Identifier ||
			reply.Sequence != ping.Sequence {
			return
		}

		v.emitPing(fmt.Sprintf(
			"reply from %s: icmp_seq=%d",
			reply.Source,
			reply.Sequence,
		))
		if v.pendingPing == nil {
			return
		}
		ping.Received++
		if ping.Count == 0 || ping.Received < ping.Count {
			ping.Sequence++
			ping.timer = time.NewTimer(time.Second)
			return
		}
		v.finishPing()
	}
}

// NextPing supplies the next timer to the owner select loop.
func (v *Controller) NextPing() <-chan time.Time {
	if v.pendingPing == nil || v.pendingPing.timer == nil {
		return nil
	}
	return v.pendingPing.timer.C
}

// Handle executes a request received from Requests on the endpoint owner.
func (v *Controller) Handle(request Request) {
	if request.context == nil {
		return
	}
	select {
	case <-v.done:
		return
	default:
	}
	if request.context.Err() != nil {
		return
	}
	if v.pendingPing != nil && v.pendingPing.Context.Err() != nil {
		v.cancelPing()
	}
	switch request.kind {
	case requestShow:
		v.respond(request, v.handleShow())

	case requestARPTable:
		v.respond(request, v.handleARPTable())

	case requestARP:
		v.respond(request, v.handleARP(request.ip))

	case requestPing:
		v.handlePing(request)

	default:
		v.respond(request, commandResponse{Error: wrapError(
			"handle request",
			fmt.Sprint(request.kind),
			ErrUnknownRequest,
			nil,
		)})
	}
}

func (v *Controller) respond(request Request, response commandResponse) {
	response.Done = true
	select {
	case request.response <- response:
	default:
	}
}

func (v *Controller) handleShow() commandResponse {
	var b strings.Builder
	w := tabwriter.NewWriter(&b, 0, 0, 2, ' ', 0)

	if _, writeErr := fmt.Fprintf(w, "interface\t%s\n", v.name); writeErr != nil {
		return commandResponse{Error: wrapError("show", "interface", nil, writeErr)}
	}
	if _, writeErr := fmt.Fprintf(w, "address\t%s\n", v.config.IP); writeErr != nil {
		return commandResponse{Error: wrapError("show", "address", nil, writeErr)}
	}
	if _, writeErr := fmt.Fprintf(w, "mac\t%s\n", v.config.MAC); writeErr != nil {
		return commandResponse{Error: wrapError("show", "mac", nil, writeErr)}
	}
	if flushErr := w.Flush(); flushErr != nil {
		return commandResponse{Error: wrapError("show", "", nil, flushErr)}
	}

	return commandResponse{
		Message: b.String(),
	}
}

func (v *Controller) handleARP(ip netip.Addr) commandResponse {
	if !ip.Is4() {
		return commandResponse{
			Error: wrapError("ARP", ip.String(), ErrInvalidIPv4, nil),
		}
	}

	lookup, lookupErr := v.stack.LookupARP(ip)
	if lookupErr != nil {
		return commandResponse{
			Error: lookupErr,
		}
	}

	if lookup.Found {
		return commandResponse{
			Message: fmt.Sprintf(
				"%s is %s",
				ip,
				lookup.MAC,
			),
		}
	}

	requestErr := v.stack.ARPRequest(ip)
	if requestErr != nil {
		return commandResponse{
			Error: requestErr,
		}
	}

	return commandResponse{
		Message: fmt.Sprintf(
			"ARP request sent for %s",
			ip,
		),
	}
}

func (v *Controller) handleARPTable() commandResponse {
	entries, listErr := v.stack.ARPEntries()
	if listErr != nil {
		return commandResponse{Error: listErr}
	}
	if len(entries) == 0 {
		return commandResponse{Message: "ARP table is empty"}
	}

	var b strings.Builder
	w := tabwriter.NewWriter(&b, 0, 0, 2, ' ', 0)
	if _, writeErr := fmt.Fprintln(w, "ADDRESS\tMAC"); writeErr != nil {
		return commandResponse{Error: wrapError("show ARP", "header", nil, writeErr)}
	}
	for _, entry := range entries {
		if _, writeErr := fmt.Fprintf(w, "%s\t%s\n", entry.IP, entry.MAC); writeErr != nil {
			return commandResponse{Error: wrapError("show ARP", entry.IP.String(), nil, writeErr)}
		}
	}
	if flushErr := w.Flush(); flushErr != nil {
		return commandResponse{Error: wrapError("show ARP", "", nil, flushErr)}
	}
	return commandResponse{Message: b.String()}
}

func (v *Controller) handlePing(request Request) {
	if !request.ip.Is4() {
		v.respond(request, commandResponse{
			Error: wrapError("ping", request.ip.String(), ErrInvalidIPv4, nil),
		})
		return
	}
	if v.pendingPing != nil {
		v.respond(request, commandResponse{
			Error: wrapError("ping", request.ip.String(), ErrPingPending, nil),
		})
		return
	}

	v.identifier++
	v.pendingPing = &pendingPing{
		Target:     request.ip,
		Identifier: v.identifier,
		Sequence:   1,
		Count:      request.count,
		Response:   request.response,
		Context:    request.context,
	}

	lookup, lookupErr := v.stack.LookupARP(request.ip)
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

	v.pendingPing.awaitingARP = true
	arpErr := v.stack.ARPRequest(request.ip)
	if arpErr != nil {
		v.failPing(arpErr)
		return
	}
}

func (v *Controller) sendPing() error {
	if v.pendingPing == nil {
		return nil
	}

	ping := v.pendingPing
	ping.awaitingARP = false
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
		return wrapError("ping", ping.Target.String(), ErrUnresolved, nil)
	}

	return nil
}

func (v *Controller) emitPing(message string) {
	if v.pendingPing == nil {
		return
	}
	v.sendPingResponse(commandResponse{Message: message})
}

func (v *Controller) finishPing() {
	ping := v.pendingPing
	if ping == nil {
		return
	}
	v.sendPingResponse(commandResponse{Message: fmt.Sprintf("%d packets transmitted, %d received", ping.Count, ping.Received), Done: true})
	v.cancelPing()
}

func (v *Controller) failPing(cause error) {
	if v.pendingPing == nil {
		return
	}
	v.sendPingResponse(commandResponse{Error: wrapError("ping", v.pendingPing.Target.String(), nil, cause), Done: true})
	v.cancelPing()
}

func (v *Controller) sendPingResponse(response commandResponse) {
	if v.pendingPing == nil {
		return
	}
	if len(v.pendingPing.Response) >= cap(v.pendingPing.Response)-1 {
		v.pendingPing.Response <- commandResponse{Error: wrapError("output", "", ErrOutputFull, nil), Done: true}
		v.cancelPing()
		return
	}
	select {
	case v.pendingPing.Response <- response:
	default:
		// Reserve the last slot for a terminal diagnostic.
		v.cancelPing()
	}
}

func (v *Controller) cancelPing() {
	if v.pendingPing == nil {
		return
	}
	if v.pendingPing.timer != nil {
		v.pendingPing.timer.Stop()
	}
	v.pendingPing = nil
}

// Close ends controller access and wakes sessions, retaining the borrowed stack.
// Call on the owner before releasing the endpoint. It is idempotent.
func (v *Controller) Close() {
	v.cancelPing()
	select {
	case <-v.done:
	default:
		close(v.done)
	}
}
