module;

#include <cstddef>
#include <cstdint>
#include <shitnet/macros.h>
#include <span>
#include <variant>

export module shitnet.icmp;

export enum class IcmpType : std::uint8_t {
    echoReply = 0,
    echoRequest = 8,
};

export class IcmpPacketView {
  public:
    explicit IcmpPacketView(std::span<const std::byte> bytes) : bytes_(bytes) {}

    [[nodiscard]]
    fn rawType() const -> std::uint8_t {
        return std::to_integer<std::uint8_t>(bytes_[0]);
    }

    [[nodiscard]]
    fn type() const -> IcmpType {
        return static_cast<IcmpType>(rawType());
    }

    [[nodiscard]]
    fn code() const -> std::uint8_t {
        return std::to_integer<std::uint8_t>(bytes_[1]);
    }

    [[nodiscard]]
    fn identifier() const -> std::uint16_t {
        const let hi = std::to_integer<std::uint16_t>(bytes_[4]);
        const let lo = std::to_integer<std::uint16_t>(bytes_[5]);

        return static_cast<std::uint16_t>((hi << 8) | lo);
    }

    [[nodiscard]]
    fn sequence() const -> std::uint16_t {
        const let hi = std::to_integer<std::uint16_t>(bytes_[6]);
        const let lo = std::to_integer<std::uint16_t>(bytes_[7]);

        return static_cast<std::uint16_t>((hi << 8) | lo);
    }

    [[nodiscard]]
    fn payload() const -> std::span<const std::byte> {
        return bytes_.subspan(8);
    }

  private:
    std::span<const std::byte> bytes_;
};

export {

    struct IcmpEchoRequest {
        IcmpPacketView packet;
    };

    struct IcmpEchoReply {
        IcmpPacketView packet;
    };

    struct UnsupportedIcmpType {
        std::uint8_t type;
    };

    struct UnsupportedIcmpCode {
        std::uint8_t code;
    };

    using IcmpPacket = std::variant<IcmpEchoRequest, IcmpEchoReply,
                                    UnsupportedIcmpType, UnsupportedIcmpCode>;

    fn classifyIcmp(IcmpPacketView packet) -> IcmpPacket {
        switch (packet.type()) {
        case IcmpType::echoRequest:
            if (packet.code() != 0) {
                return UnsupportedIcmpCode{
                    .code = packet.code(),
                };
            }

            return IcmpEchoRequest{
                .packet = packet,
            };

        case IcmpType::echoReply:
            if (packet.code() != 0) {
                return UnsupportedIcmpCode{
                    .code = packet.code(),
                };
            }

            return IcmpEchoReply{
                .packet = packet,
            };
        }

        return UnsupportedIcmpType{
            .type = packet.rawType(),
        };
    }
}
