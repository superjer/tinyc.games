# Chunk Size Optimization Plan

Goal: find the right chunk size under Vulkan, and if small chunks are better
for generation/editing but worse for rendering, decouple the two with draw
batching.

## Background

Chunks are currently 64x64 blocks x full height (CHUNKW/CHUNKD x TILESH), in
a 16x16 sliding window (VAOW/VAOD). Chunk size was increased over time because
bigger chunks rendered faster — but that experience largely predates the
Vulkan port. In GL, every draw call carried real driver overhead; in Vulkan a
recorded vkCmdDraw + push constant costs microseconds. The remaining
per-chunk costs are CPU-side: frustum tests (camera + up to 6 shadow
cascades), the visible-list sort, LOD checks, and mesh rebuild granularity.

Big chunks hurt where it's most visible now:
- a scoot strip regenerates 16 x (64x160x64) chunks — seconds of pop-in
- one block edit remeshes 64x160x64 = ~650K block scan
- one mesh upload per frame (MAX_MESHES_PER_FRAME) means multi-frame latency
- exploration p99 frame time measured at ~33ms vs 16.6ms vsync budget
  (baseline 2026-07: p50 16.6ms, p99 33.4ms while running forward)

## Step 1: measure smaller chunks (cheap)

Config for a 32-block-chunk build (keep the window at 1024x1024 blocks):

    CHUNKW 32, CHUNKD 32
    VAOW 32, VAOD 32
    VERTEX_BUFLEN — scale with chunk area, e.g. (CHUNKW*CHUNKD*32)
                    (= 131072 at 64², same as today; 32768 at 32²)
    MAX_MESHES_PER_FRAME 4 (fairness: 4x more, 4x smaller meshes)

Why VERTEX_BUFLEN must scale: every chunk gets a fixed-size GPU buffer of
sizeof(vbuf) ≈ 60 bytes x VERTEX_BUFLEN. At 64² chunks that is ~7.9MB x 256
buffers ≈ 2GB host-visible memory(!). Scaling with area keeps the total
constant. (This fixed-slot scheme is itself worth revisiting someday.)

### Metrics (via debug socket, tools/bk)

    bk fps reset / bk fps        frame count, avg, p50, p99, worst
    bk timings reset / bk timings  per-subsystem CPU time deltas
    bk tp <ax> <az>              teleport (absolute block coords)
    bk walk <frames>             hold run-forward for N frames

Scenarios to compare per config:
1. standing still, generated area (steady-state render cost)
2. `walk 1200` through ungenerated terrain (gen + mesh + scoot stress)
3. timings: draw_terrain, shadow_render, build_meshes, sync_w_terrain_gen

Caveat: vsync pins fps at 60, so average fps won't differentiate configs
until they're slower than 60. Use p99/worst (dropped-frame spikes) and the
CPU section timings. For pure render throughput, temporarily switch the
swapchain present mode to IMMEDIATE (vulkan layer) — worth adding a
`-DNO_VSYNC` toggle if needed.

Decision: if 32 (or 16) chunks show equal-or-better steady-state render cost
and better exploration p99, just switch — no batching layer needed.

### RESULTS (2026-07-03, Iris Xe laptop, dist 1024, fly 30 blocks/s)

    standing        64-chunk: 59.7fps p99 22.4ms | 32-chunk: 58.5fps p99 25.8ms
    flying 20s      64-chunk: 47.6fps p50 17.9ms | 32-chunk: 37.7fps p50 26.1ms
    build_meshes    64: 30% of frame time        | 32: 39%
    draw_terrain    64: 0.84s CPU                | 32: 1.75s
    shadow_render   64: 0.81s                    | 32: 1.46s

64 wins everywhere, decisively in traversal. (Bias note: fly moves per-frame,
so the slower config traversed less terrain in the window — 32 lost anyway.)
Per-chunk fixed costs dominate at 32: 4x chunks x 7 frustum tests, the
visible-list sort, per-rebuild OpenMP dispatch, 4x draw calls in main +
shadow passes. Verdict: KEEP 64. Step 2 (MDI) would fix the draw/cull side
but not build_meshes, which is the real traversal bottleneck.

### Next lead: redundant remeshes from light updates

build_meshes is ~30% of traversal frame time even at 64. Each set_sunlight
marks up to 4 chunks dirty; during a strip's initial light BFS the same chunk
gets remeshed many times (~6ms per 64-chunk rebuild). Ideas: don't dirty
chunks from light changes while the emitting chunk is still settling, or
defer remesh until a chunk has been clean of light updates for N frames.

## Step 2: multi-draw indirect (only if small chunks regress rendering)

Decouple mesh granularity from draw-call count:

- All sub-chunk meshes live in ONE big vertex buffer, each sub-chunk owning a
  fixed-budget region (same fixed-slot philosophy as today, smaller slots).
- CPU culling builds an array of VkDrawIndirectCommand (vertexCount,
  firstVertex, instanceCount=1, firstInstance=i) per visible sub-chunk,
  plus a parallel per-draw buffer of chunk origins.
- The per-chunk push constant becomes an instanced vertex attribute indexed
  by firstInstance — works everywhere, no shaderDrawParameters needed.
- Each pass (main, water, shadow cascades) becomes a single
  vkCmdDrawIndirect(..., drawCount=N).

Touches: glsetup.c (buffers, vertex input layout), mesh.c (upload targets),
draw.c + shadow.c (draw recording), main.vert (origin attribute replaces
push constant chunk_x/y/z).

## Fallback hybrid

Keep 64x64 render chunks but mesh in 16x16 sub-cells writing into per-cell
budget regions of the chunk's buffer (a few vkCmdDraws per chunk). Fixes
edit-remesh latency only; generation granularity unchanged.

## Harness

remote.c is a unix-socket debug/automation interface (default
/tmp/blocko.sock, override with BLOCKO_SOCK). One command per connection.
tools/bk is the client. This is also the seed of the future game-state API:
add read/manipulate commands to remote_reply()'s dispatch.
