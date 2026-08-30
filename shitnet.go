package shitnet

/*
#cgo CFLAGS: -I${SRCDIR}/include
#cgo LDFLAGS: -L${SRCDIR}/build/linux/x86_64/debug -lshitnet -lstdc++

#include <shitnet/shitnet.h>

static inline shitnet_arp_event shitnet_event_arp(shitnet_event *event) {
	return event->data.arp;
}

static inline shitnet_icmp_echo_event shitnet_event_icmp(shitnet_event *event) {
	return event->data.icmp_echo;
}

static inline void shitnet_copy_icmp_payload(
	shitnet_icmp_echo_event *event,
	uint8_t *destination
) {
	for (size_t index = 0; index < event->payload_len; ++index) {
		destination[index] = event->payload[index];
	}
}
*/
import "C"

import (
	"fmt"
	"net/netip"

	"github.com/tethux/shitnet/errs"
)

const maxFrameSize = 65536

// MAC is a six-byte Ethernet address.
type MAC [6]byte

func (m MAC) String() string {
	return fmt.Sprintf("%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5])
}

// Config contains the addresses assigned to a stack instance.
type Config struct {
	MAC MAC
	IP  netip.Addr
}

// Shitnet owns one native userspace network stack.
type Shitnet struct {
	ptr *C.shitnet_t
}

// TXPoll is the result of polling the transmit queue.
type TXPoll struct {
	Frame     []byte
	Available bool
}

// ARPLookup is the result of an ARP table lookup.
type ARPLookup struct {
	MAC   MAC
	Found bool
}

// ARPEntry contains one address learned by the native stack.
type ARPEntry struct {
	IP  netip.Addr
	MAC MAC
}

// QueueResult describes whether a frame was queued or needs address resolution.
type QueueResult uint8

const (
	// QueueUnresolved indicates that no destination MAC is known.
	QueueUnresolved QueueResult = iota
	// QueueQueued indicates that a frame was queued for transmission.
	QueueQueued
)

// EventType identifies a stack event.
type EventType uint8

const (
	// EventNone indicates that no event is available.
	EventNone EventType = iota
	// EventARPLearned reports a learned or updated ARP entry.
	EventARPLearned
	// EventICMPEchoRequest reports a received ICMP echo request.
	EventICMPEchoRequest
	// EventICMPEchoReply reports a received ICMP echo reply.
	EventICMPEchoReply
)

// ARPEvent reports an address learned from ARP traffic.
type ARPEvent struct {
	IP  netip.Addr
	MAC MAC
}

// ICMPEchoEvent reports a received ICMP echo packet.
type ICMPEchoEvent struct {
	Source     netip.Addr
	Identifier uint16
	Sequence   uint16
	Payload    []byte
}

// Event contains the value matching Type.
type Event struct {
	Type EventType
	ARP  *ARPEvent
	ICMP *ICMPEchoEvent
}

// EventPoll is the result of polling the event queue.
type EventPoll struct {
	Event     Event
	Available bool
}

// New creates a stack with explicit MAC and IPv4 addresses.
func New(config Config) (*Shitnet, error) {
	if !config.IP.Is4() {
		return nil, operationError("create", errs.ErrIPv4Required, nil)
	}

	var cconfig C.shitnet_config

	for i := range 6 {
		cconfig.mac[i] = C.uint8_t(config.MAC[i])
	}

	ip := config.IP.As4()

	for i := range 4 {
		cconfig.ip[i] = C.uint8_t(ip[i])
	}

	ptr := C.shitnet_create(
		&cconfig,
	)

	if ptr == nil {
		return nil, operationError("create", errs.ErrCreateFailed, nil)
	}

	return &Shitnet{
		ptr: ptr,
	}, nil
}

// Close releases the native stack. It is safe to call more than once.
func (s *Shitnet) Close() {
	if s == nil || s.ptr == nil {
		return
	}

	C.shitnet_destroy(s.ptr)
	s.ptr = nil
}

// ARPRequest queues one broadcast ARP request for ip.
func (s *Shitnet) ARPRequest(ip netip.Addr) error {
	if s == nil || s.ptr == nil {
		return operationError("ARP request", errs.ErrClosed, nil)
	}

	if !ip.Is4() {
		return operationError("ARP request", errs.ErrIPv4Required, nil)
	}

	addr := ip.As4()

	target := [4]C.uint8_t{
		C.uint8_t(addr[0]),
		C.uint8_t(addr[1]),
		C.uint8_t(addr[2]),
		C.uint8_t(addr[3]),
	}

	result := C.shitnet_arp_request(
		s.ptr,
		&target[0],
	)

	if result == C.SHITNET_QUEUE_QUEUED {
		return nil
	}

	return nativeError("ARP request", result)
}

// Receive passes one Ethernet frame into the stack.
func (s *Shitnet) Receive(frame []byte) error {
	if s == nil || s.ptr == nil {
		return operationError("receive", errs.ErrClosed, nil)
	}
	if len(frame) == 0 {
		return operationError("receive", errs.ErrInvalidArgument, nil)
	}

	result := C.shitnet_receive(s.ptr, (*C.uint8_t)(&frame[0]), C.size_t(len(frame)))
	if result != C.SHITNET_OK {
		return nativeError("receive", result)
	}
	return nil
}

// LookupARP looks up an IPv4 address in the learned ARP table.
func (s *Shitnet) LookupARP(ip netip.Addr) (ARPLookup, error) {
	if s == nil || s.ptr == nil {
		return ARPLookup{}, operationError("ARP lookup", errs.ErrClosed, nil)
	}
	if !ip.Is4() {
		return ARPLookup{}, operationError("ARP lookup", errs.ErrIPv4Required, nil)
	}

	address := ip.As4()
	target := [4]C.uint8_t{C.uint8_t(address[0]), C.uint8_t(address[1]), C.uint8_t(address[2]), C.uint8_t(address[3])}
	var mac [6]C.uint8_t
	result := C.shitnet_arp_lookup(s.ptr, &target[0], &mac[0])
	if result < 0 {
		return ARPLookup{}, nativeError("ARP lookup", result)
	}

	lookup := ARPLookup{Found: result == C.SHITNET_LOOKUP_FOUND}
	for index := range lookup.MAC {
		lookup.MAC[index] = byte(mac[index])
	}
	return lookup, nil
}

// ARPEntries returns a snapshot of the native ARP table.
func (s *Shitnet) ARPEntries() ([]ARPEntry, error) {
	if s == nil || s.ptr == nil {
		return nil, operationError("list ARP", errs.ErrClosed, nil)
	}

	count := int(C.shitnet_arp_count(s.ptr))
	entries := make([]ARPEntry, 0, count)
	for index := range count {
		var address [4]C.uint8_t
		var mac [6]C.uint8_t
		result := C.shitnet_arp_entry(
			s.ptr,
			C.size_t(index),
			&address[0],
			&mac[0],
		)
		if result < 0 {
			return nil, nativeError("list ARP", result)
		}
		if result != C.SHITNET_LOOKUP_FOUND {
			continue
		}

		ip := [4]byte{
			byte(address[0]),
			byte(address[1]),
			byte(address[2]),
			byte(address[3]),
		}
		entry := ARPEntry{IP: netip.AddrFrom4(ip)}
		for offset := range entry.MAC {
			entry.MAC[offset] = byte(mac[offset])
		}
		entries = append(entries, entry)
	}
	return entries, nil
}

// ICMPEchoRequest queues an echo request when the destination MAC is resolved.
func (s *Shitnet) ICMPEchoRequest(ip netip.Addr, identifier, sequence uint16, payload []byte) (QueueResult, error) {
	if s == nil || s.ptr == nil {
		return QueueUnresolved, operationError("ICMP echo request", errs.ErrClosed, nil)
	}
	if !ip.Is4() {
		return QueueUnresolved, operationError("ICMP echo request", errs.ErrIPv4Required, nil)
	}

	address := ip.As4()
	target := [4]C.uint8_t{C.uint8_t(address[0]), C.uint8_t(address[1]), C.uint8_t(address[2]), C.uint8_t(address[3])}
	var payloadPointer *C.uint8_t
	if len(payload) != 0 {
		payloadPointer = (*C.uint8_t)(&payload[0])
	}
	result := C.shitnet_icmp_echo_request(s.ptr, &target[0], C.uint16_t(identifier), C.uint16_t(sequence), payloadPointer, C.size_t(len(payload)))
	if result < 0 {
		return QueueUnresolved, nativeError("ICMP echo request", result)
	}
	if result == C.SHITNET_QUEUE_QUEUED {
		return QueueQueued, nil
	}
	return QueueUnresolved, nil
}

// PollEvent returns the next owned copy of a stack event.
func (s *Shitnet) PollEvent() (EventPoll, error) {
	if s == nil || s.ptr == nil {
		return EventPoll{}, operationError("poll event", errs.ErrClosed, nil)
	}

	var native C.shitnet_event
	result := C.shitnet_poll_event(s.ptr, &native)
	if result < 0 {
		return EventPoll{}, nativeError("poll event", result)
	}
	if result == 0 {
		return EventPoll{}, nil
	}

	event := Event{Type: EventType(native._type)}
	switch native._type {
	case C.SHITNET_EVENT_ARP_LEARNED:
		value := C.shitnet_event_arp(&native)
		var ip [4]byte
		for index := range ip {
			ip[index] = byte(value.ip[index])
		}
		arp := ARPEvent{IP: netip.AddrFrom4(ip)}
		for index := range arp.MAC {
			arp.MAC[index] = byte(value.mac[index])
		}
		event.ARP = &arp
	case C.SHITNET_EVENT_ICMP_ECHO_REQUEST, C.SHITNET_EVENT_ICMP_ECHO_REPLY:
		value := C.shitnet_event_icmp(&native)
		var ip [4]byte
		for index := range ip {
			ip[index] = byte(value.source_ip[index])
		}
		icmp := ICMPEchoEvent{Source: netip.AddrFrom4(ip), Identifier: uint16(value.identifier), Sequence: uint16(value.sequence)}
		icmp.Payload = make([]byte, int(value.payload_len))
		if len(icmp.Payload) != 0 {
			//nolint:gocritic // cgo expands this helper call into an expression the linter misreads.
			C.shitnet_copy_icmp_payload(&value, (*C.uint8_t)(&icmp.Payload[0]))
		}
		event.ICMP = &icmp
	}
	return EventPoll{Event: event, Available: true}, nil
}

// TXSize returns the number of frames waiting for transmission.
func (s *Shitnet) TXSize() (int, error) {
	if s == nil || s.ptr == nil {
		return 0, operationError("TX size", errs.ErrClosed, nil)
	}

	return int(
		C.shitnet_tx_size(s.ptr),
	), nil
}

// PollTX returns the next queued frame when one is available.
func (s *Shitnet) PollTX() (TXPoll, error) {
	if s == nil || s.ptr == nil {
		return TXPoll{}, operationError("poll TX", errs.ErrClosed, nil)
	}

	size, sizeErr := s.TXSize()
	if sizeErr != nil {
		return TXPoll{}, sizeErr
	}

	if size == 0 {
		return TXPoll{}, nil
	}

	buffer := make([]byte, maxFrameSize)

	var written C.size_t

	result := C.shitnet_poll_tx(
		s.ptr,
		(*C.uint8_t)(&buffer[0]),
		C.size_t(len(buffer)),
		&written,
	)

	if result < 0 {
		return TXPoll{}, nativeError("poll TX", result)
	}

	if result == 0 {
		return TXPoll{}, nil
	}

	return TXPoll{
		Frame:     buffer[:int(written)],
		Available: true,
	}, nil
}

func nativeError(operation string, result C.int) error {
	var kind error

	switch result {
	case C.SHITNET_ERR_INVALID_ARGUMENT:
		kind = errs.ErrInvalidArgument
	case C.SHITNET_ERR_INVALID_PACKET:
		kind = errs.ErrInvalidPacket
	case C.SHITNET_ERR_BUFFER_TOO_SMALL:
		kind = errs.ErrBufferTooSmall
	default:
		kind = errs.ErrInternal
	}

	return operationError(operation, kind, nil)
}

func operationError(operation string, kind, cause error) error {
	return &errs.Operation{
		Op:    operation,
		Kind:  kind,
		Cause: cause,
	}
}
