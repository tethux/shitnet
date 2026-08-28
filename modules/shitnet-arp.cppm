module;

#include <cstddef>
#include <cstdint>
#include <span>

export module shitnet.arp;

export import shitnet.address;

export class ArpPacketView {
  public:
    explicit ArpPacketView(std::span<const std::byte> bytes) : bytes_(bytes) {}

    [[nodiscard]]
    std::uint16_t operation() const {
        const auto hi = std::to_integer<std::uint16_t>(bytes_[6]);
        const auto lo = std::to_integer<std::uint16_t>(bytes_[7]);
        return static_cast<std::uint16_t>((hi << 8) | lo);
    }

    [[nodiscard]]
    MacAddress senderMac() const {
        MacAddress result{};
        for (std::size_t i = 0; i < 6; ++i)
            result.bytes[i] = bytes_[8 + i];
        return result;
    }

    [[nodiscard]]
    IPv4Address senderIp() const {
        IPv4Address result{};
        for (std::size_t i = 0; i < 4; ++i)
            result.bytes[i] = bytes_[14 + i];
        return result;
    }

  private:
    std::span<const std::byte> bytes_;
};
