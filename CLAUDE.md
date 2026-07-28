# Contexto del Proyecto — Procedural Creature Lab

## Qué es esto

No es un juego. Es un laboratorio técnico en C++ para generar, animar y simular criaturas procedurales, sin depender de ningún engine (Unity/Unreal/Godot). El entregable final es una aplicación con visor 3D + panel ImGui donde se puede:

- generar una criatura a partir de una seed,
- ajustar sus parámetros en tiempo real y verla regenerarse,
- verla caminar y adaptarse a terreno irregular,
- cruzarla con otra criatura para producir descendencia (crossbreeding).

## Inspiración y referencia técnica real (verificada, no asumir lo contrario)

Inspirado en *Critter Crosser* (dev: RujiK). Hechos confirmados por investigación directa — **no asumir Unity/C#** ni arquitecturas más complejas de lo que realmente usa:

- Hecho en **GameMaker (GML) + shaders GLSL** escritos a mano. No usa un engine grande.
- El ADN de cada criatura es un **array/struct plano de números** (longitud de cuerpo, anchura, número de patas, ojos, orejas, etc.) — **no** un lenguaje de genoma, ni un árbol/grafo morfológico abstracto con sockets.
- El generador completo son **~300 líneas de código**.
- Cada criatura se genera **"vagamente" a partir de una única plantilla/padre**, no completamente desde cero al azar. Es variación sobre plantilla, no generación libre.
- La animación de cuerpo/cuello/cola es una **cadena "sigue al líder"**: cada segmento sigue la posición/rotación del segmento anterior con un pequeño retraso (como un ciempiés), no un solver de IK completo por segmento.
- El aspecto "2.5D" viene de **geometría 3D real con polígonos deliberadamente reducidos + shading pixel-art**, no de sprites.
- La adaptación a terreno usa el patrón estándar de la industria: raycast por pata → punto de apoyo → IK → ajuste de columna/cadera/cuello.

## Decisiones de arquitectura ya tomadas (no reabrir sin motivo fuerte)

- **ADN = struct/lista de parámetros planos.** Nada de genoma-como-programa ni node graph con sockets. Forma aproximada:
  `seed, bodyLength, bodyHeight, neckLength, tailLength, legCount, hornSize, eyeSize, earSize, bodyFat, muscle, aggressiveness, ...`
- **Variación por plantilla**: las nuevas criaturas parten de una criatura base/padre y mutan parámetros; no generación libre desde cero.
- **Animación de cuerpo/cola/cuello**: cadena "follow the leader" con retraso, antes de considerar IK completo por segmento.
- **Patas**: raycast + IK (FABRIK o CCD) + ajuste de pelvis/columna para adaptación a terreno.
- **Esqueleto inicial**: jerarquía fija tipo cuadrúpedo (Pelvis → Spine → Neck → Head / Tail; 4 patas). No generalizar a grafo arbitrario ni a otros planes corporales hasta que el cuadrúpedo funcione de punta a punta.
- **Renderizado**: geometría simple por hueso (cápsulas/cilindros/esferas) + reducción deliberada de detalle + shader pixel-art. Nada de PBR ni sombras dinámicas complejas.

## Stack técnico

- C++20/23
- OpenGL (contexto vía GLFW)
- GLM (matemáticas)
- Dear ImGui (UI de debug/laboratorio)
- stb_image (carga de texturas si hace falta)
- Build: CMake

## Perfil del desarrollador (para calibrar explicaciones)

- Primera vez programando gráficos por código / usando una API gráfica de bajo nivel (OpenGL). No asumir conocimiento previo de VAOs/VBOs/shaders/matrices de proyección — explicar brevemente la primera vez que aparezca cada concepto nuevo.
- Experiencia previa sólida en Java, GDScript, C#, Ruby on Rails, React; cómodo con Linux, Git, CI/CD — no hace falta explicar control de versiones ni conceptos generales de programación.
- Dedicación: 20h+ semanales.
- Trabaja apoyado en Claude Code como copiloto constante. No teme la curva de aprendizaje, pero prefiere explicaciones claras la primera vez que se introduce un concepto nuevo de gráficos o matemáticas 3D, sin alargarse más de lo necesario.

## No-objetivos (explícitamente fuera de alcance)

- No es un juego con niveles, objetivos o historia.
- No usar Unity, Unreal, Godot ni ningún engine externo.
- No implementar un "lenguaje de genoma" interpretado ni body plans abstractos con sockets — se evaluó y se descartó tras investigar el proyecto real de referencia.
- No perseguir realismo PBR en el render.
- No generalizar a anatomías arbitrarias (arañas, aves, insectos) antes de tener un cuadrúpedo funcionando de punta a punta.

## Roadmap (fases)

1. Ventana OpenGL + contexto + cámara orbital básica + ImGui inicializado.
2. Sistema de ADN: struct/array de parámetros + seed + generación pseudoaleatoria reproducible.
3. Generador de esqueleto: jerarquía fija de cuadrúpedo a partir del ADN.
4. Generador de malla: geometría procedural simple por hueso (cápsulas/cilindros), unión de piezas.
5. Shaders: pixel shading + reducción de detalle para el look "orgánico low-poly".
6. Animación procedural: respiración, cadena follow-the-leader para cuello/cola, mirada de cabeza hacia un objetivo.
7. Inverse Kinematics de patas (FABRIK o CCD) + ciclo de marcha (paso/trote) independiente del número de patas.
8. Adaptación a terreno: raycast por pata + ajuste de pelvis/columna/cuello.
9. Cruce genético: mezcla de dos ADN (interpolación de parámetros + herencia de rasgos discretos) + mutación aleatoria.
10. Exportación (glTF/OBJ) + guardado/carga de ADN + historial genealógico (opcional).

## Fase actual

**Fase 1 — sin empezar.** Objetivo inmediato: ventana OpenGL funcionando con geometría básica en pantalla y cámara orbital controlable con ratón.

## Convenciones de trabajo con Claude Code

- Avanzar fase por fase, sin adelantar trabajo de fases posteriores.
- Cada fase debe terminar en algo visible/ejecutable, no solo código sin probar.
- Explicar brevemente cualquier concepto de gráficos 3D/OpenGL la primera vez que aparece en una fase.
- Preguntar antes de introducir dependencias nuevas que no estén en el stack técnico de este documento.
