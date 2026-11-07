`habbanya erg true 2.json` is the current template. CMake copies it to `build/src`
when configuring the application; edit this source copy to retain changes.

`grid3x3 magnetic.json` is a small magnetic-placement example, also copied to
`build/src`. It has nine zones with radius `1.0` and twelve connections arranged as a
3×3 grid graph (IDs 0–8 in row order). Node 4 is rock; the others are sand.
The graph starts from seeded random positions, so its connections do not impose
fixed grid coordinates.

Every node has `susceptibility: 0.02`, which attracts it toward the application's
central magnet. Set this to `0.0` for a neutral comparison or `-0.02` for repulsion;
use the same seed for each comparison. Seed `12419` is used in the regression
tests. Magnetic forces begin after the first twenty embedding steps.

Repulsion can push several nodes onto the same boundary cell. The current
rasterizer overwrites colliding nodes, which can make zone expansion fail. This
occurs with this graph at seed `2026` and susceptibility `-0.02`; attraction and
neutral placement pass for that seed. The default positive example avoids this
case in the tested seeds (`12419`, `2026`, `42`).

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
values must be non-negative. IDs, distances, areas, and passage dimensions must
be non-negative whole numbers within the C++ `int` range. Passage and wall
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

The current force-directed embedding uses the same spring target for every
connection: `sqrt(width * height / (3.5 * numberOfZones))`. This sets a global
spacing scale, not a guaranteed distance: node repulsion, border repulsion,
magnetic forces and the fixed iteration budget also affect final positions.
Embedding does not read node radii or connection parameters.
The active `stepForceDirected()` path does not call the existing
`applyEdgeRepulse()` helper and has no constraint preventing edge crossings.

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
| `passes.minPassWidth`, `maxPassWidth` | Passage widths on the generated zone border |
| `passes.minWallLength`, `maxWallLength` | Wall lengths between passages |
| `resourceBlend.distance` | Resource blending across a zone border |
| `intakeDistance`, `areaGuaranteed`, `bonusValue` | Directed resource influence and guarantees during blending |

There is currently no per-connection target distance or required total shared
border length. A longer passage request does not stretch either zone during
embedding or growth; it is applied when passages are generated on existing
borders.

When embedding finishes, intersecting straight graph edges are listed in a
console warning before rasterization and zone growth. This is diagnostic only;
generation continues. See GenCore's embedding diagnostics for the query API.

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
