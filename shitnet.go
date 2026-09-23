// Package shitnet provides Go bindings for the shitnet userspace network stack.
// It requires the installed shitnet native package at build time.
package shitnet

import (
	"net/netip"

	binding "github.com/tethux/shitnet/internal/binding"
)

// MAC is a six-byte Ethernet address.
type MAC [6]byte

// String formats the MAC address in colon-separated hexadecimal notation.
func (m MAC) String() string { return binding.MAC(m).String() }

// Config contains the addresses assigned to a stack instance.
type Config struct {
	MAC MAC
	IP  netip.Addr
}

// Shitnet owns one native userspace network stack.
type Shitnet struct{ handle *binding.Shitnet }

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

// EventType identifies a stack event.
type EventType uint8

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

const (
	// QueueUnresolved indicates that no destination MAC is known.
	QueueUnresolved QueueResult = QueueResult(binding.QueueUnresolved)
	// QueueQueued indicates that a frame was queued for transmission.
	QueueQueued QueueResult = QueueResult(binding.QueueQueued)
	// EventNone indicates that no event is available.
	EventNone EventType = EventType(binding.EventNone)
	// EventARPLearned reports a learned or updated ARP entry.
	EventARPLearned EventType = EventType(binding.EventARPLearned)
	// EventICMPEchoRequest reports a received ICMP echo request.
	EventICMPEchoRequest EventType = EventType(binding.EventICMPEchoRequest)
	// EventICMPEchoReply reports a received ICMP echo reply.
	EventICMPEchoReply EventType = EventType(binding.EventICMPEchoReply)
)

// New creates a stack with explicit MAC and IPv4 addresses.
func New(config Config) (*Shitnet, error) {
	stack, err := binding.New(binding.Config{MAC: binding.MAC(config.MAC), IP: config.IP})
	if err != nil {
		return nil, err
	}
	return &Shitnet{handle: stack}, nil
}

// Close releases the native stack. It is safe to call more than once.
func (s *Shitnet) Close() { s.native().Close() }

// ARPRequest queues one broadcast ARP request for ip.
func (s *Shitnet) ARPRequest(ip netip.Addr) error { return s.native().ARPRequest(ip) }

// Receive passes one Ethernet frame into the stack.
func (s *Shitnet) Receive(frame []byte) error { return s.native().Receive(frame) }

// LookupARP looks up an IPv4 address in the learned ARP table.
func (s *Shitnet) LookupARP(ip netip.Addr) (ARPLookup, error) {
	lookup, err := s.native().LookupARP(ip)
	return ARPLookup{MAC: MAC(lookup.MAC), Found: lookup.Found}, err
}

// ARPEntries returns a snapshot of the native ARP table.
func (s *Shitnet) ARPEntries() ([]ARPEntry, error) {
	entries, err := s.native().ARPEntries()
	if err != nil {
		return nil, err
	}
	result := make([]ARPEntry, len(entries))
	for i, entry := range entries {
		result[i] = ARPEntry{IP: entry.IP, MAC: MAC(entry.MAC)}
	}
	return result, nil
}

// ICMPEchoRequest queues an echo request when the destination MAC is resolved.
func (s *Shitnet) ICMPEchoRequest(ip netip.Addr, identifier, sequence uint16, payload []byte) (QueueResult, error) {
	result, err := s.native().ICMPEchoRequest(ip, identifier, sequence, payload)
	return QueueResult(result), err
}

// PollEvent returns the next owned copy of a stack event.
func (s *Shitnet) PollEvent() (EventPoll, error) {
	poll, err := s.native().PollEvent()
	if err != nil {
		return EventPoll{}, err
	}
	event := Event{Type: EventType(poll.Event.Type)}
	if poll.Event.ARP != nil {
		event.ARP = &ARPEvent{IP: poll.Event.ARP.IP, MAC: MAC(poll.Event.ARP.MAC)}
	}
	if poll.Event.ICMP != nil {
		event.ICMP = &ICMPEchoEvent{
			Source:     poll.Event.ICMP.Source,
			Identifier: poll.Event.ICMP.Identifier,
			Sequence:   poll.Event.ICMP.Sequence,
			Payload:    poll.Event.ICMP.Payload,
		}
	}
	return EventPoll{Event: event, Available: poll.Available}, nil
}

// TXSize returns the number of frames waiting for transmission.
func (s *Shitnet) TXSize() (int, error) { return s.native().TXSize() }

// PollTX returns the next queued frame when one is available.
func (s *Shitnet) PollTX() (TXPoll, error) {
	poll, err := s.native().PollTX()
	return TXPoll(poll), err
}

func (s *Shitnet) native() *binding.Shitnet {
	if s == nil {
		return nil
	}
	return s.handle
}
