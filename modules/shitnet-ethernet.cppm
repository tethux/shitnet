module;

#include <cstddef>
#include <cstdint>
#include <shitnet/macros.h>
#include <span>

export module shitnet.ethernet;

export import shitnet.address;

export enum class EtherType : std::uint16_t {
    ipv4 = 0x800,
    arp = 0x806,
    ipv6 = 0x86dd,
};

export class EthernetFrameView {
  public:
    explicit EthernetFrameView(std::span<const std::byte> bytes)
        : bytes_(bytes) {}

    [[nodiscard]]
    fn destination() const -> MacAddress {
        MacAddress result{};
        for (std::size_t i = 0; i < 6; ++i)
            result.bytes[i] = bytes_[i];
        return result;
    }

    [[nodiscard]]
    fn source() const -> MacAddress {
        MacAddress result{};
        for (std::size_t i = 0; i < 6; ++i)
            result.bytes[i] = bytes_[6 + i];
        return result;
    }

    [[nodiscard]]
    fn etherType() const -> EtherType {
        const auto hi = std::to_integer<std::uint16_t>(bytes_[12]);
        const auto lo = std::to_integer<std::uint16_t>(bytes_[13]);
        return static_cast<EtherType>((hi << 8) | lo);
    }

    [[nodiscard]]
    fn payload() const -> std::span<const std::byte> {
        return bytes_.subspan(14);
    }

  private:
    std::span<const std::byte> bytes_;
};
