#include "lla/intro/register.hpp"

#include "lla/ui/text.hpp"
#include "lla/ui/theme.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace lla {
namespace {

constexpr float kIntroSeconds = 2.5f;

constexpr Color kBackground{12, 12, 16, 255};
constexpr Color kLetterbox{6, 6, 8, 255};
constexpr Color kBandFar{42, 46, 58, 255};
constexpr Color kBandMid{70, 78, 96, 255};
constexpr Color kBandNear{110, 122, 146, 255};
constexpr const char* kIntroTitle = "Intro";
constexpr const char* kLoading = "Loading";

void draw_sliding_blocks(ICanvas& canvas, float y, float height, float speed, float elapsed, Color color) {
    const float span = 240.f;
    const float block = 96.f;
    float offset = std::fmod(elapsed * speed, span);
    if (offset < 0.f) {
        offset += span;
    }
    const float width = static_cast<float>(canvas.width());
    for (float x = -span + offset; x < width + span; x += span) {
        canvas.fill_rect({x, y, block, height}, color);
    }
}

class IntroModule final : public IModule {
public:
    IntroModule(IPlayPhase* phase, IDeviceSettings* devices) : phase_(phase), devices_(devices) {}

    std::string_view name() const override { return "intro.cutscene"; }

    void update(const InputFrame& input) override {
        if (phase_ == nullptr || phase_->phase() != PlayPhase::Intro) {
            showing_ = false;
            ready_to_enter_ = false;
            return;
        }
        if (!showing_) {
            showing_ = true;
            elapsed_ = 0.f;
            ready_to_enter_ = false;
        }
        if (ready_to_enter_) {
            phase_->begin_party();
            showing_ = false;
            return;
        }
        if (input.dt > 0.f) {
            elapsed_ += input.dt;
        }
        if (elapsed_ >= kIntroSeconds) {
            ready_to_enter_ = true;
        }
    }

    void draw(ICanvas& canvas) override {
        if (!showing_ || phase_ == nullptr || phase_->phase() != PlayPhase::Intro) {
            return;
        }
        const float width = static_cast<float>(canvas.width());
        const float height = static_cast<float>(canvas.height());
        if (width <= 0.f || height <= 0.f) {
            return;
        }
        const ui::Theme theme = ui::theme_from(devices_);
        const bool contrast = devices_ != nullptr && devices_->get().high_contrast;
        canvas.clear(contrast ? theme.background : kBackground);

        const float bar_h = std::round(height * 0.14f);
        const float scene_top = bar_h;
        const float scene_bottom = height - bar_h;
        const float scene_h = scene_bottom - scene_top;
        draw_sliding_blocks(canvas, scene_top + scene_h * 0.22f, 28.f, 36.f, elapsed_, kBandFar);
        draw_sliding_blocks(canvas, scene_top + scene_h * 0.42f, 40.f, 70.f, elapsed_, kBandMid);
        draw_sliding_blocks(canvas, scene_top + scene_h * 0.66f, 56.f, 120.f, elapsed_, kBandNear);

        const Color letterbox = contrast ? theme.panel : kLetterbox;
        canvas.fill_rect({0.f, 0.f, width, bar_h}, letterbox);
        canvas.fill_rect({0.f, scene_bottom, width, bar_h}, letterbox);

        const float title_h = static_cast<float>(ui::text_height(theme.title_scale));
        const Rect scene{0.f, scene_top, width, scene_h};
        Rect title_box{16.f, scene_top + 28.f, std::max(1.f, width - 32.f), title_h};
        title_box = ui::intersect_rect(title_box, scene);
        ui::draw_text_box(canvas, title_box, kIntroTitle, theme.title, theme.title_scale, ui::TextAlign::Center, false);

        const float progress = std::clamp(elapsed_ / kIntroSeconds, 0.f, 1.f);
        const float track_w = std::min(520.f, std::max(160.f, width - 160.f));
        const float track_h = 18.f;
        const float track_x = std::round((width - track_w) * 0.5f);
        const float track_y = scene_bottom - 72.f;
        canvas.fill_rect({track_x, track_y, track_w, track_h}, theme.button);
        ui::stroke_rect(canvas, {track_x, track_y, track_w, track_h}, theme.button_edge, 2);
        if (progress > 0.f) {
            canvas.fill_rect({track_x, track_y, std::max(2.f, track_w * progress), track_h}, theme.selected);
        }

        const float label_h = static_cast<float>(ui::text_height(theme.text_scale));
        const float label_gap = static_cast<float>(ui::text_height(2));
        float label_y = track_y - label_gap - label_h;
        const float below_title = title_box.y + title_box.h + 8.f;
        if (label_y < below_title) {
            label_y = below_title;
        }
        Rect label_box{16.f, label_y, std::max(1.f, width - 32.f), label_h};
        const Rect above_track{0.f, scene_top, width, std::max(0.f, track_y - 8.f - scene_top)};
        label_box = ui::intersect_rect(label_box, above_track);
        ui::draw_text_box(canvas, label_box, kLoading, theme.label, theme.text_scale, ui::TextAlign::Center, false);
    }

private:
    IPlayPhase* phase_ = nullptr;
    IDeviceSettings* devices_ = nullptr;
    bool showing_ = false;
    bool ready_to_enter_ = false;
    float elapsed_ = 0.f;
};

}  // namespace

void register_intro(ModuleRegistry& registry, IPlayPhase* phase, IDeviceSettings* devices) {
    registry.add<IntroModule>(phase, devices);
}

}  // namespace lla
