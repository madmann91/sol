#include "sol/algorithms/path_tracer.h"
#include "sol/image.h"
#include "sol/cameras.h"

namespace sol {

void PathTracer::render(Image& image, size_t frame_index) const {
    using Sampler = PcgSampler;
    Renderer::for_each_tile(image.width(), image.height(),
        [&] (size_t xmin, size_t ymin, size_t xmax, size_t ymax) {
            for (size_t y = ymin; y < ymax; ++y) {
                for (size_t x = xmin; x < xmax; ++x) {
                    Sampler sampler(Renderer::pixel_seed(frame_index, x, y));
                    auto ray = scene_.camera->generate_ray(
                        Renderer::sample_pixel(sampler, x, y, image.width(), image.height()));
                    auto color = trace_path(sampler, ray); 
                    image.accumulate(x, y, color);
                }
            }
        });
}

static inline const Light* pick_light(Sampler& sampler, const Scene& scene) {
    // TODO: Sample lights adaptively
    auto light_index = proto::clamp<size_t>(sampler() * scene.lights.size(), 0, scene.lights.size() - 1);
    return scene.lights[light_index].get();
}

Color PathTracer::trace_path(Sampler& sampler, proto::Rayf ray) const {
    auto light_pick_prob = 1.0f / scene_.lights.size();
    auto pdf_prev_bounce = 0.0f;
    auto throughput = Color::constant(1.0f);
    auto color = Color::black();

    ray.tmin = config_.ray_offset;
    for (size_t path_len = 0; path_len < config_.max_path_len; path_len++) {
        auto hit = scene_.intersect_closest(ray);
        if (!hit)
            break;

        auto out_dir = -ray.dir;

        // Direct hits on a light source
        if (hit->light && hit->surf_info.is_front_side) {
            auto emission = hit->light->emission(out_dir, hit->surf_info.surf_coords);
            // Convert the bounce pdf from solid angle to area
            auto pdf_prev_bounce_area = pdf_prev_bounce * proto::dot(out_dir, hit->surf_info.normal()) / (ray.tmax * ray.tmax);
            auto mis_weight = Renderer::balance_heuristic(pdf_prev_bounce_area, emission.pdf_area * light_pick_prob);
            color += throughput * emission.intensity * mis_weight;
        }

        if (!hit->bsdf)
            break;

        // Evaluate direct lighting
        bool is_specular = hit->bsdf->type() == Bsdf::Type::Specular;
        if (!is_specular) {
            auto light = pick_light(sampler, scene_);
            auto light_sample = light->sample_area(sampler, hit->surf_info.point);

            auto in_dir   = light_sample.pos - hit->surf_info.point;
            auto cos_surf = proto::dot(in_dir, hit->surf_info.point);
            auto shadow_ray = proto::Rayf::between_points(hit->surf_info.point, light_sample.pos, config_.ray_offset);

            if (cos_surf > 0 &&
                !light_sample.intensity.is_black() &&
                !scene_.intersect_any(shadow_ray))
            {
                // Normalize the incoming direction
                auto light_dist = proto::length(in_dir);
                auto light_dist_squared = light_dist * light_dist;
                cos_surf /= light_dist;
                in_dir   /= light_dist;

                auto pdf_bounce = light->has_area() ? hit->bsdf->pdf(in_dir, hit->surf_info, out_dir) : 0.0f;
                auto pdf_light  = light_sample.pdf_area * light_pick_prob;
                auto geom_term  = cos_surf / light_dist_squared;

                auto mis_weight = light->has_area() ?
                    Renderer::balance_heuristic(pdf_light, pdf_bounce * geom_term) : 1.0f;

                color +=
                    light_sample.intensity *
                    throughput *
                    hit->bsdf->eval(in_dir, hit->surf_info, out_dir) *
                    (light_sample.cos * geom_term * mis_weight / pdf_light);
            }
        }

        // Russian Roulette
        auto survival_prob = 1.0f;
        if (path_len >= config_.min_russian_roulette_path_len) {
            survival_prob = Renderer::survival_prob(
                throughput.luminance(),
                config_.min_survival_prob,
                config_.max_survival_prob);
            if (sampler() >= survival_prob)
                break;
        }

        // Bounce
        auto bsdf_sample = hit->bsdf->sample(sampler, hit->surf_info, out_dir);
        throughput *= bsdf_sample.color * bsdf_sample.cos / (bsdf_sample.pdf * survival_prob);
        ray.dir = bsdf_sample.in_dir;
        ray.org = hit->surf_info.point;
        pdf_prev_bounce = is_specular ? 0.0f : bsdf_sample.pdf;
    }
    return color;
}

} // namespace sol