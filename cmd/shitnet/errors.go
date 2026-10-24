package main

import (
	"errors"
	"strings"
)

var errCreateVPC = errors.New("create VPC")

type operationError struct {
	op     string
	target string
	kind   error
	cause  error
}

func (e *operationError) Error() string {
	parts := make([]string, 0, 4)
	for _, part := range []string{e.op, e.target, errorText(e.kind), errorText(e.cause)} {
		if part != "" {
			parts = append(parts, part)
		}
	}
	return strings.Join(parts, ": ")
}

func (e *operationError) Unwrap() []error {
	result := make([]error, 0, 2)
	if e.kind != nil {
		result = append(result, e.kind)
	}
	if e.cause != nil {
		result = append(result, e.cause)
	}
	return result
}

func wrapError(operation, target string, kind, cause error) error {
	return &operationError{
		op:     operation,
		target: target,
		kind:   kind,
		cause:  cause,
	}
}

func errorText(value error) string {
	if value == nil {
		return ""
	}
	return value.Error()
}
