#pragma once

#include "components/mousearea.h"
#include "components/theme.h"
#include "core/dsl.h"
#include "core/image.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace components {

struct CarouselItem {
    std::string source;
    std::string title;
    std::string subtitle;
};

struct CarouselStyle {
    CarouselStyle() : CarouselStyle(theme::DarkThemeColors()) {}

    explicit CarouselStyle(const theme::ThemeColorTokens& tokens) {
        background = tokens.surface;
        border = theme::withOpacity(tokens.border, 0.62f);
        text = theme::withAlpha(tokens.text, 0.96f);
        mutedText = theme::withAlpha(tokens.text, 0.66f);
        overlayTop = theme::color(0.0f, 0.0f, 0.0f, 0.0f);
        overlayBottom = theme::color(0.0f, 0.0f, 0.0f, tokens.dark ? 0.60f : 0.46f);
        indicator = theme::withAlpha(tokens.text, 0.28f);
        activeIndicator = tokens.primary;
        shadow = theme::shadow(tokens, 26.0f, 10.0f, 0.28f, 0.14f);
    }

    core::Color background;
    core::Color border;
    core::Color text;
    core::Color mutedText;
    core::Color overlayTop;
    core::Color overlayBottom;
    core::Color indicator;
    core::Color activeIndicator;
    core::Shadow shadow;
    float radius = 22.0f;
};

class CarouselBuilder {
public:
    CarouselBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    CarouselBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    CarouselBuilder& items(std::vector<CarouselItem> value) { items_ = std::move(value); return *this; }
    CarouselBuilder& index(float value) { index_ = value; return *this; }
    CarouselBuilder& active(int value) { index_ = static_cast<float>(value); return *this; }
    CarouselBuilder& cardWidthRatio(float value) { cardWidthRatio_ = std::clamp(value, 0.32f, 1.0f); return *this; }
    CarouselBuilder& overlap(float value) { overlap_ = std::clamp(value, 0.0f, 0.60f); return *this; }
    CarouselBuilder& parallax(float value) { parallax_ = std::max(0.0f, value); return *this; }
    CarouselBuilder& style(const CarouselStyle& value) { style_ = value; return *this; }
    CarouselBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = CarouselStyle(tokens); return *this; }
    CarouselBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    CarouselBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }
    CarouselBuilder& onChange(std::function<void(float)> callback) { onChange_ = std::move(callback); return *this; }

    void build() {
        const int count = static_cast<int>(items_.size());
        const float safeWidth = std::max(1.0f, width_);
        const float safeHeight = std::max(1.0f, height_);
        const float center = clampIndex(index_, count);
        const float cardWidth = std::clamp(safeWidth * cardWidthRatio_, std::min(260.0f, safeWidth), safeWidth);
        const float cardHeight = safeHeight;
        const float sideStep = cardWidth * (1.0f - overlap_);
        const float cardX = (safeWidth - cardWidth) * 0.5f;
        const float dragStep = std::max(80.0f, sideStep);
        const std::function<void(float)> onChange = onChange_;
        CarouselState& state = stateFor(id_);
        const core::Transition cardTransition = state.directMotion ? core::Transition::none() : transition_;
        const auto emitIndex = [&state, count, onChange](float next, bool directMotion) {
            if (!onChange || count <= 1) {
                return;
            }
            const float clamped = clampIndex(next, count);
            if (state.hasEmittedIndex && !shouldEmitIndex(state.emittedIndex, clamped)) {
                return;
            }
            state.directMotion = directMotion;
            state.emittedIndex = clamped;
            state.hasEmittedIndex = true;
            onChange(clamped);
        };
        const auto scrollToIndex = [center, emitIndex](const MouseScrollEvent& event) {
            if (event.stepY == 0.0f) {
                return;
            }
            emitIndex(center - event.stepY, true);
        };
        const auto beginDrag = [&state, center](const MouseEvent&) {
            state.dragStartIndex = center;
            state.directMotion = true;
            state.emittedIndex = center;
            state.hasEmittedIndex = true;
        };
        const auto dragToIndex = [&state, dragStep, emitIndex](const MouseDragEvent& event) {
            if (dragStep <= 0.0f) {
                return;
            }
            emitIndex(state.dragStartIndex - event.totalX / dragStep, true);
        };
        const auto jumpToIndex = [emitIndex](float next) {
            emitIndex(next, false);
        };

        ui_.stack(id_)
            .size(safeWidth, safeHeight)
            .clip()
            .content([&] {
                if (count == 0) {
                    drawEmptyState(safeWidth, safeHeight);
                    return;
                }

                mouseArea(ui_, id_ + ".input")
                    .size(safeWidth, safeHeight)
                    .zIndex(kRootInputZIndex)
                    .scrollStep(kScrollStep)
                    .maxScrollStep(1.0f)
                    .dragThreshold(kDragThreshold)
                    .onScroll(scrollToIndex)
                    .onDragStart(beginDrag)
                    .onDrag(dragToIndex)
                    .build();

                const int anchorIndex = static_cast<int>(std::floor(center));
                for (int offset = -1; offset <= 2; ++offset) {
                    const int itemIndex = anchorIndex + offset;
                    if (itemIndex < 0 || itemIndex >= count) {
                        continue;
                    }
                    drawCard(itemIndex, cardX, 0.0f, cardWidth, cardHeight, sideStep, center, cardTransition, jumpToIndex, scrollToIndex, beginDrag, dragToIndex);
                }

                drawIndicators(safeWidth, safeHeight, count, static_cast<int>(std::round(center)), jumpToIndex);
            })
            .build();
    }

