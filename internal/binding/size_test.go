package shitnet

import (
	"errors"
	"testing"

	"github.com/tethux/shitnet/errs"
)

func TestCheckedNativeSize(t *testing.T) {
	for _, test := range []struct {
		name    string
		call    func() (int, error)
		want    int
		invalid bool
	}{
		{name: "empty", call: func() (int, error) { return checkedNativeSize("test", 0, 0) }},
		{name: "exact buffer", call: func() (int, error) { return checkedNativeSize("test", 65536, 65536) }, want: 65536},
		{name: "buffer overflow", call: func() (int, error) { return checkedNativeSize("test", 65537, 65536) }, invalid: true},
		{name: "negative bound", call: func() (int, error) { return checkedNativeSize("test", 0, -1) }, invalid: true},
	} {
		t.Run(test.name, func(t *testing.T) {
			value, callErr := test.call()
			if test.invalid {
				if !errors.Is(callErr, errs.ErrInternal) {
					t.Fatalf("unchecked length: %d, %v", value, callErr)
				}
			} else if callErr != nil || value != test.want {
				t.Fatalf("length: %d, %v", value, callErr)
			}
		})
	}
}
