#pragma once

#include "components/theme.h"
#include "components/tooltip.h"
#include "core/dsl.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace components {

struct LineChartStyle {
    LineChartStyle() : LineChartStyle(theme::dark()) {}

    explicit LineChartStyle(const theme::ThemeColorTokens& tokens) {
        background = tokens.surface;
        title = tokens.text;
        label = theme::withOpacity(tokens.text, 0.56f);
        grid = theme::withOpacity(tokens.border, tokens.dark ? 0.38f : 0.36f);
        line = tokens.primary;
        point = tokens.primary;
        pointHover = core::mixColor(tokens.primary, theme::color(1.0f, 1.0f, 1.0f), tokens.dark ? 0.18f : 0.10f);
        pointPressed = core::mixColor(tokens.primary, theme::color(0.0f, 0.0f, 0.0f), 0.12f);
        tooltipBackground = tokens.dark
            ? core::mixColor(tokens.surface, theme::color(0.0f, 0.0f, 0.0f), 0.18f)
            : theme::color(1.0f, 1.0f, 1.0f, 0.96f);
        tooltipText = tokens.text;
        border = theme::withOpacity(tokens.border, 0.76f);
        shadow = theme::shadow(tokens, 18.0f, 4.0f, 0.20f, 0.10f);
    }

    core::Color background;
    core::Color title;
    core::Color label;
    core::Color grid;
    core::Color line;
    core::Color point;
    core::Color pointHover;
    core::Color pointPressed;
    core::Color tooltipBackground;
    core::Color tooltipText;
    core::Color border;
    core::Shadow shadow;
    float radius = 18.0f;
};

