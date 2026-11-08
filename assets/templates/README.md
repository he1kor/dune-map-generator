`habbanya erg true 2.json` is the current template. CMake copies it to `build/src`
when configuring the application; edit this source copy to retain changes.

### Template editor

Open the **Template editor** tab to create a template or load an existing JSON.
**Settings** edits the name, wall material, one to three noise octaves,
and optional map dimension limits. The **File** menu contains New, Open, Save,
and Save as. An asterisk marks unsaved changes. New, Open, and closing the app offer to save
your work before continuing.

- Double-click empty canvas to add a square node; drag a node to move it.
- Click a node, hold Shift, and click another to connect them. The cursor and
  edge icons select **Selection** (`V`) and **Edge** (`E`) modes; Shift temporarily
  activates Edge mode. The source stays selected after connecting; keep holding
  Shift to connect it to more zones. Self-connections and duplicates are ignored.
- Ctrl-click adds or removes individual zones from the selection. Drag empty
  space to select a box of zones; Ctrl adds to the current selection. Drag any
  selected zone to move the entire group. Hold Ctrl during movement to snap the
  grabbed zone's center to a 64-unit grid cross or a cell center halfway between
  crosses, while keeping the group's spacing. Cell centers are marked with dots.
- Double-click a node to edit its ID, radius, susceptibility, octave weights,
  and resource thresholds. Renaming an ID updates all connected rules.
- Resource intervals color the slightly rounded square from bottom (threshold 0) to top (1).
  **Split row** adds an interval; removing a row extends its touching neighbor.
  Larger radii produce larger squares. `S` on each node is its susceptibility.
- Double-click a connection to edit shared passage, wall, blend and optional
  geometry settings. Both directed rules appear separately as `A -> B` and
  `B -> A`, and can be enabled independently.
- Connections repeat black min-wall, transparent min-pass, black max-wall,
  transparent max-pass segments. Each length unit uses two screen pixels and
  the black stroke uses 3.5 pixels, independent of zoom and generation resolution.
- Use the mouse wheel to zoom about the pointer; drag with the middle or right
  mouse button to pan. **Fit** (or `F` / `Home` over the canvas) frames all nodes.
  Right-click opens canvas actions; `Delete` removes the selected item and its
  dependent rules. `Esc` cancels a pending connection.
- **Auto place** creates a compact diagram with a rectangular outer boundary,
  symmetric rows/columns and horizontal, vertical or 45-degree connections where
  possible. A uniform harmonic layout preserves grid symmetries; the generator's
  certified planar construction is retained for irregular topologies. Branches
  use the private drawing scaffold instead of collapsing onto their parent.
  Clearance-aware compaction shortens long edges before grid snapping.
  It checks for node overlaps, edge crossings, and edges passing through unrelated
  nodes after snapping. Connections leave room for one complete dotted pattern
  at 100% zoom, without the previous large clearance halos. The initial camera
  fits the entire graph into view. Only visualizer
  positions change, and the whole placement is one undo step.
- A **!** icon in the top menu warns when the graph topology is non-planar.
  Hover or click it for an explanation and the offending connections. Auto place
  is disabled until the topology is fixed. Moving nodes cannot fix non-planarity;
  crossed lines in a manual drawing alone do not indicate non-planar topology.

**Edit** contains Undo/Redo and selection actions. All document edits, including
new/open, can be undone; a group drag or settings Apply is one history step.
Inside a settings menu, Undo/Redo also restores individual field edits,
checkboxes and resource-row changes before Apply.
Saving retains history. Returning to the saved values clears the unsaved marker.
Undo/redo notifications stack and show every changed value as `from -> to`;
their bodies let mouse input pass through to controls below. Click **x** to
dismiss a message, or let it fade after four seconds (the last 0.4 seconds fade
out). Hover does not extend its lifetime. Camera navigation and
selection do not edit the document or add history entries.

**Copy data** (`Ctrl+C`) copies the last clicked zone's settings or the selected
edge's shared settings and both directed rules. **Paste data** (`Ctrl+V`) applies
node data to all selected zones, or edge data to the selected edge. Both actions
are available in Edit and the right-click menu. Destination IDs, canvas positions,
and connections stay unchanged. Edge directions map first-to-second and
second-to-first; an absent source direction clears the corresponding destination
rule. Invalid or incompatible data is rejected without changing the document.
One paste to multiple zones is one undo step. Input boxes retain normal text copy/paste.
Canvas coordinates are rounded to two decimal places on load and movement.

Descriptions and the Help content live in
[`assets/ui/template_editor_descriptions.json`](../ui/template_editor_descriptions.json).
See [`assets/ui/README.md`](../ui/README.md) for runtime overrides.
That JSON also defines material display names, such as **Cliff rock** and
**Thick spice**; saved templates retain keys such as `cliff_rock` and `thick_spice`.

