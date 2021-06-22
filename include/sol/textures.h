#ifndef SOL_TEXTURES_H
#define SOL_TEXTURES_H

#include <cstddef>
#include <cmath>

#include <proto/vec.h>
#include <proto/utils.h>
#include <proto/hash.h>

#include "sol/color.h"
#include "sol/image.h"

namespace sol {

/// Available border modes for image textures.
struct BorderMode {
    enum class Tag { Clamp, Repeat, Mirror, End };
    static constexpr size_t tag_count() { return static_cast<size_t>(Tag::End); }

    struct Clamp {
        static constexpr Tag tag() { return Tag::Clamp; }
        proto::Vec2f operator () (const proto::Vec2f&) const;
    };

    struct Repeat {
        static constexpr Tag tag() { return Tag::Repeat; }
        proto::Vec2f operator () (const proto::Vec2f&) const;
    };

    struct Mirror {
        static constexpr Tag tag() { return Tag::Mirror; }
        proto::Vec2f operator () (const proto::Vec2f&) const;
    };
};

/// Available filters for an image texture.
struct ImageFilter {
    enum class Tag { Nearest, Bilinear, End };
    static constexpr size_t tag_count() { return static_cast<size_t>(Tag::End); }

    struct Nearest {
        static constexpr Tag tag() { return Tag::Nearest; }
        template <typename F>
        Color operator () (const proto::Vec2f&, size_t, size_t, F&&) const;
    };

    struct Bilinear {
        static constexpr Tag tag() { return Tag::Bilinear; }
        template <typename F>
        Color operator () (const proto::Vec2f&, size_t, size_t, F&&) const;
    };
};

/// Abstract texture that produces a floating-point value from a UV coordinate.
class Texture {
public:
    const enum class Tag {
        ConstantTexture = 0,
        ConstantColorTexture = 1,
        ImageTextureBegin = 2,
        ImageTextureEnd =
            ImageTextureBegin + BorderMode::tag_count() * ImageFilter::tag_count()
    } tag;

    Texture(Tag tag)
        : tag(tag)
    {}

    virtual ~Texture() {}

    /// Produces a value, given a particular texture coordinate.
    virtual float sample(const proto::Vec2f& uv) const = 0;

    virtual proto::fnv::Hasher& hash(proto::fnv::Hasher&) const = 0;
    virtual bool equals(const Texture&) const = 0;

protected:
    template <typename ImageFilterType, typename BorderModeType>
    static constexpr Tag make_image_texture_tag() {
        return static_cast<Tag>(
            static_cast<size_t>(Tag::ImageTextureBegin) +
            static_cast<size_t>(ImageFilterType::tag()) * BorderMode::tag_count() +
            static_cast<size_t>(BorderModeType::tag()));
    }
};

/// Abstract texture that produces a color from a UV coordinate. Can be used as a
/// regular (i.e. scalar) texture that returns the luminance of the color of each
/// sample.
class ColorTexture : public Texture {
public:
    ColorTexture(Tag tag)
        : Texture(tag)
    {}

    float sample(const proto::Vec2f& uv) const override final {
        return sample_color(uv).luminance();
    }

    virtual Color sample_color(const proto::Vec2f& uv) const = 0;
};

/// Constant texture that evaluates to the same scalar everywhere.
class ConstantTexture final : public Texture {
public:
    ConstantTexture(float constant)
        : Texture(Tag::ConstantTexture), constant_(constant)
    {}

    float sample(const proto::Vec2f&) const override { return constant_; }

    proto::fnv::Hasher& hash(proto::fnv::Hasher& hasher) const {
        return hasher.combine(tag).combine(constant_);
    }

    bool equals(const Texture& other) const override {
        return other.tag == tag && static_cast<const ConstantTexture&>(other).constant_ == constant_;
    }

private:
    float constant_;
};

/// Constant texture that evaluates to the same color everywhere.
class ConstantColorTexture final : public ColorTexture {
public:
    ConstantColorTexture(const Color& color)
        : ColorTexture(Tag::ConstantColorTexture), color_(color)
    {}

    Color sample_color(const proto::Vec2f&) const override { return color_; }

    proto::fnv::Hasher& hash(proto::fnv::Hasher& hasher) const {
        return color_.hash(hasher.combine(tag));
    }

    bool equals(const Texture& other) const override {
        return other.tag == tag && static_cast<const ConstantColorTexture&>(other).color_ == color_;
    }

private:
    Color color_;
};

template <typename T> struct ConstantTextureSelector {};
template <> struct ConstantTextureSelector<float> { using Type = ConstantTexture; };
template <> struct ConstantTextureSelector<Color> { using Type = ConstantColorTexture; };

/// Helper to choose a constant texture type based on the constant's type.
template <typename T> using ConstantTextureType = typename ConstantTextureSelector<T>::Type;

/// Texture made of an image, using the given filter and border handling mode.
template <typename ImageFilter, typename BorderMode>
class ImageTexture final : public ColorTexture {
public:
    ImageTexture(
        const Image& image,
        ImageFilter&& filter = ImageFilter(),
        BorderMode&& border_mode = BorderMode())
        : ColorTexture(make_image_texture_tag<ImageFilter, BorderMode>())
        , image_(image)
        , filter_(std::move(filter))
        , border_mode_(std::move(border_mode))
    {}

    Color sample_color(const proto::Vec2f& uv) const override;

    proto::fnv::Hasher& hash(proto::fnv::Hasher& hasher) const {
        return hasher.combine(tag).combine(&image_);
    }

    bool equals(const Texture& other) const override {
        return other.tag == tag && &static_cast<const ImageTexture&>(other).image() == &image_;
    }

    const Image& image() const { return image_; }

private:
    const Image& image_;

    ImageFilter filter_;
    BorderMode border_mode_;
};

} // namespace sol

#endif
