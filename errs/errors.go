package errs

import "errors"

var (
	// ErrInvalidArgument reports an invalid value at the native boundary.
	ErrInvalidArgument = errors.New("invalid argument")
	// ErrInvalidPacket reports a malformed packet.
	ErrInvalidPacket = errors.New("invalid packet")
	// ErrBufferTooSmall reports insufficient output capacity.
	ErrBufferTooSmall = errors.New("buffer too small")
	// ErrInternal reports an unexpected native failure.
	ErrInternal = errors.New("internal error")
	// ErrIPv4Required reports an address outside the supported IPv4 family.
	ErrIPv4Required = errors.New("IPv4 required")
	// ErrCreateFailed reports failure to allocate a native stack.
	ErrCreateFailed = errors.New("create failed")
	// ErrClosed reports use of a closed stack.
	ErrClosed = errors.New("closed")
)

// Operation adds operation context while preserving category and cause.
type Operation struct {
	Op    string
	Kind  error
	Cause error
}

// Error returns a stable human-readable diagnostic.
func (e *Operation) Error() string {
	message := "shitnet: " + e.Op + ": " + e.Kind.Error()
	if e.Cause != nil {
		message += ": " + e.Cause.Error()
	}
	return message
}

// Unwrap exposes both the category and underlying cause to errors.Is and errors.As.
func (e *Operation) Unwrap() []error {
	if e.Cause == nil {
		return []error{e.Kind}
	}

	return []error{e.Kind, e.Cause}
}