Settings menus have labeled input boxes and `(?)` explanations on each parameter.
Unchecked single-value overrides keep their input visible and disabled, showing
the default value (or Automatic for radius-derived center distances).
Enter applies the menu. Escape, Cancel, and the close icon ask before discarding
changed settings. **Help** (`F2`) explains the controls and template concepts.

| Shortcut | Action |
| --- | --- |
| Ctrl+N / Ctrl+O | New template / Open JSON |
| Ctrl+S / Ctrl+Shift+S | Save / Save as |
| Ctrl+Z | Undo |
| Ctrl+Y / Ctrl+Shift+Z | Redo |
| Ctrl+A | Select all zones on the canvas |
| Ctrl+comma | Global settings |
| Ctrl+Enter | Use for generation |
| Enter / Delete | Edit / delete selection on the canvas |
| V / E / hold Shift | Selection mode / Edge mode / temporary Edge mode |
| F / Home | Fit graph |
| F2 | Help |

Each node stores its canvas center in optional metadata alongside `id` and `data`:

```json
"visualizer": {"x": 200.0, "y": -80.0}
```

These coordinates affect only the editor. Generation ignores them, even when
the drawn edges cross. Templates without coordinates get an initial grid in
the editor; saving records that layout. Bundled templates include saved layouts.
Editing retains other optional fields and unknown extension metadata.

Drafts with empty, isolated or nonplanar graphs can be saved and reopened.
Field values and resource interval coverage must still be valid. **Use for
generation** checks the full generator requirements, selects a separate snapshot,
and applies its dimension limits. Switch to **Generator** and click **Generate**.
Later editor changes take effect when you click **Use for generation** again.
Saving a runtime JSON in `build/src` does not update its source in
`assets/templates`; use Save as into that folder to retain edits across CMake
configuration.
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

### Per-zone position magnets (editor)

The editor's Magnet mode (M, or hold Alt) toggles a position magnet with a
single click. Drag a zone outward to increase force, or grab it off-center and
drag toward its center to reduce force. Force ranges from 0.1 to 100; its minimum
does not disable it. Node settings also expose the toggle and force. Red/blue
contours glow more strongly as force increases. Each gesture is undoable.

In Magnet mode the wheel changes the centered square map frame's scale. Its
minimum fits all zone squares, including non-magnets; it can expand but cannot
shrink below that fit. Fit frame resets its scale. Ctrl+wheel zooms the camera.
Adding, moving, resizing and auto-placing zones update the frame and targets.

An enabled node saves `data.magnet` with a positive `force` and a `position`
object containing normalized `x`/`y` coordinates: top left is (0, 0), center is
(0.5, 0.5), bottom right is (1, 1). The frame saves its `scale`, derived `center`
and `halfExtent` under the root `visualizer.magnetFrame`. Targets are recomputed
for destination zones when pasting node data. Camera navigation does not change
them. Any target outside the unit square is ineligible for application.

The generator loads these targets and uses them during initialization and
relaxation. Initialization checks rotated/reflected planar candidates and can
place magnetized leaves in their intended face before continuous movement
begins. Subsequent positional attraction is independent of central-magnet
susceptibility; the shared central magnet remains active. High strength holds
the node closer to its target, using a stable spring step, but is not a lock.
Map bounds and crossings still constrain movement. Targets at map edges are
inset by half a tile so they can rasterize safely. Outside targets are ignored.
Magnetized zones also retain radius-based spacing from neighboring seeds so
central repulsion cannot crowd another seed onto their target. This protects
placement space; it does not impose exact zone areas or guarantee every passage.

Enable **Random magnet rotation** in Global settings > Position magnets, or add
this optional top-level setting to the template:

```json
"magnets": {"randomRotation": true}
```

The seed chooses one common rotation around the map center for every active
position magnet. Forces, pairwise distances and distances to the center stay
unchanged. Rotation is measured in map cells, including on rectangular maps.
The saved targets and editor drawing stay in their authored orientation. The
same seed and map size reproduce the same rotation; it does not consume the
growth/resource random stream. Omitted or `false` keeps the fixed orientation.

Angles across the full circle are considered, but the entire target group must
fit inside the map without scaling or clipping. Near-corner arrangements allow
fewer angles; exact corner targets can limit square maps to quarter-turns and
rectangles to half-turns. Enlarge the editor map frame or move targets toward its
center to allow more rotation. An already outside/inactive target stays inactive.

The generator reads saved `data.magnet.position`, not raw visualizer coordinates
or the editor frame. With no active positional magnets, the previous seeded
layout behavior is preserved. Zone growth, resource noise and passage rules are
unchanged. After editing a selected template, use **Use for generation** again
to update the generator's snapshot.

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