class LineChartBuilder {
    struct TooltipItem {
        std::string sourceId;
        std::string text;
        float x = 0.0f;
        float y = 0.0f;
    };

public:
    LineChartBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    LineChartBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    LineChartBuilder& title(const std::string& value) { title_ = value; return *this; }
    LineChartBuilder& values(std::vector<float> value) { values_ = std::move(value); return *this; }
    LineChartBuilder& labels(std::vector<std::string> value) { labels_ = std::move(value); return *this; }
    LineChartBuilder& style(const LineChartStyle& value) { style_ = value; return *this; }
    LineChartBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = LineChartStyle(tokens); return *this; }
    LineChartBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }

    void build() {
        if (values_.empty()) {
            values_ = {0.22f, 0.30f, 0.20f, 0.55f, 0.42f, 0.86f};
        }

        const float titleX = 20.0f;
        const float titleY = 18.0f;
        const float plotX = 28.0f;
        const float plotY = 70.0f;
        const float plotWidth = std::max(1.0f, width_ - 56.0f);
        const float plotHeight = std::max(1.0f, height_ - 112.0f);
        const float bottomY = plotY + plotHeight;
        AnimState& anim = ui_.state<AnimState>(id_ + ".anim");
        syncAnimation(anim);
        const std::vector<float>& displayValues = anim.display;
        const int count = static_cast<int>(displayValues.size());
        const float stepX = count > 1 ? plotWidth / static_cast<float>(count - 1) : 0.0f;
        const float lineHeight = 4.0f;
        const float pointSize = 13.0f;

        ui_.stack(id_)
            .size(width_, height_)
            .content([&] {
                ui_.rect(id_ + ".bg")
                    .size(width_, height_)
                    .color(style_.background)
                    .radius(style_.radius)
                    .border(1.0f, style_.border)
                    .shadow(style_.shadow)
                    .build();

                ui_.text(id_ + ".title")
                    .x(titleX)
                    .y(titleY)
                    .size(std::max(0.0f, width_ - titleX * 2.0f), 28.0f)
                    .text(title_)
                    .fontSize(22.0f)
                    .lineHeight(26.0f)
                    .color(style_.title)
                    .build();

                for (int line = 0; line < 4; ++line) {
                    const float y = plotY + static_cast<float>(line) * plotHeight / 3.0f;
                    ui_.rect(id_ + ".grid." + std::to_string(line))
                        .x(plotX)
                        .y(y)
                        .size(plotWidth, 1.0f)
                        .color(style_.grid)
                        .build();
                }

                for (int index = 0; index + 1 < count; ++index) {
                    const core::Vec2 from = pointAt(displayValues, index, plotX, bottomY, plotHeight, stepX);
                    const core::Vec2 to = pointAt(displayValues, index + 1, plotX, bottomY, plotHeight, stepX);
                    const float radius = lineHeight * 0.5f;
                    const float segmentX = std::min(from.x, to.x) - radius;
                    const float segmentY = std::min(from.y, to.y) - radius;
                    const float segmentWidth = std::fabs(to.x - from.x) + lineHeight;
                    const float segmentHeight = std::fabs(to.y - from.y) + lineHeight;

                    ui_.polygon(id_ + ".segment." + std::to_string(index))
                        .x(segmentX)
                        .y(segmentY)
                        .size(segmentWidth, segmentHeight)
                        .points(capsulePoints(from, to, radius, segmentX, segmentY))
                        .color(style_.line)
                        .transition(transition_)
                        .build();
                }

                std::vector<TooltipItem> tooltips;
                tooltips.reserve(displayValues.size());
                for (int index = 0; index < count; ++index) {
                    const core::Vec2 point = pointAt(displayValues, index, plotX, bottomY, plotHeight, stepX);
                    const std::string pointId = id_ + ".point." + std::to_string(index);
                    ui_.rect(pointId)
                        .x(point.x - pointSize * 0.5f)
                        .y(point.y - pointSize * 0.5f)
                        .size(pointSize, pointSize)
                        .states(style_.point, style_.pointHover, style_.pointPressed)
                        .radius(pointSize * 0.5f)
                        .instantStates()
                        .transition(transition_)
                        .animate(core::AnimProperty::Color)
                        .onClick([] {})
                        .build();

                    tooltips.push_back({pointId, dataLabel(index) + "  " + percent(valueAt(values_, index)), point.x, point.y});
                }

                const float labelWidth = std::max(28.0f, std::min(42.0f, count > 1 ? stepX : plotWidth));
                for (int index = 0; index < count; ++index) {
                    const core::Vec2 point = pointAt(displayValues, index, plotX, bottomY, plotHeight, stepX);
                    const float labelX = std::clamp(point.x - labelWidth * 0.5f, 0.0f, std::max(0.0f, width_ - labelWidth));
                    label(id_ + ".label." + std::to_string(index), dataLabel(index), labelX, height_ - 34.0f, labelWidth);
                }

                for (const TooltipItem& item : tooltips) {
                    components::tooltip(ui_, item.sourceId + ".tooltip")
                        .source(item.sourceId)
                        .value(item.text)
                        .anchor(item.x, item.y)
                        .bounds(width_, height_)
                        .style(tooltipStyle())
                        .build();
                }

                if (anim.animating) {
                    ui_.stack(id_ + ".animator")
                        .size(0.0f, 0.0f)
                        .onTimer(0.016f, [] {})
                        .build();
                }
            })
            .build();
    }