private:
    static constexpr float kVisibleDistance = 1.45f;
    static constexpr float kDetailedDistance = 1.05f;
    static constexpr float kInteractiveDistance = 1.30f;
    static constexpr float kIndexEmitThreshold = 0.01f;
    static constexpr float kScrollStep = 0.18f;
    static constexpr float kDragThreshold = 3.0f;
    static constexpr int kRootInputZIndex = -1000;

    static float clampIndex(float value, int count) {
        if (count <= 0) {
            return 0.0f;
        }
        return std::clamp(value, 0.0f, static_cast<float>(count - 1));
    }

    static float smoothAmount(float value) {
        const float t = std::clamp(value, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    static bool shouldEmitIndex(float previous, float next) {
        return std::fabs(previous - next) >= kIndexEmitThreshold;
    }

    struct CarouselState {
        float dragStartIndex = 0.0f;
        float emittedIndex = 0.0f;
        bool directMotion = false;
        bool hasEmittedIndex = false;
    };

    static CarouselState& stateFor(const std::string& id) {
        static std::unordered_map<std::string, CarouselState> states;
        return states[id];
    }

    void drawEmptyState(float width, float height) {
        ui_.rect(id_ + ".empty.bg")
            .size(width, height)
            .color(style_.background)
            .radius(style_.radius)
            .border(1.0f, style_.border)
            .build();

        ui_.text(id_ + ".empty.text")
            .size(width, height)
            .text("No items")
            .fontSize(18.0f)
            .lineHeight(22.0f)
            .color(style_.mutedText)
            .horizontalAlign(core::HorizontalAlign::Center)
            .verticalAlign(core::VerticalAlign::Center)
            .build();
    }

    void drawCard(int itemIndex,
                  float baseX,
                  float baseY,
                  float cardWidth,
                  float cardHeight,
                  float sideStep,
                  float center,
                  const core::Transition& cardTransition,
                  const std::function<void(float)>& onChange,
                  const std::function<void(const MouseScrollEvent&)>& onScroll,
                  const std::function<void(const MouseEvent&)>& onDragStart,
                  const std::function<void(const MouseDragEvent&)>& onDrag) {
        const CarouselItem& item = items_[static_cast<std::size_t>(itemIndex)];
        const float distance = static_cast<float>(itemIndex) - center;
        const float absDistance = std::fabs(distance);
        if (absDistance > kVisibleDistance) {
            return;
        }

        const float focus = 1.0f - std::clamp(absDistance, 0.0f, 1.0f);
        const float side = smoothAmount(std::min(absDistance, 1.0f));
        const float scale = 1.0f - side * 0.12f - std::max(0.0f, absDistance - 1.0f) * 0.06f;
        const float opacity = std::clamp(1.0f - absDistance * 0.24f, 0.28f, 1.0f);
        const float x = baseX + distance * sideStep;
        const float y = baseY + side * 18.0f + std::max(0.0f, absDistance - 1.0f) * 8.0f;
        const float textShift = -distance * parallax_ * 0.08f;
        const int z = 100 - static_cast<int>(std::round(absDistance * 10.0f));
        const bool active = absDistance < 0.55f;
        const bool detailed = absDistance <= kDetailedDistance;
        const bool interactive = absDistance <= kInteractiveDistance;
        const bool drawShadow = active && style_.shadow.enabled && focus > 0.92f;
        const std::string cardId = id_ + ".card." + std::to_string(itemIndex);
        const bool imageReady = item.source.empty() || core::ImagePrimitive::isSourceReady(item.source);
        const bool drawSurface = drawShadow || detailed;

        ui_.stack(cardId)
            .x(x)
            .y(y)
            .size(cardWidth, cardHeight)
            .zIndex(z)
            .clip()
            .scale(scale)
            .transformOrigin(0.5f, 0.5f)
            .opacity(opacity)
            .transition(cardTransition)
            .animate(core::AnimProperty::Frame | core::AnimProperty::Transform | core::AnimProperty::Opacity)
            .content([&] {
                if (drawSurface) {
                    ui_.rect(cardId + ".surface")
                        .size(cardWidth, cardHeight)
                        .color(style_.background)
                        .radius(style_.radius)
                        .border(1.0f, style_.border)
                        .shadow(drawShadow ? style_.shadow : core::Shadow{})
                        .build();
                }

                ui_.stack(cardId + ".image.layer")
                    .size(cardWidth, cardHeight)
                    .content([&] {
                        if (!imageReady) {
                            const float loaderSize = std::clamp(cardHeight * 0.22f, 34.0f, 64.0f);
                            ui_.image(cardId + ".loader")
                                .x((cardWidth - loaderSize) * 0.5f)
                                .y((cardHeight - loaderSize) * 0.5f)
                                .size(loaderSize, loaderSize)
                                .source("mona-loading-default.gif")
                                .contain()
                                .opacity(0.82f)
                                .build();
                        }

                        ui_.image(cardId + ".image")
                            .x(0.0f)
                            .y(0.0f)
                            .size(cardWidth, cardHeight)
                            .source(item.source)
                            .cover()
                            .radius(style_.radius)
                            .transformOrigin(0.5f, 0.5f)
                            .build();

                        ui_.rect(cardId + ".shade")
                            .size(cardWidth, cardHeight)
                            .gradient(style_.overlayTop, style_.overlayBottom, core::GradientDirection::Vertical)
                            .radius(style_.radius)
                            .opacity(active ? 1.0f : (detailed ? 0.78f : 0.68f))
                            .build();
                    })
                    .build();

                if (detailed) {
                    ui_.text(cardId + ".title")
                        .x(22.0f + textShift)
                        .y(std::max(0.0f, cardHeight - 64.0f))
                        .size(std::max(0.0f, cardWidth - 44.0f), 26.0f)
                        .text(item.title)
                        .fontSize(active ? 22.0f : 19.0f)
                        .lineHeight(24.0f)
                        .color(style_.text)
                        .build();

                    if (!item.subtitle.empty()) {
                        ui_.text(cardId + ".subtitle")
                            .x(22.0f + textShift)
                            .y(std::max(0.0f, cardHeight - 35.0f))
                            .size(std::max(0.0f, cardWidth - 44.0f), 18.0f)
                            .text(item.subtitle)
                            .fontSize(14.0f)
                            .lineHeight(18.0f)
                            .color(style_.mutedText)
                            .build();
                    }
                }

                if (interactive) {
                    mouseArea(ui_, cardId + ".hit")
                        .size(cardWidth, cardHeight)
                        .zIndex(20)
                        .radius(style_.radius)
                        .scrollStep(kScrollStep)
                        .maxScrollStep(1.0f)
                        .dragThreshold(kDragThreshold)
                        .onTap([onChange, itemIndex] {
                            if (onChange) {
                                onChange(static_cast<float>(itemIndex));
                            }
                        })
                        .onScroll(onScroll)
                        .onDragStart(onDragStart)
                        .onDrag(onDrag)
                        .build();
                }
            })
            .build();
    }

    void drawIndicators(float width, float height, int count, int activeIndex, const std::function<void(float)>& onChange) {
        const float dot = 7.0f;
        const float gap = 7.0f;
        const float total = static_cast<float>(count) * dot + static_cast<float>(std::max(0, count - 1)) * gap;
        const float startX = (width - total) * 0.5f;
        const float y = std::max(0.0f, height - 18.0f);
        for (int i = 0; i < count; ++i) {
            const bool active = i == activeIndex;
            ui_.rect(id_ + ".indicator." + std::to_string(i))
                .x(startX + static_cast<float>(i) * (dot + gap))
                .y(y)
                .size(active ? dot * 2.4f : dot, dot)
                .color(active ? style_.activeIndicator : style_.indicator)
                .radius(dot * 0.5f)
                .transition(transition_)
                .animate(core::AnimProperty::Frame | core::AnimProperty::Color)
                .onClick([onChange, i] {
                    if (onChange) {
                        onChange(static_cast<float>(i));
                    }
                })
                .build();
        }
    }

    core::dsl::Ui& ui_;
    std::string id_;
    std::vector<CarouselItem> items_;
    CarouselStyle style_;
    core::Transition transition_ = core::Transition::make(0.24f, core::Ease::OutCubic);
    std::function<void(float)> onChange_;
    float width_ = 720.0f;
    float height_ = 260.0f;
    float index_ = 0.0f;
    float cardWidthRatio_ = 0.68f;
    float overlap_ = 0.36f;
    float parallax_ = 28.0f;
};

inline CarouselBuilder carousel(core::dsl::Ui& ui, const std::string& id) {
    return CarouselBuilder(ui, id);
}

} // namespace components
