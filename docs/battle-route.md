# First verified battle route

The opening checkpoint can be driven into the first encounter and battle
interface without distributing a ROM. Starting from
`artifacts/checkpoint-v1-opening/final.dbzstate`, use this deterministic input
route (starting frame, duration in frames, hexadecimal button mask):

```text
1 2 020
20 2 020
40 2 100
180 820 010
1001 2 100
1061 2 100
1121 2 100
1181 2 100
1241 2 100
1301 2 100
1361 2 100
1421 2 100
1481 2 100
1541 2 100
1601 2 100
1661 2 100
1721 2 100
1781 2 100
```

This selects **Fly**, holds Up through the map segment, advances the
Tenshinhan encounter, and reaches the battle command and animation screens.
The 1,800-frame replay was checked against the reference interpreter with
state, video, and audio hashes matching on every frame. The route is therefore
an integration target for native battle-logic reconstruction.

To reproduce this check from reset, without an existing checkpoint:

```sh
python3 tests/test_battle.py build/dbz-port "$DBZ_ROM"
```

The test creates the frame-3000 opening checkpoint using `tests/start-game.inputs`,
then resumes for 1,800 frames using `research/scout/battle-route.inputs`.
Both stages compare every frame against the reference. Temporary checkpoints
and traces are removed when the test finishes. It also checks that the actor-slot
update at `$00:98D5` was reached; this does not establish full-game coverage.

Configure with `-DDBZ_EXTENDED_TESTS=ON` to include `battle_equivalence` in CTest.
