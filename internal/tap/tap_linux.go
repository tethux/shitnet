package tap

import (
	"errors"
	"os"

	"golang.org/x/sys/unix"
)

const devicePath = "/dev/net/tun"

type Device struct {
	file *os.File
	name string
}

func Open(name string) (*Device, error) {
	request, requestErr := unix.NewIfreq(name)
	if requestErr != nil {
		return nil, operationError("open", name, ErrInvalidName, requestErr)
	}
	request.SetUint16(unix.IFF_TAP | unix.IFF_NO_PI)

	file, openErr := os.OpenFile(devicePath, os.O_RDWR, 0)
	if openErr != nil {
		return nil, operationError("open", devicePath, ErrOpen, openErr)
	}

	// Linux file descriptors originate as non-negative ints despite os.File exposing uintptr.
	//nolint:gosec // The descriptor is returned by open(2), whose return type is int.
	configureErr := unix.IoctlIfreq(int(file.Fd()), unix.TUNSETIFF, request)
	if configureErr != nil {
		closeErr := file.Close()
		return nil, operationError(
			"configure",
			name,
			ErrConfigure,
			errors.Join(configureErr, closeErr),
		)
	}

	return &Device{
		file: file,
		name: request.Name(),
	}, nil
}

func (d *Device) Name() string {
	if d == nil {
		return ""
	}
	return d.name
}

func (d *Device) Read(buffer []byte) (int, error) {
	if len(buffer) == 0 {
		return 0, operationError("read", d.Name(), ErrEmptyBuffer, nil)
	}
	if d == nil || d.file == nil {
		return 0, operationError("read", d.Name(), ErrRead, os.ErrClosed)
	}

	read, readErr := d.file.Read(buffer)
	if readErr != nil {
		return 0, operationError("read", d.name, ErrRead, readErr)
	}
	return read, nil
}

func (d *Device) Write(frame []byte) error {
	if len(frame) == 0 {
		return operationError("write", d.Name(), ErrEmptyFrame, nil)
	}
	if d == nil || d.file == nil {
		return operationError("write", d.Name(), ErrWrite, os.ErrClosed)
	}

	written, writeErr := d.file.Write(frame)
	if writeErr != nil {
		return operationError("write", d.name, ErrWrite, writeErr)
	}
	if written != len(frame) {
		return operationError("write", d.name, ErrWrite, os.ErrInvalid)
	}
	return nil
}

func (d *Device) Close() error {
	if d == nil || d.file == nil {
		return nil
	}

	file := d.file
	d.file = nil
	if closeErr := file.Close(); closeErr != nil {
		return operationError("close", d.name, ErrClose, closeErr)
	}
	return nil
}
