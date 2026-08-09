# Retained Layer Cache

This document describes the runtime-level retained layer cache. It is a bottom-layer optimization and does not require component API changes.

## Goal

Dirty repaint reduces the repaint area. It does not reduce the number of static primitives that must be replayed inside that area.

The retained layer cache addresses that second cost:

```text
static subtree primitives -> offscreen layer texture
dirty repaint -> draw cached layer texture + dynamic primitives
```

This is useful for complex pages such as Gallery, where a button hover can intersect many static cards, shadows, and text runs.

## Current MVP

The implementation is intentionally conservative:

- Runtime automatically selects static child subtrees and adjacent stable siblings.
- Two or more adjacent eligible siblings can be merged into one retained paint-run layer without changing paint order.
- Components do not opt in and do not expose a cache API.
- OpenGL stores each retained layer as a texture-backed framebuffer.
- Vulkan stores each retained layer as a sampled color-attachment image plus framebuffer, and composites it with premultiplied alpha.
- Non-supporting backends safely fall back to normal primitive replay.
- Backdrop blur and dependent visual subtrees are not cached. They split sibling runs, so eligible static siblings before and after them can still be cached as separate runs.
- Animated, interactive, scroll, timer, frame callback, dirty-key, image, and SVG subtrees are not cached.
- A subtree must have enough draw cost and area before it is cached.
- A candidate must be stable for two frames before creating a layer texture.

The cache key includes structure, paint bounds, draw cost, DPI scale, and paint-affecting element properties.

## Hot Path Optimization

After the first MVP shipped, the runtime kept the candidate checks on the render hot path too long. That was fine for complex static pages, but it was wasteful on animation-heavy demos that mostly contain leaf primitives.

The current shape is:

- `layout()` / subtree rebuild caches static blocker flags on `Element`.
- `update()` caches whether a subtree currently has active animation in `PaintBoundsInstance`.
- `render()` only reads those cached flags and skips retained-layer probing for leaf-only children.

This keeps the optimization bottom-layer only, while avoiding repeated subtree recursion during every frame.

## Render Flow

```text
render dirty rect
  traverse ordered children
    collect each adjacent run of eligible stable siblings
    if a run has at least two siblings and passes combined cost/area checks:
      use or build one retained paint-run layer
    otherwise process each child independently:
      if child subtree has a valid retained layer:
        draw layer texture clipped by dirty/scissor
      else if child subtree is a cache candidate:
        build its retained layer and draw the child normally for this frame
      else:
        render child subtree normally
```

Building either an individual subtree layer or a sibling paint-run layer disables nested retained-layer reuse for that build. This avoids nested framebuffer state surprises.

A freshly rebuilt layer is not sampled in the same frame that creates it. The runtime marks the layer valid, renders the subtree or sibling run through the normal primitive path for that frame, and requests one follow-up full paint. The follow-up frame lets the render cache and the retained layer become visible together from a stable state. After that, unchanged static content continues to use retained-layer hits; the cache is not disabled for transition-capable static UI.

## Stats

The window title render stats include:

```text
Layer H/M/D/Re
```

- `H`: retained layer cache hits.
- `M`: cache misses or unstable candidates.
- `D`: layer texture draws.
- `Re`: layer texture rebuilds.

Healthy button interaction on a complex static page should trend toward high `H`, low `Re`, and lower primitive draw counts.

## Known Limits

- Sibling paint runs only merge adjacent eligible children; blockers split a run, and the runtime never reorders or merges across them.
- It currently avoids inherited active transforms for correctness.
- It does not cache backdrop blur because blur samples existing framebuffer content.
- It is not a full retained scene graph. Rect batching is a separate backend optimization, not a general cross-primitive batch renderer provided by this cache.
- Vulkan keeps retained layer textures alive across swapchain rebuilds when possible, while recreating render-pass-dependent framebuffers lazily.
