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
