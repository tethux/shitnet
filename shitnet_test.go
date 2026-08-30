package shitnet

import (
	"net/netip"
	"testing"
)

func TestICMPEchoRequestAfterARPLearning(t *testing.T) {
	t.Parallel()

	stack, newErr := New(Config{
		MAC: MAC{0x02, 0x00, 0x00, 0x00, 0x00, 0x02},
		IP:  netip.MustParseAddr("10.0.0.2"),
	})
	if newErr != nil {
		t.Fatalf("create stack: %v", newErr)
	}
	t.Cleanup(stack.Close)

	peerIP := netip.MustParseAddr("10.0.0.1")
	peerMAC := MAC{0x02, 0x00, 0x00, 0x00, 0x00, 0x01}
	reply := make([]byte, 42)
	copy(reply[6:12], peerMAC[:])
	copy(reply[12:22], []byte{0x08, 0x06, 0x00, 0x01, 0x08, 0x00, 6, 4, 0, 2})
	peerAddress := peerIP.As4()
	copy(reply[22:28], peerMAC[:])
	copy(reply[28:32], peerAddress[:])

	receiveErr := stack.Receive(reply)
	if receiveErr != nil {
		t.Fatalf("receive ARP reply: %v", receiveErr)
	}

	lookup, lookupErr := stack.LookupARP(peerIP)
	if lookupErr != nil {
		t.Fatalf("lookup ARP entry: %v", lookupErr)
	}
	if !lookup.Found || lookup.MAC != peerMAC {
		t.Fatalf("unexpected ARP lookup: %+v", lookup)
	}
	entries, entriesErr := stack.ARPEntries()
	if entriesErr != nil {
		t.Fatalf("list ARP entries: %v", entriesErr)
	}
	if len(entries) != 1 || entries[0].IP != peerIP || entries[0].MAC != peerMAC {
		t.Fatalf("unexpected ARP entries: %+v", entries)
	}

	eventPoll, eventErr := stack.PollEvent()
	if eventErr != nil {
		t.Fatalf("poll ARP event: %v", eventErr)
	}
	if !eventPoll.Available || eventPoll.Event.ARP == nil {
		t.Fatalf("missing ARP event: %+v", eventPoll)
	}
	if eventPoll.Event.ARP.IP != peerIP || eventPoll.Event.ARP.MAC != peerMAC {
		t.Fatalf("unexpected ARP event: %+v", eventPoll.Event.ARP)
	}

	queued, requestErr := stack.ICMPEchoRequest(peerIP, 0x1234, 7, []byte("ping"))
	if requestErr != nil {
		t.Fatalf("queue ICMP echo request: %v", requestErr)
	}
	if queued != QueueQueued {
		t.Fatalf("unexpected queue result: %v", queued)
	}

	transmit, transmitErr := stack.PollTX()
	if transmitErr != nil {
		t.Fatalf("poll transmit queue: %v", transmitErr)
	}
	if !transmit.Available || len(transmit.Frame) != 46 {
		t.Fatalf("unexpected transmit result: %+v", transmit)
	}
	if transmit.Frame[34] != 8 || transmit.Frame[38] != 0x12 || transmit.Frame[39] != 0x34 {
		t.Fatalf("unexpected ICMP echo frame: %x", transmit.Frame)
	}

	echoReply := make([]byte, 46)
	copy(echoReply[12:14], []byte{0x08, 0x00})
	copy(echoReply[14:18], []byte{0x45, 0x00, 0x00, 32})
	echoReply[23] = 1
	copy(echoReply[26:30], peerAddress[:])
	stackAddress := netip.MustParseAddr("10.0.0.2").As4()
	copy(echoReply[30:34], stackAddress[:])
	copy(echoReply[34:42], []byte{0, 0, 0, 0, 0x12, 0x34, 0, 7})
	copy(echoReply[42:], "pong")

	replyErr := stack.Receive(echoReply)
	if replyErr != nil {
		t.Fatalf("receive ICMP echo reply: %v", replyErr)
	}
	echoEvent, echoEventErr := stack.PollEvent()
	if echoEventErr != nil {
		t.Fatalf("poll ICMP event: %v", echoEventErr)
	}
	if !echoEvent.Available || echoEvent.Event.ICMP == nil {
		t.Fatalf("missing ICMP event: %+v", echoEvent)
	}
	if echoEvent.Event.Type != EventICMPEchoReply ||
		echoEvent.Event.ICMP.Source != peerIP ||
		echoEvent.Event.ICMP.Identifier != 0x1234 ||
		echoEvent.Event.ICMP.Sequence != 7 ||
		string(echoEvent.Event.ICMP.Payload) != "pong" {
		t.Fatalf("unexpected ICMP event: %+v", echoEvent.Event)
	}
}
