`habbanya erg true 2.json` is the current template. CMake copies it to `build/src`
when configuring the application; edit this source copy to retain changes.
Intermediate sand zones 10, 11, 12, 14, 15, 16, 17, 19 use radius `3.0` and
susceptibility `-0.125` to give them more space and push them toward the perimeter
between the rock islands. Reload the template after changing its runtime JSON.

### Explicit edge geometry example

[`edge geometry example.json`](edge%20geometry%20example.json) is a four-zone star
with zone 1 connected to zones 2, 3 and 4. It is copied to `build/src` and fixes
the map at 64×64 to make its absolute tile distances easy to compare. All radii
are 1 and susceptibilities are 0, so neither relative size nor magnets explain
the different requested lengths. The zones use distinct materials: 1 rock,
2 sand, 3 dunes, 4 spice. Enable zone IDs to identify their seed points.

| Connection | `preferredCenterDistance` | `distanceWeight` | Purpose |
| --- | ---: | ---: | --- |
| 1—2 | 8 tiles | 20 | Strong short-distance preference |
| 1—3 | 8 tiles | 1 | Same requested distance, weaker preference |
| 1—4 | 24 tiles | 20 | Strong long-distance preference |

The optional fields belong inside each symmetrical edge's `data` object:

```json
"geometry": {"preferredCenterDistance": 24.0, "distanceWeight": 20.0}
```

`preferredCenterDistance` is the desired straight-line distance between the two
embedded seed points, in tiles. It does not request passage width or a shared
border length. If omitted, spacing comes from the node radii and map area.

`distanceWeight` controls how strongly that edge's distance error affects
refinement. Conceptually its contribution is proportional to
`distanceWeight * (actualDistance - preferredDistance)^2`, alongside repulsion
and other placement preferences. Weight 20 makes that error more costly than
weight 1; it does not multiply the requested distance. Omission means 1. A weight
of 0 disables only the distance objective; the graph connection, passages and
planarity constraints remain.

Try seed `42`, then change only the 1—3 weight from 1 to 20 and compare with the
same seed. Its distance should fit the 8-tile request more closely. Increase
the 1—4 target from 24 to 32 to ask for a longer connection. Reload the template
after edits. All lengths are preferences, so planarity, map bounds and other
forces can prevent an exact match. The other bundled templates omit `geometry`
and use their radius-derived default distances.

Measured at seed `42`, the supplied template produces center distances about
`8.86`, `14.93`, and `23.72` tiles for 1—2, 1—3, and 1—4 respectively. Raising
only the 1—3 weight to 20 brings that distance to about `8.99` tiles. Removing
all three geometry objects gives roughly 32–33 tiles per connection at this map
size. Full generation and crossing checks passed for the supplied example and
both comparisons at seeds `42`, `12419`, and `2026`.

### Magnetic grid example

`grid3x3 magnetic.json` is a small magnetic-placement example, also copied to
`build/src`. It has nine zones with radius `1.0` and twelve connections arranged as a
3×3 grid graph (IDs 0–8 in row order). Node 4 is rock; the others are sand.
The planar initializer and refinement use seeded variation; connections do not
impose fixed grid coordinates.

Every node has `susceptibility: 0.02`, which attracts it toward the application's
central magnet. Set this to `0.0` for a neutral comparison or `-0.02` for repulsion;
use the same seed for each comparison. Seed `12419` is used in the regression
tests. Magnetic forces begin after the first twenty embedding steps.

Planar refinement constrains magnetic movement and validates the result after
rounding to tiles, preventing intersecting edges and coincident raster centers.
Strong attraction or repulsion remains a preference and may be limited by these
constraints.

Each rock island explicitly contains one resource entry:

```json
{"name": "rock", "startThreshold": 0.0, "endThreshold": 1.0}
```

All generated boundary walls, including sand-to-sand walls, use the top-level rule:

```json
"walls": {"resource": "cliff_rock"}
```

Plain rock and cliff rock are separate materials. Resource arrays can be reordered
independently and do not need zero-width placeholders for unused materials.
Each zone's positive resource intervals must cover `[0, 1]` without gaps or overlaps.

Validation happens before a template becomes active. Errors identify the exact
field, for example `nodes/9/data/resources/octave3_weight`. A failed load retains
the last valid template, and each new attempt clears the previous error.

Numeric fields accept both integer and decimal JSON syntax (`3` and `3.0`).
Radii must be positive; susceptibility may be negative. Noise weights and bonus
values must be non-negative. IDs, resource-blending/intake distances, bonus-area
budgets, and passage dimensions must be non-negative whole numbers within the
C++ `int` range. Preferred center distances may be decimal numbers. Passage and wall
minimums cannot exceed their maximums, and octave radii must have `minRadius <= maxRadius`.
Numbers must be finite; strings and booleans are not converted to numbers.

