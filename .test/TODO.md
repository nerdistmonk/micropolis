# Devworks resume notes

Branch: devworks (repo /tmp/mpdev). Latest commit: f6ef0cc (regenerated 8 scenarios).
Rules: devworks may add features. plus is bug-fix only. main is vanilla, do not touch.
Commit and push only when the user asks. Use the Co-authored-by trailer.

## Done (pushed)
- b93a8f1 removed snro scenarios. plus 0ca3364.
- 0ef3544 v2 .scn format (pipe + landfill blocks), cumulative water cap.
- 72c0d2c pooled power grid (union-find), water network stats, inspector supply row.
- 5f53a1f 18-zone water towers, pipe-to-river source (no 4-tile reach), half water when unpowered.
- f6ef0cc regenerated all 8 scenarios with pooled power, plants added where needed.

## Current scenario status (out8 = committed f6ef0cc)
| Scenario | towers | centers | unsupplied | short grids |
|---|---|---|---|---|
| Dullsville | 1 | 28/28 | 0 | 0 (clean) |
| Tokyo | 30 | 305/305 | 0 | 3 (user chose to leave) |
| Bern | 29 | 194/194 | 0 | 5 |
| Boston | 19 | 262/303 | 41 | 4 |
| Detroit | 51 | 271/287 | 16 | 4 |
| Hamburg | 15 | 137/282 | 145 | 5 |
| Rio_de_Janeiro | 12 | 112/397 | 286 | 9 |
| San_Francisco | 17 | 262/272 | 10 | 2 |

## Next steps (in order)
1. Rio de Janeiro: 286 unsupplied zones, 9 short grids. Investigate why towers
   cannot cover it (water source / pipe reach / tower sites) and why plants are not added.
2. Hamburg (145 unsupplied), Boston (41), Detroit (16), San Francisco (10), Bern (5 short grids).
3. Verify with scen2 over ~40k cycles: population must not collapse, grids_short=0, unpowered_zones=0.
   Tokyo short grids are accepted by user; still must not regress.
4. Only after user approval: copy regenerated scenarios to /tmp/mpdev/scenarios, build
   (cd /tmp/mpdev/build && ninja), commit, push devworks.

## Open questions / unverified
- Start funds per scenario. The generator uses GEN_UNLIMITED=1 so funds are hidden.
  deduct=1 fails Scenario_Validate (core/s_scenario.c ~95: def.funds < 0). Print def.funds before deducting.
- Islands walled by rail junctions and road corners: LayWire (core/s_con.c:546-590) cannot cross them.
  User chose to leave Tokyo's 3 short grids as-is.
- 48-zone nuclear path (nuc.c) segfaulted at 42 zones in the harness. Not resolved.
- Wires/pipes through roads are valid in code but not verified in-game.
- Lake and ocean sources only reasoned from tile IDs 2-20 (IsWaterSource). Not tested in a live map.
- Plant tiles are sinks for water (do not conduct). Behaviour change to report.
- Power conducts through zones (s_sim.c:1013 sets CONDBIT on zones; s_power.c IsConductiveTile).

## Tooling (in .test/)
- harness/gen_scn2.c: scenario generator (scratch, not in repo).
  Build:
    O=$(ls /tmp/mpdev/build/CMakeFiles/micropolis_sdl3.dir/core/*.o)
    gcc -std=gnu11 -O1 -I/tmp/mpdev/core/headers -I/tmp/mpdev/core -o gen_scn2 gen_scn2.c $O -lm
  Run (dir needs a scenarios/ subfolder):
    mkdir -p outN/scenarios && GEN_UNLIMITED=1 GEN_DEBUG=1 ./gen_scn2 /tmp/mpdev/scenarios/X.scn outN/ 0
- harness/edgechk.c, wdiag.c, scen2.c, scen3.c, scen4.c: checks.
  They need AllocateMapBuffers() and an Audio_MakeSound stub. Run Simulate 3000 steps before checking.
- summary8.txt: one-line result per scenario for the committed set.
- Generator runs for all 8 scenarios take about 10 minutes total.
- Note: some edits were made in /tmp/lf-test; .test/harness holds the copies as of this note.
