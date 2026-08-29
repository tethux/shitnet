module;

#include <shitnet/macros.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>

export module shitnet.arp;

export import shitnet.address;

export enum class ArpOperation : std::uint16_t {
    request = 1,
    reply = 2,
};

export enum class ArpHardwareType : std::uint16_t {
    ethernet = 1,
};

export enum class ArpProtocolType : std::uint16_t {
    ipv4 = 0x0800,
};

export struct ArpEntry {
    IPv4Address ip;
    MacAddress mac;
};

export class ArpPacketView {
  public:
    explicit ArpPacketView(std::span<const std::byte> bytes) : bytes_(bytes) {}

    [[nodiscard]]
    fn hardwareType() const -> ArpHardwareType {
        const let hi = std::to_integer<std::uint16_t>(bytes_[0]);
        const let lo = std::to_integer<std::uint16_t>(bytes_[1]);

        return static_cast<ArpHardwareType>((hi << 8) | lo);
    }

    [[nodiscard]]
    fn protocolType() const -> ArpProtocolType {
        const let hi = std::to_integer<std::uint16_t>(bytes_[2]);
        const let lo = std::to_integer<std::uint16_t>(bytes_[3]);

        return static_cast<ArpProtocolType>((hi << 8) | lo);
    }

    [[nodiscard]]
    fn hardwareLength() const -> std::uint8_t {
        return std::to_integer<std::uint8_t>(bytes_[4]);
    }

    [[nodiscard]]
    fn protocolLength() const -> std::uint8_t {
        return std::to_integer<std::uint8_t>(bytes_[5]);
    }

    [[nodiscard]]
    fn rawOperation() const -> std::uint16_t {
        const let hi = std::to_integer<std::uint16_t>(bytes_[6]);
        const let lo = std::to_integer<std::uint16_t>(bytes_[7]);

        return static_cast<std::uint16_t>((hi << 8) | lo);
    }

    [[nodiscard]]
    fn operation() const -> ArpOperation {
        return static_cast<ArpOperation>(rawOperation());
    }

    [[nodiscard]]
    fn senderMac() const -> MacAddress {
        MacAddress result{};

        for (std::size_t i = 0; i < 6; ++i)
            result.bytes[i] = bytes_[8 + i];

        return result;
    }

    [[nodiscard]]
    fn senderIp() const -> IPv4Address {
        IPv4Address result{};

        for (std::size_t i = 0; i < 4; ++i)
            result.bytes[i] = bytes_[14 + i];

        return result;
    }

    [[nodiscard]]
    fn targetMac() const -> MacAddress {
        MacAddress result{};

        for (std::size_t i = 0; i < 6; ++i)
            result.bytes[i] = bytes_[18 + i];

        return result;
    }

    [[nodiscard]]
    fn targetIp() const -> IPv4Address {
        IPv4Address result{};

        for (std::size_t i = 0; i < 4; ++i)
            result.bytes[i] = bytes_[24 + i];

        return result;
    }

  private:
    std::span<const std::byte> bytes_;
};

export {

    struct ArpRequest {
        ArpPacketView packet;
    };

    struct ArpReply {
        ArpPacketView packet;
    };

    struct UnsupportedArpHardware {
        ArpHardwareType hardware;
    };

    struct UnsupportedArpProtocol {
        ArpProtocolType protocol;
    };

    struct InvalidArpAddressLengths {
        std::uint8_t hardwareLength;
        std::uint8_t protocolLength;
    };

    struct UnknownArpOperation {
        std::uint16_t operation;
    };

    using ArpPacket =
        std::variant<ArpRequest, ArpReply, UnsupportedArpHardware,
                     UnsupportedArpProtocol, InvalidArpAddressLengths,
                     UnknownArpOperation>;

    fn classifyArp(ArpPacketView packet) -> ArpPacket {
        if (packet.hardwareType() != ArpHardwareType::ethernet) {
            return UnsupportedArpHardware{
                .hardware = packet.hardwareType(),
            };
        }

        if (packet.protocolType() != ArpProtocolType::ipv4) {
            return UnsupportedArpProtocol{
                .protocol = packet.protocolType(),
            };
        }

        if (packet.hardwareLength() != 6 || packet.protocolLength() != 4) {
            return InvalidArpAddressLengths{
                .hardwareLength = packet.hardwareLength(),
                .protocolLength = packet.protocolLength(),
            };
        }

        switch (packet.rawOperation()) {
        case static_cast<std::uint16_t>(ArpOperation::request):
            return ArpRequest{
                .packet = packet,
            };

        case static_cast<std::uint16_t>(ArpOperation::reply):
            return ArpReply{
                .packet = packet,
            };

        default:
            return UnknownArpOperation{
                .operation = packet.rawOperation(),
            };
        }
    }
}
