# First verified battle route

The opening checkpoint can be driven into the first encounter and battle
interface without distributing a ROM. Starting from
`artifacts/checkpoint-v1-opening/final.dbzstate`, use this deterministic input
route (frame, buttons, duration):

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
the next integration target for native battle-logic reconstruction.
