module;

#include <shitnet/macros.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>

export module shitnet.ipv4;

export import shitnet.address;

export enum class IpProtocol : std::uint8_t {
    icmp = 1,
    tcp = 6,
    udp = 17,
};

export class IPv4PacketView {
  public:
    explicit IPv4PacketView(std::span<const std::byte> bytes) : bytes_(bytes) {}

    [[nodiscard]]
    fn version() const -> std::uint8_t {
        return std::to_integer<std::uint8_t>(bytes_[0]) >> 4;
    }

    [[nodiscard]]
    fn headerLength() const -> std::size_t {
        const let ihl = std::to_integer<std::uint8_t>(bytes_[0]) & 0x0f;

        return static_cast<std::size_t>(ihl) * 4;
    }

    [[nodiscard]]
    fn totalLength() const -> std::uint16_t {
        const let hi = std::to_integer<std::uint16_t>(bytes_[2]);
        const let lo = std::to_integer<std::uint16_t>(bytes_[3]);

        return static_cast<std::uint16_t>((hi << 8) | lo);
    }

    [[nodiscard]]
    fn protocol() const -> IpProtocol {
        return static_cast<IpProtocol>(
            std::to_integer<std::uint8_t>(bytes_[9]));
    }

    [[nodiscard]]
    fn source() const -> IPv4Address {
        IPv4Address result{};

        for (std::size_t i = 0; i < 4; ++i)
            result.bytes[i] = bytes_[12 + i];

        return result;
    }

    [[nodiscard]]
    fn destination() const -> IPv4Address {
        IPv4Address result{};

        for (std::size_t i = 0; i < 4; ++i)
            result.bytes[i] = bytes_[16 + i];

        return result;
    }

    [[nodiscard]]
    fn payload() const -> std::span<const std::byte> {
        return bytes_.subspan(headerLength(), totalLength() - headerLength());
    }

  private:
    std::span<const std::byte> bytes_;
};

export {

    struct IPv4Icmp {
        IPv4PacketView packet;
    };

    struct IPv4Tcp {
        IPv4PacketView packet;
    };

    struct IPv4Udp {
        IPv4PacketView packet;
    };

    struct UnsupportedIpVersion {
        std::uint8_t version;
    };

    struct UnsupportedIpv4HeaderLength {
        std::size_t length;
    };

    struct InvalidIpv4Length {};

    struct UnknownIpProtocol {
        std::uint8_t protocol;
    };

    using IPv4Packet =
        std::variant<IPv4Icmp, IPv4Tcp, IPv4Udp, UnsupportedIpVersion,
                     UnsupportedIpv4HeaderLength, InvalidIpv4Length,
                     UnknownIpProtocol>;

    fn classifyIPv4(IPv4PacketView packet, std::size_t available)
        -> IPv4Packet {
        if (packet.version() != 4) {
            return UnsupportedIpVersion{
                .version = packet.version(),
            };
        }

        if (packet.headerLength() != 20) {
            return UnsupportedIpv4HeaderLength{
                .length = packet.headerLength(),
            };
        }

        if (packet.totalLength() < packet.headerLength() ||
            packet.totalLength() > available) {
            return InvalidIpv4Length{};
        }

        switch (packet.protocol()) {
        case IpProtocol::icmp:
            return IPv4Icmp{
                .packet = packet,
            };

        case IpProtocol::tcp:
            return IPv4Tcp{
                .packet = packet,
            };

        case IpProtocol::udp:
            return IPv4Udp{
                .packet = packet,
            };
        }

        return UnknownIpProtocol{
            .protocol = static_cast<std::uint8_t>(packet.protocol()),
        };
    }
}
