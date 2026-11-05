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
