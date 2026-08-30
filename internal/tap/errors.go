package tap

import "errors"

var (
	ErrInvalidName = errors.New("invalid TAP name")
	ErrOpen        = errors.New("open TAP")
	ErrConfigure   = errors.New("configure TAP")
	ErrRead        = errors.New("read TAP")
	ErrWrite       = errors.New("write TAP")
	ErrClose       = errors.New("close TAP")
	ErrEmptyBuffer = errors.New("empty TAP read buffer")
	ErrEmptyFrame  = errors.New("empty TAP frame")
)

type Operation struct {
	Op     string
	Target string
	Kind   error
	Cause  error
}

func (e *Operation) Error() string {
	message := "tap: " + e.Op
	if e.Target != "" {
		message += " " + e.Target
	}
	if e.Kind != nil {
		message += ": " + e.Kind.Error()
	}
	if e.Cause != nil {
		message += ": " + e.Cause.Error()
	}
	return message
}

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

func operationError(op, target string, kind, cause error) error {
	return &Operation{
		Op:     op,
		Target: target,
		Kind:   kind,
		Cause:  cause,
	}
}
