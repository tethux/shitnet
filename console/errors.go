package console

import (
	"errors"
	"strings"
)

var (
	// ErrClosed reports use after the owner closes console access.
	ErrClosed = errors.New("console closed")
	// ErrOutputFull reports a session that cannot keep up with ping output.
	ErrOutputFull = errors.New("console output buffer full")
	// ErrInvalidIPv4 reports a command requiring an IPv4 address.
	ErrInvalidIPv4 = errors.New("IPv4 address required")
	// ErrInvalidCount reports a nonpositive or oversized ping count.
	ErrInvalidCount = errors.New("positive ping count required")
	// ErrPingPending reports another command already using ping.
	ErrPingPending = errors.New("another ping is pending")
	// ErrUnresolved reports an unresolved ping destination.
	ErrUnresolved = errors.New("destination unresolved")
	// ErrUnknownCommand reports an unsupported command.
	ErrUnknownCommand = errors.New("unknown command")
	// ErrUnknownRequest reports an unsupported owner request.
	ErrUnknownRequest = errors.New("unknown request")
	// ErrUsage reports invalid command arguments.
	ErrUsage = errors.New("invalid command usage")
)

// Operation preserves the command, target, category, and underlying cause.
type Operation struct {
	Op     string
	Target string
	Kind   error
	Cause  error
}

// Error formats a command diagnostic.
func (e *Operation) Error() string {
	parts := make([]string, 0, 4)
	for _, part := range []string{e.Op, e.Target, errorText(e.Kind), errorText(e.Cause)} {
		if part != "" {
			parts = append(parts, part)
		}
	}
	return strings.Join(parts, ": ")
}

// Unwrap exposes both category and cause.
func (e *Operation) Unwrap() []error {
	result := make([]error, 0, 2)
	if e.Kind != nil {
		result = append(result, e.Kind)
	}
	if e.Cause != nil {
		result = append(result, e.Cause)
	}
	return result
}

func wrapError(operation, target string, kind, cause error) error {
	return &Operation{
		Op:     operation,
		Target: target,
		Kind:   kind,
		Cause:  cause,
	}
}

func errorText(value error) string {
	if value == nil {
		return ""
	}
	return value.Error()
}
