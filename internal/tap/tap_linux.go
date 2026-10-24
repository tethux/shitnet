package tap

import (
	"errors"
	"os"
	"sync"

	"golang.org/x/sys/unix"
)

const devicePath = "/dev/net/tun"

type Device struct {
	file      *os.File
	name      string
	closeOnce sync.Once
	closeErr  error
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

	raw, rawErr := file.SyscallConn()
	if rawErr != nil {
		return nil, operationError("configure", name, ErrConfigure, errors.Join(rawErr, file.Close()))
	}
	var ioctlErr error
	controlErr := raw.Control(func(fd uintptr) {
		// Preserve Go's nonblocking descriptor so Close interrupts a pending Read.
		//nolint:gosec // The descriptor is supplied by os.File's runtime-managed syscall connection.
		ioctlErr = unix.IoctlIfreq(int(fd), unix.TUNSETIFF, request)
	})
	configureErr := errors.Join(controlErr, ioctlErr)
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

	d.closeOnce.Do(func() {
		if closeErr := d.file.Close(); closeErr != nil {
			d.closeErr = operationError("close", d.name, ErrClose, closeErr)
		}
	})
	return d.closeErr
}
