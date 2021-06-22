#include "sol/textures.h"

namespace sol {

proto::Vec2f BorderMode::Clamp::operator () (const proto::Vec2f& uv) const {
    return proto::clamp(uv, proto::Vec2f(0), proto::Vec2f(1));
}

proto::Vec2f BorderMode::Repeat::operator () (const proto::Vec2f& uv) const {
    auto u = uv[0] - std::floor(uv[0]);
    auto v = uv[1] - std::floor(uv[1]);
    return proto::Vec2f(u, v);
}

proto::Vec2f BorderMode::Mirror::operator () (const proto::Vec2f& uv) const {
    auto u = std::fmod(uv[0], 2.0f);
    auto v = std::fmod(uv[0], 2.0f);
    u = u > 1.0f ? 2.0f - u : u;
    v = v > 1.0f ? 2.0f - v : v;
    return proto::Vec2f(u, v);
}

template <typename F>
Color ImageFilter::Nearest::operator () (const proto::Vec2f& uv, size_t width, size_t height, F&& f) const {
    size_t x = proto::clamp<int>(uv[0] * width,  0, width  - 1);
    size_t y = proto::clamp<int>(uv[1] * height, 0, height - 1);
    return f(x, y);
}

template <typename F>
Color ImageFilter::Bilinear::operator () (const proto::Vec2f& uv, size_t width, size_t height, F&& f) const {
    auto i = uv[0] * (width - 1);
    auto j = uv[1] * (height - 1);
    auto u = i - std::floor(i);
    auto v = j - std::floor(j);
    auto x0 = proto::clamp<int>(i, 0, width  - 1);
    auto y0 = proto::clamp<int>(j, 0, height - 1);
    auto x1 = proto::clamp<int>(x0 + 1, 0, width  - 1);
    auto y1 = proto::clamp<int>(y0 + 1, 0, height - 1);
    auto p00 = f(x0, y0);
    auto p10 = f(x1, y0);
    auto p01 = f(x0, y1);
    auto p11 = f(x1, y1);
    return lerp(lerp(p00, p10, u), lerp(p01, p11, u), v);
}

template <typename ImageFilter, typename BorderMode>
Color ImageTexture<ImageFilter, BorderMode>::sample_color(const proto::Vec2f& uv) const {
    auto fixed_uv = border_mode_(uv);
    return filter_(fixed_uv, image_.width(), image_.height(),
        [&] (size_t i, size_t j) { return image_.rgb_at(i, j); });
}

template class ImageTexture<ImageFilter::Nearest, BorderMode::Clamp>;
template class ImageTexture<ImageFilter::Nearest, BorderMode::Repeat>;
template class ImageTexture<ImageFilter::Nearest, BorderMode::Mirror>;
template class ImageTexture<ImageFilter::Bilinear, BorderMode::Clamp>;
template class ImageTexture<ImageFilter::Bilinear, BorderMode::Repeat>;
template class ImageTexture<ImageFilter::Bilinear, BorderMode::Mirror>;

} // namespace sol
