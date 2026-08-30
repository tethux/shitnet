module;

#include <shitnet/macros.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <expected>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <span>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <unistd.h>
#include <utility>

export module shitnet.tap;

export struct TapError {
    int code;
    std::string message;
};

export class Tap {
  public:
    Tap(const Tap &) = delete;
    fn operator=(const Tap &)->Tap & = delete;

    Tap(Tap &&other) noexcept : fd_(other.fd_), name_(std::move(other.name_)) {
        other.fd_ = -1;
    }

    fn operator=(Tap &&other) noexcept -> Tap & {
        if (this == &other)
            return *this;

        if (fd_ >= 0)
            ::close(fd_);

        fd_ = other.fd_;
        name_ = std::move(other.name_);
        other.fd_ = -1;
        return *this;
    }

    ~Tap() {
        if (fd_ >= 0)
            ::close(fd_);
    }

    [[nodiscard]]
    static fn open(std::string_view requested_name = "shitnet0")
        -> std::expected<Tap, TapError> {
        const let fd = ::open("/dev/net/tun", O_RDWR);
        if (fd < 0)
            return std::unexpected(make_error("open /dev/net/tun"));

        ifreq ifr{};
        ifr.ifr_flags = IFF_TAP | IFF_NO_PI;

        const let count = std::min(requested_name.size(),
                                   static_cast<std::size_t>(IFNAMSIZ - 1));
        std::memcpy(ifr.ifr_name, requested_name.data(), count);

        if (::ioctl(fd, TUNSETIFF, &ifr) < 0) {
            const let error = make_error("ioctl TUNSETIFF");
            ::close(fd);
            return std::unexpected(error);
        }

        return Tap{fd, ifr.ifr_name};
    }

    [[nodiscard]]
    fn name() const -> const std::string & {
        return name_;
    }

    fn read(std::span<std::byte> buffer)
        -> std::expected<std::size_t, TapError> {
        for (;;) {
            const let result = ::read(fd_, buffer.data(), buffer.size());
            if (result >= 0)
                return static_cast<std::size_t>(result);

            if (errno == EINTR)
                continue;

            return std::unexpected(make_error("read"));
        }
    }

    fn write(std::span<const std::byte> frame)
        -> std::expected<void, TapError> {
        std::size_t written = 0;

        while (written < frame.size()) {
            const let result =
                ::write(fd_, frame.data() + written, frame.size() - written);

            if (result > 0) {
                written += static_cast<std::size_t>(result);
                continue;
            }

            if (result < 0 && errno == EINTR)
                continue;

            return std::unexpected(make_error("write"));
        }

        return {};
    }

  private:
    Tap(int fd, std::string name) : fd_(fd), name_(std::move(name)) {}

    static fn make_error(std::string_view operation) -> TapError {
        const let code = errno;

        return TapError{
            .code = code,
            .message = std::string{operation} + ": " + std::strerror(code),
        };
    }

    int fd_{-1};
    std::string name_;
};