Declare one to three consecutive octaves starting with `octave1`. Each zone gets
one weight per declared octave, defaulting to zero when omitted. A weight for an
undeclared octave is an error. To remove an octave, also remove its weights from
the zones. Different resource-list lengths and orders remain valid; each list
must independently cover the full threshold range and use known material names.

Node IDs must be unique. Connections must reference existing, distinct nodes;
duplicate undirected connections and duplicate rules in the same direction are
rejected. A directed rule requires a corresponding undirected connection. Each
zone needs at least one undirected connection because the current generator
starts zone expansion from graph edges.

### Zone sizes and connection geometry

Templates are checked for planarity when loaded. Nonplanar zone graphs are
rejected with an obstruction listing original edge IDs, while the previous
template stays selected. Directed resource rules do not add adjacency edges.

Embedding constructs a planar drawing, then refines it with movement constraints
that keep nodes and edges separated. Intersections are forbidden; distances and
sizes are preferences. The graph's bounding box remains centered on the map.
Seeded variation changes relative positions and lengths, and embedding uses its
own random stream so its initialization attempts do not consume growth randomness.

Node radii now also guide embedding spacing. Relative radii are normalized to
the map area; an unspecified edge prefers approximately 0.9 times the sum of its
two normalized radii. Repulsion, boundary constraints and magnets influence the
result. This is approximate size control, not a final zone-area guarantee.

Node `data.radius` is a relative weight used later when seeding zone growth.
For an edge from A to B, A receives the first `rA / (rA + rB)` fraction of the
rasterized center-to-center line, and B receives the remainder (rounded to
whole tiles). Thus radii 3 and 1 split that line approximately 75%/25%.
Multiplying every radius by the same amount has no effect. Random growth and
smoothing follow, so radius does not prescribe a final tile area or a geometric
radius in map units.

The connection fields currently have these meanings:

| Fields | Used for |
| --- | --- |
| `geometry.preferredCenterDistance` | Preferred distance between the two seed points, in tiles |
| `geometry.distanceWeight` | Non-negative importance of that edge's distance preference; default 1 |
| `passes.minPassWidth`, `maxPassWidth` | Passage widths on the generated zone border |
| `passes.minWallLength`, `maxWallLength` | Wall lengths between passages |
| `resourceBlend.distance` | Resource blending across a zone border |
| `intakeDistance`, `areaGuaranteed`, `bonusValue` | Directed resource influence and guarantees during blending |

Add the optional `geometry` object inside a symmetrical edge's `data`:

```json
"geometry": {"preferredCenterDistance": 15.0, "distanceWeight": 2.0}
```

Explicit distances must be finite and positive. Omission selects the radius-based
default. A weight of zero disables that edge's distance objective. Conflicting
lengths are allowed and approximated while preserving planarity. Increasing a
center distance can elongate growth seeds, but does not guarantee a shared-border
length or passage width. Passage fields still apply to the generated borders.

Before zone growth, embedding also validates the drawing after center positions
are rounded to tile coordinates. It retains a valid result during refinement.
If none survives rounding at the chosen resolution, generation reports an error
instead of accepting an intersecting or collapsed layout; try a larger map.
The later growth, smoothing and wall stages still determine final zone areas,
connectivity and passages.

Map width and height are selected in the application and passed to generation.
Each side must be an even number from **32 to 128 cells**. Templates without
dimension rules allow every size in that range, including rectangles.

Add an optional top-level `dimensions` object to restrict a template. For example,
this allows only square maps with sides from 64 to 128 cells:

```json
"dimensions": {
    "width": {"min": 64, "max": 128},
    "height": {"min": 64, "max": 128},
    "aspectRatio": {"min": 1.0, "max": 1.0}
}
```

Bounds are inclusive. Width and height bounds must themselves be even and within
32-128. `aspectRatio` means **width divided by height**: `0.5` is twice as tall as
it is wide, and `2.0` is twice as wide as it is tall. Use `min: 0.5, max: 2.0`
to permit both orientations up to 2:1, or `min: 1.0, max: 1.0` for squares.
Each range and each bound is optional; omitted values use the application limits
(32-128 for each side, 0.25-4.0 for width/height). Invalid or impossible combinations
are rejected when the template loads.

The UI lists only permitted even values. Choosing a side preserves that choice
and adjusts the other side only when the template's ratio requires it. Loading a
template keeps the current size if possible; otherwise it selects the closest
permitted size and shows a notice. Square-only templates link the two controls.
