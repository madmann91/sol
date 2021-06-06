#ifndef SOL_IMAGE_H
#define SOL_IMAGE_H

#include <optional>
#include <memory>
#include <vector>
#include <string_view>
#include <cassert>

#include "sol/color.h"

namespace sol {

/// Image represented as a list of floating-point channels, each having the same width and height.
/// An image can have an arbitrary number of channels, but some image formats only support 3 or 4 channels.
/// By convention, the top-left corner of the image is at (0, 0).
struct Image {
    template <size_t Bits> using Word = std::make_unsigned_t<proto::SizedIntegerType<Bits>>;

public:
    /// File formats for image `load()` and `save()` functions.
    enum class Format {
        Auto, Png, Jpeg, Tiff, Exr
    };

    using Channel = std::unique_ptr<float[]>;
    using Channels = std::vector<Channel>;

    Image() = default;
    Image(const Image&) = delete;
    Image(Image&&) = default;
    Image(size_t width, size_t height, size_t channel_count)
        : width_(width), height_(height)
    {
        for (size_t i = 0; i < channel_count; ++i)
            channels_[i] = std::make_unique<float[]>(width * height);
    }

    Image& operator = (const Image&) = delete;
    Image& operator = (Image&&) = default;

    size_t width() const { return width_; }
    size_t height() const { return height_; }
    size_t channel_count() const { return channels_.size(); }

    RgbColor rgb_at(size_t x, size_t y) const {
        assert(channel_count() == 3);
        auto i = y * width_ + x;
        return RgbColor(channels_[0][i], channels_[1][i], channels_[2][i]);
    }

    void accumulate(size_t x, size_t y, const RgbColor& color) {
        assert(channel_count() == 3);
        auto i = y * width_ + x;
        channels_[0][i] += color.r;
        channels_[1][i] += color.g;
        channels_[2][i] += color.b;
    }

    Channel& channel(size_t i) { return channels_[i]; }
    const Channel& channel(size_t i) const { return channels_[i]; }

    /// Saves the image to a file, using the given format.
    /// If format is `Auto`, the function uses the EXR format for the image.
    bool save(const std::string_view& path, Format format = Format::Auto) const;

    /// Loads an image from a file, using the given format hint.
    /// If the format is `Auto`, the function will try to auto-detect which format the image is in.
    static std::optional<Image> load(const std::string_view& path, Format format = Format::Auto);

    /// Converts an unsigned integer type to an image component.
    template <size_t Bits>
    static proto_always_inline float word_to_component(Word<Bits> word) { return word * (1.0f / 255.0f); }

    /// Converts a component to an unsigned integer type.
    template <size_t Bits>
    static proto_always_inline Word<Bits> component_to_word(float f) {
        static_assert(sizeof(Word<Bits>) < sizeof(uint32_t));
        return std::min(
            uint32_t{std::numeric_limits<Word<Bits>>::max()},
            static_cast<uint32_t>(std::max(f * (static_cast<float>(std::numeric_limits<Word<Bits>>::max()) + 1.0f), 0.0f)));
    }

private:
    size_t width_ = 0;
    size_t height_ = 0;
    std::vector<std::unique_ptr<float[]>> channels_;
};

} // namespace sol

#endif
