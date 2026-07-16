# Feline-PK — Dynamic Reachability Index

Incremental (dynamic) extension of Feline, per Chapter 4 of the thesis. Maintains the
Feline index (two topological orderings / 2D coordinates) as the graph changes, instead
of rebuilding it on every update.

**Status (first increment):** vertex insert/remove, edge insertion (with SCC folding),
and dynamic queries. Edge removal and batch insertion are future work.

## Building

Dynamic code is off by default. Enable it from the repository root:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DFELINE_BUILD_DYNAMIC=ON
    cmake --build build -j"$(nproc)"

## Testing

    ./build/dynamic/feline_dyn_tests

## Demo

    ./build/dynamic/dyn_demo <updates_file>

See `../docs/superpowers/specs/2026-07-15-feline-dynamic-index-design.md` for the design.

## Demo input format

One command per line:

    v <id>        insert a disconnected vertex
    e <u> <v>     insert edge (u, v)
    q <u> <v>     query reachability; prints "<u> <v> <0|1>"

## Known limitations / future work

- Cycle-closing insertions reposition the folded representative r' **locally** (thesis
  Alg. 10 lines 21–22) via bounded per-edge reorders over r''s incident E_DAG edges
  (I → r' → O), reusing the acyclic reorder machinery, instead of rebuilding the whole
  index. `DynIndex` now stores **gap-tolerant signed integer coordinates**
  (`unordered_map<vertex_t, int64_t>`) rather than a dense 0..k-1 position permutation,
  so the fold reposition is genuinely **O(affected region)**: folded members are removed
  by simply erasing their coordinates (leaving gaps — no compaction), and reorders only
  permute a set's own existing coordinate values among themselves. This matches the
  original 2014 integer-coordinate representation; no value ever needs to be created
  between two existing ones, so with `int64_t` there is no exhaustion and no relabeling.
- `append_isolated` is now **O(1)**: it hands out the next X coordinate at the high end
  (`next_x_++`) and the next Y coordinate at the front (`--next_y_`), with no Y-order
  reindexing. Repeated insertion of many disconnected vertices is therefore O(1) each.
- Not yet implemented: edge removal (Alg. 11) and batch insertion (Alg. 12–14).
