module;

#include <cstddef>
#include <shitnet/macros.h>
#include <span>
#include <vector>

export module shitnet.frame;

export class Frame {
  public:
    explicit Frame(std::size_t size) : bytes_(size) {}

    [[nodiscard]]
    fn bytes() -> std::span<std::byte> {
        return bytes_;
    }

    [[nodiscard]]
    fn bytes() const -> std::span<const std::byte> {
        return bytes_;
    }

  private:
    std::vector<std::byte> bytes_;
};
