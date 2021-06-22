#include "sol/algorithms/path_tracer.h"
#include "sol/image.h"
#include "sol/cameras.h"
#include "sol/bsdfs.h"
#include "sol/lights.h"

namespace sol {

void PathTracer::render(Image& image, size_t sample_index, size_t sample_count) const {
    using Sampler = PcgSampler;
    Renderer::for_each_tile(image.width(), image.height(),
        [&] (size_t xmin, size_t ymin, size_t xmax, size_t ymax) {
            for (size_t y = ymin; y < ymax; ++y) {
                for (size_t x = xmin; x < xmax; ++x) {
                    auto color = Color::black();
                    for (size_t i = 0; i < sample_count; ++i) {
                        Sampler sampler(Renderer::pixel_seed(sample_index + i, x, y));
                        auto ray = scene_.camera->generate_ray(
                            Renderer::sample_pixel(sampler, x, y, image.width(), image.height()));
                        color += trace_path(sampler, ray);
                    }
                    image.accumulate(x, y, color);
                }
            }
        });
}

static inline const Light* pick_light(Sampler& sampler, const Scene& scene) {
    // TODO: Sample lights adaptively
    auto light_index = std::min(static_cast<size_t>(sampler() * scene.lights.size()), scene.lights.size() - 1);
    return scene.lights[light_index].get();
}

Color PathTracer::trace_path(Sampler& sampler, proto::Rayf ray) const {
    static constexpr bool disable_mis = false;
    static constexpr bool disable_nee = false;
    static constexpr bool disable_rr  = false;

    auto light_pick_prob = 1.0f / scene_.lights.size();
    auto pdf_prev_bounce = 0.0f;
    auto throughput = Color::constant(1.0f);
    auto color = Color::black();

    for (size_t path_len = 0; path_len < config_.max_path_len; path_len++) {
        auto hit = scene_.intersect_closest(ray);
        if (!hit)
            break;

        auto out_dir = -ray.dir;

        // Direct hits on a light source
        if (hit->light && hit->surf_info.is_front_side) {
            // Convert the bounce pdf from solid angle to area measure
            auto pdf_prev_bounce_area =
                pdf_prev_bounce * proto::dot(out_dir, hit->surf_info.normal()) / (ray.tmax * ray.tmax);

            auto emission = hit->light->emission(out_dir, hit->surf_info.surf_coords);
            auto mis_weight = pdf_prev_bounce != 0.0f ?
                Renderer::balance_heuristic(pdf_prev_bounce_area, emission.pdf_area * light_pick_prob) : 1.0f;
            if constexpr (disable_mis || disable_nee)
                mis_weight = pdf_prev_bounce != 0 ? 0 : 1;
            color += throughput * emission.intensity * mis_weight;
        }

        if (!hit->bsdf)
            break;

        // Evaluate direct lighting
        bool skip_nee = disable_nee || hit->bsdf->type == Bsdf::Type::Specular;
        if (!skip_nee) {
            auto light = pick_light(sampler, scene_);
            auto light_sample = light->sample_area(sampler, hit->surf_info.point);

            auto in_dir   = light_sample.pos - hit->surf_info.point;
            auto cos_surf = proto::dot(in_dir, hit->surf_info.normal());
            auto shadow_ray = proto::Rayf::between_points(hit->surf_info.point, light_sample.pos, config_.ray_offset);

            if (cos_surf > 0 && !light_sample.intensity.is_black() && !scene_.intersect_any(shadow_ray)) {
                // Normalize the incoming direction
                auto inv_light_dist = 1.0f / proto::length(in_dir);
                cos_surf *= inv_light_dist;
                in_dir   *= inv_light_dist;

                auto pdf_bounce = light->has_area() ? hit->bsdf->pdf(in_dir, hit->surf_info, out_dir) : 0.0f;
                auto pdf_light  = light_sample.pdf_area * light_pick_prob;
                auto geom_term  = light_sample.cos * inv_light_dist * inv_light_dist;

                auto mis_weight = light->has_area() ?
                    Renderer::balance_heuristic(pdf_light, pdf_bounce * geom_term) : 1.0f;

                if constexpr (disable_mis)
                    mis_weight = 1;

                color +=
                    light_sample.intensity *
                    throughput *
                    hit->bsdf->eval(in_dir, hit->surf_info, out_dir) *
                    (geom_term * cos_surf * mis_weight / pdf_light);
            }
        }

        // Russian Roulette
        auto survival_prob = 1.0f;
        if (!disable_rr && path_len >= config_.min_rr_path_len) {
            survival_prob = proto::clamp(
                throughput.luminance(),
                config_.min_survival_prob,
                config_.max_survival_prob);
            if (sampler() >= survival_prob)
                break;
        }

        // Bounce
        auto bsdf_sample = hit->bsdf->sample(sampler, hit->surf_info, out_dir);
        throughput *= bsdf_sample.color * (bsdf_sample.cos / (bsdf_sample.pdf * survival_prob));
        ray = proto::Rayf(hit->surf_info.point, bsdf_sample.in_dir, config_.ray_offset);
        pdf_prev_bounce = skip_nee ? 0.0f : bsdf_sample.pdf;
    }
    return color;
}

} // namespace sol
