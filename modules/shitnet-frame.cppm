module;

#include <cstddef>
#include <span>
#include <vector>

export module shitnet.frame;

export class Frame {
  public:
    explicit Frame(std::size_t size) : bytes_(size) {}

    [[nodiscard]]
    std::span<std::byte> bytes() {
        return bytes_;
    }

    [[nodiscard]]
    std::span<const std::byte> bytes() const {
        return bytes_;
    }

  private:
    std::vector<std::byte> bytes_;
};