private:
    struct AnimState {
        std::vector<float> display;
        std::vector<float> target;
        bool animating = false;
    };

    static float valueAt(const std::vector<float>& values, int index) {
        if (index < 0 || index >= static_cast<int>(values.size())) {
            return 0.0f;
        }
        return values[static_cast<std::size_t>(index)];
    }

    static bool closeValues(const std::vector<float>& left, const std::vector<float>& right) {
        if (left.size() != right.size()) {
            return false;
        }
        for (std::size_t i = 0; i < left.size(); ++i) {
            if (std::fabs(left[i] - right[i]) > 0.001f) {
                return false;
            }
        }
        return true;
    }

    void syncAnimation(AnimState& anim) const {
        if (anim.display.size() != values_.size()) {
            anim.display = values_;
            anim.target = values_;
            anim.animating = false;
            return;
        }

        if (!closeValues(anim.target, values_)) {
            anim.target = values_;
            anim.animating = true;
        }

        if (!anim.animating) {
            return;
        }

        bool moving = false;
        for (std::size_t i = 0; i < anim.display.size(); ++i) {
            const float next = anim.display[i] + (anim.target[i] - anim.display[i]) * 0.20f;
            if (std::fabs(next - anim.target[i]) > 0.002f) {
                anim.display[i] = next;
                moving = true;
            } else {
                anim.display[i] = anim.target[i];
            }
        }
        anim.animating = moving;
    }

    static core::Vec2 pointAt(const std::vector<float>& values,
                              int index,
                              float plotX,
                              float bottomY,
                              float plotHeight,
                              float stepX) {
        const float value = std::clamp(valueAt(values, index), 0.0f, 1.0f);
        return {
            plotX + static_cast<float>(index) * stepX,
            bottomY - value * plotHeight
        };
    }

    static std::vector<core::Vec2> capsulePoints(const core::Vec2& from,
                                                 const core::Vec2& to,
                                                 float radius,
                                                 float originX,
                                                 float originY) {
        const float dx = to.x - from.x;
        const float dy = to.y - from.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length <= 0.001f) {
            return circlePoints(from, radius, originX, originY);
        }

        const float angle = std::atan2(dy, dx);
        const float normalAngle = angle + 1.57079632679f;
        std::vector<core::Vec2> points;
        points.reserve(18u);
        appendArc(points, to, radius, normalAngle, normalAngle - 3.14159265359f, originX, originY);
        appendArc(points, from, radius, normalAngle - 3.14159265359f, normalAngle - 6.28318530718f, originX, originY);
        return points;
    }

    static std::vector<core::Vec2> circlePoints(const core::Vec2& center, float radius, float originX, float originY) {
        std::vector<core::Vec2> points;
        points.reserve(18u);
        appendArc(points, center, radius, 0.0f, 6.28318530718f, originX, originY);
        return points;
    }

    static void appendArc(std::vector<core::Vec2>& points,
                          const core::Vec2& center,
                          float radius,
                          float startAngle,
                          float endAngle,
                          float originX,
                          float originY) {
        constexpr int segments = 8;
        for (int step = 0; step <= segments; ++step) {
            const float t = static_cast<float>(step) / static_cast<float>(segments);
            const float angle = startAngle + (endAngle - startAngle) * t;
            points.push_back({
                center.x + std::cos(angle) * radius - originX,
                center.y + std::sin(angle) * radius - originY
            });
        }
    }

    void label(const std::string& id, const std::string& value, float x, float y, float width) {
        ui_.text(id)
            .x(x)
            .y(y)
            .size(width, 22.0f)
            .text(value)
            .fontSize(14.0f)
            .lineHeight(18.0f)
            .color(style_.label)
            .horizontalAlign(core::HorizontalAlign::Center)
            .build();
    }

    std::string dataLabel(int index) const {
        if (index >= 0 && index < static_cast<int>(labels_.size())) {
            return labels_[index];
        }
        return "P" + std::to_string(index + 1);
    }

    static std::string percent(float value) {
        return std::to_string(static_cast<int>(std::clamp(value, 0.0f, 1.0f) * 100.0f + 0.5f)) + "%";
    }

    TooltipStyle tooltipStyle() const {
        TooltipStyle result;
        result.background = style_.tooltipBackground;
        result.text = style_.tooltipText;
        result.border = style_.border;
        return result;
    }

    core::dsl::Ui& ui_;
    std::string id_;
    std::string title_ = "LineChart";
    std::vector<float> values_;
    std::vector<std::string> labels_ = {"Jan", "Feb", "Mar", "Apr", "May", "Jun"};
    LineChartStyle style_;
    core::Transition transition_ = core::Transition::make(0.16f, core::Ease::OutCubic);
    float width_ = 206.0f;
    float height_ = 236.0f;
};

inline LineChartBuilder lineChart(core::dsl::Ui& ui, const std::string& id) {
    return LineChartBuilder(ui, id);
}

} // namespace components
