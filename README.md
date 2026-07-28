# Creatures — Procedural Creature Lab

A from-scratch C++/OpenGL lab for generating, animating, and simulating procedural creatures. Not a game — there are no levels, objectives, or story. The point is the engineering underneath: procedural generation from a flat parameter set, a "follow-the-leader" animation chain, per-leg IK with terrain adaptation, and genetic crossbreeding between creatures — the same building blocks used in real creature-generation tools, built without an engine (no Unity/Unreal/Godot) so every layer is visible and owned.

Inspired by [*Critter Crosser*](https://rujik.itch.io/critter-crosser) (GameMaker + hand-written GLSL): a creature's DNA is a flat struct of numbers (body length, leg count, horn size, ...), not an interpreted "genome language" or an abstract morphology graph.

## Highlights

- **DNA-driven generation**: a flat, seeded parameter struct (not a genome DSL) deterministically produces the same creature from the same seed
- **Fixed quadruped skeleton**: Pelvis → Spine → Neck/Head, Tail, 4 legs — generated procedurally from DNA, not hand-authored per creature
- **Procedural mesh**: capsule/cylinder geometry built per bone, deliberately low-poly, no external 3D models
- **Follow-the-leader animation**: neck/tail/body motion propagates segment-to-segment with a delay, centipede-style, instead of a full per-segment IK solver
- **Leg IK + terrain adaptation**: raycast per leg → foot placement → FABRIK/CCD → pelvis/spine/neck adjustment, the standard industry pattern for grounding a walk cycle on uneven terrain
- **Genetic crossbreeding**: two creatures' DNA blend via parameter interpolation, discrete-trait inheritance, and mutation to produce offspring
- **Pixel-shaded low-poly look**: real 3D geometry with reduced detail + custom GLSL shading, not sprites, no PBR

<details>
<summary>Resumen en español 🇪🇸</summary>

<br>

**Creatures** es un laboratorio técnico en C++/OpenGL para generar, animar y simular criaturas procedurales — no es un juego. El ADN de cada criatura es un struct plano de parámetros (no un lenguaje de genoma), a partir del cual se genera un esqueleto de cuadrúpedo fijo, una malla procedural low-poly, animación tipo "follow the leader" para cuello/cola, IK de patas con adaptación a terreno irregular, y cruce genético entre criaturas para producir descendencia. Todo construido sin motor externo (sin Unity/Unreal/Godot), para que cada capa del pipeline sea visible y propia.

</details>

## Tech Stack

- C++20/23
- OpenGL (context via GLFW)
- GLAD (OpenGL function loader)
- GLM (math)
- Dear ImGui (debug/lab UI)
- stb_image (texture loading)
- CMake + FetchContent (no vcpkg, no system package installs)

## Docs

- Architecture and design decisions: [PROJECT_OVERVIEW.md](PROJECT_OVERVIEW.md)
- Build / run / troubleshooting: [HELP.md](HELP.md)
- Phase-by-phase roadmap: [docs/DEVELOPMENT_PLAN.md](docs/DEVELOPMENT_PLAN.md)

## Run (quick)

```
cmake -B build -S .
cmake --build build
```

See [HELP.md](HELP.md) for toolchain setup and troubleshooting.

## License

MIT — see [LICENSE](LICENSE).

## Contributing

This is a solo portfolio project, but PRs are welcome:

- Keep changes small and focused
- Follow the phase order in [docs/DEVELOPMENT_PLAN.md](docs/DEVELOPMENT_PLAN.md) — later phases assume earlier ones are done
- No AI/Claude co-author trailer on any commit

## Author

Built and maintained by [Mango77x](https://github.com/Mango77x).
