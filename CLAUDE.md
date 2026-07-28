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
- La adaptación a terreno usa el patrón estándar de la industria: raycast por pata → punto de apoyo → IK → ajuste de columna/cadera/cuello.
- El aspecto "2.5D" viene de **geometría 3D real con polígonos deliberadamente reducidos + shading pixel-art**, no de sprites. **Confirmado con fuentes primarias citables (28/07/2026)**, tras una corrección y contra-corrección propia — ver nota de metodología abajo:
  - Entrevista al desarrollador en el blog oficial de GameMaker: *"GameMaker is primarily a 2D game engine so 3D requires you to do most of the heavy lifting yourself"* — [gamemaker.io/en/blog/socket-beast-rujik](https://gamemaker.io/en/blog/socket-beast-rujik). Misma entrevista confirma también, con cita textual, lo del ADN como array plano de números y el crossbreeding como lerp entre dos arrays.
  - Devlog oficial #5, título **"Rendering Organic Monsters"**, descripción: *"mediante la eliminación de polígonos y la actualización de animaciones procedimentales"* — [youtube.com/watch?v=T2oUOWNNnx4](https://www.youtube.com/watch?v=T2oUOWNNnx4). "Eliminación de polígonos" solo tiene sentido si hay una malla 3D real.
  - **Nota de metodología — por qué esto importa**: una revisión visual previa (solo mirando 7 capturas estáticas de Steam) concluyó erróneamente que era arte 2D de sprites, porque una malla 3D low-poly renderizada con cámara/ángulo fijo + shader de pixelado puede verse indistinguible de sprites 2D dibujados a mano en una imagen estática — ese es justo el efecto buscado. **Lección: para afirmar cómo está construido algo (3D vs 2D, técnica de render), no basta con mirar capturas — hay que buscar declaraciones directas del desarrollador (entrevistas, devlogs, código) antes de dar algo por "confirmado".**

## Referencia visual (Critter Crosser)

Capturas reales descargadas de la ficha de Steam (`store.steampowered.com/app/2792320/Critter_Crosser/`) el 28/07/2026, guardadas en `reference/critter-crosser/` (carpeta ignorada en git — solo para consulta local, nunca se commitea). Sirven para el mood/paleta/iluminación/composición — no para inferir la técnica de render (ver nota de metodología arriba: eso se confirma con fuentes, no mirando capturas):

- **Perspectiva**: oblicua/dimétrica fija tipo Pokémon/Stardew Valley (edificios muestran tejado + paredes a la vez), no una cámara 3D libre.
- **Paleta**: saturada pero no neón; cada superficie (hierba, agua, ladrillo) usa ~3-4 bandas de tono planas, sin degradados suaves.
- **Densidad de píxel**: pixel art "gordo"/de baja resolución deliberada, tamaño de píxel consistente en todos los elementos (nada de mezclar resoluciones).
- **Criaturas/personajes**: pequeños en pantalla (~1-1.5 tiles de alto), formas planas de color con sombreado interno simple (luz/tono medio/sombra), sin contorno negro grueso tipo cartoon — la silueta se lee por el sombreado, no por el contorno.
- **Iluminación**: dirección de luz consistente (arriba-izquierda) horneada en cada sprite; encima se superponen efectos dinámicos vía shader (lluvia + un cono de luz cálida siguiendo al jugador de noche, agua con ripple/dither animado, partículas de salpicadura) — coherente con "GameMaker + shaders GLSL escritos a mano".
- **Tono/ambientación**: mundo mundano y reconocible (casas de ladrillo, aceras, coches, comisaría, una tienda llamada "The CompPost") poblado de criaturas fantásticas — el contraste "monstruos en la vida cotidiana" es el gancho visual, no un mundo de fantasía estilizado.
- **UI**: paneles con fuente pixel gruesa, bordes celestes gruesos, barras de vida segmentadas, texto con sombra — heredero directo del estilo Pokémon GBA pero más limpio.

**Regla de trabajo**: antes de implementar o generar cualquier elemento visual (paleta de color, shader, geometría/silueta de criatura, UI de ImGui con intención estética), revisar las capturas en `reference/critter-crosser/` para contrastar contra este análisis en vez de improvisar de memoria.

## Decisiones de arquitectura ya tomadas (no reabrir sin motivo fuerte)

- **ADN = struct/lista de parámetros planos.** Nada de genoma-como-programa ni node graph con sockets. Forma aproximada:
  `seed, bodyLength, bodyHeight, neckLength, tailLength, legCount, hornSize, eyeSize, earSize, bodyFat, muscle, aggressiveness, ...`
- **Variación por plantilla**: las nuevas criaturas parten de una criatura base/padre y mutan parámetros; no generación libre desde cero.
- **Animación de cuerpo/cola/cuello**: cadena "follow the leader" con retraso, antes de considerar IK completo por segmento.
- **Patas**: raycast + IK (FABRIK o CCD) + ajuste de pelvis/columna para adaptación a terreno.
- **Esqueleto inicial**: jerarquía fija tipo cuadrúpedo (Pelvis → Spine → Neck → Head / Tail; 4 patas). No generalizar a grafo arbitrario ni a otros planes corporales hasta que el cuadrúpedo funcione de punta a punta.
- **Renderizado**: geometría simple por hueso (cápsulas/cilindros/esferas) + reducción deliberada de detalle + shader pixel-art. Nada de PBR ni sombras dinámicas complejas.
- **Cámara libre orbital, con zoom, proyección en perspectiva** (decisión final revisada 28/07/2026, Fase 9): la cámara pasó por varias iteraciones — fija/oblicua (Fase 5) → ortográfica sin zoom (primera pasada de Fase 9, para imitar la proyección paralela confirmada en las capturas de Critter Crosser) → **de vuelta a libre + perspectiva + zoom**, por decisión explícita del usuario de separarse del estilo de cámara de Critter Crosser y tratar esto como una herramienta de laboratorio, no una recreación visual 1:1. Arrastrar con el botón **izquierdo** orbita (yaw/pitch), scroll hace zoom (distancia). El botón derecho queda reservado para mover a la criatura (mantener pulsado = sigue el ratón; sin pulsar, la criatura no se mueve), así que no chocan.
- **Terreno con colinas suaves y pronunciadas, no terrazas de bloques** (decisión final revisada 28/07/2026, Fase 9): las terrazas de bloques (con paredes verticales bruscas) causaban clipping constante — el cuerpo no evita las paredes, solo sigue la altura media de las 4 patas, así que un salto de altura brusco entre celdas vecinas hacía que el IK se estirara más allá de su alcance. Sustituido por un heightfield continuo de colinas/depresiones deliberadas (suma de caídas gaussianas en `Terrain.cpp`, no terrazas ni ondas repetitivas) — pendiente siempre suave en cualquier punto, pero con elevación claramente visible, para poder probar bien la adaptación a terreno. `TerrainHeight` mantiene la misma firma, así que el raycast por pata no cambió.
- **Escala global de la criatura** (`kCreatureScale` en `Skeleton.h`, Fase 9): tras notar que la criatura ocupaba varios bloques de terreno en vez de ~1 como en la referencia, se introdujo un único factor de escala aplicado al esqueleto (posiciones de joints) y a los radios de malla, en vez de retocar cada rango de ADN por separado — mantiene toda la variación/proporciones relativas intactas, solo cambia el tamaño global. Velocidad de marcha, zancada y desplazamientos de animación idle se escalan con la misma constante para seguir siendo proporcionales.
- **Cinemática de columna como cadena de ángulos con retraso, no un único ángulo global** (Fase 9, objetivo explícito del usuario: "simular las kinemáticas de un cuerpo de la manera más realista posible"): la columna, cuello y cola se doblan de verdad al girar (no solo cambian de grosor). Reglas aprendidas iterando con el usuario, todas en `Animation.cpp`:
  - El giro real del cuerpo (`bodyYaw` en `main.cpp`) tiene una velocidad angular máxima (`kMaxTurnRate`) — sin esto, si el objetivo cambia de dirección más rápido de lo que la cadena puede seguir, el hueco entre pecho y caderas crece sin límite y la columna se enrolla sobre sí misma. Límite duro adicional por articulación (`kMaxJointBendAngle`) como red de seguridad.
  - Cada eslabón de la columna persigue al de delante (no al pecho directamente) a su propio ritmo, cada vez más lento hacia las caderas — una ola que se propaga, no un ángulo aplicado de golpe.
  - Cuello y cabeza usan un ángulo **propio y más rápido** que el del pecho (no encadenado a su ritmo) — en un animal real la cabeza decide hacia dónde ir antes de que el cuerpo la siga; encadenar el retraso del cuello al del pecho lo hacía doblemente lento.
  - Las posiciones de `NeckEnd`/`HeadTip` se recalculan **frescas cada frame** ancladas al ángulo con retraso (que ya tiene su propia suavidad) — nunca con su propio `ExpLerp` de posición independiente. Dos extremos de una misma cápsula persiguiendo objetivos a ritmos distintos hace que la cápsula se estire/tuerza en vez de comportarse como una pieza rígida; ese fue el bug real detrás de "la cabeza bambolea".
  - Cuernos/orejas/ojos rotan (no solo trasladan) con el mismo ángulo que la cabeza — una traslación pura solo aproxima bien un giro pequeño; con giros de decenas de grados se despegan visiblemente de la superficie de la cabeza.
  - La cola hereda "peso": caída constante (no solo oscilación) y un balanceo propio más lento que las caderas, ya que es continuación de la columna, no un muñón rígido.
  - Las patas delanteras cuelgan del pecho ya doblado (mismo ángulo), y su objetivo de marcha se calcula sobre esa MISMA base doblada — si el objetivo se calcula sin el doblez, el IK se estira más allá de su alcance y la pata "vuela" sin tocar el suelo. Un solo transform (`bodyTransform`, basado en la orientación trasera con retraso) sirve para todo — nunca hizo falta un segundo transform paralelo para la mitad delantera, solo que la posición de referencia de cada pata tenga en cuenta el doblez antes de aplicarlo.
  - El IK de patas en sí (`SolveTwoBoneIK`, `main.cpp`) no se tocó en ningún momento de este trabajo.
- **Cabeza: tamaño y largo como campos de ADN propios**, desacoplados de `eyeSize` (Fase 9): `headSize` controla el radio de la cápsula de cabeza, `headLength` su longitud — antes el largo era una constante fija (0.3, no ajustable) y el radio dependía indirectamente de `eyeSize`.

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

1. Ventana OpenGL + contexto + cámara orbital básica (herramienta de desarrollo, ver decisión de cámara fija) + ImGui inicializado.
2. Sistema de ADN: struct/array de parámetros + seed + generación pseudoaleatoria reproducible.
3. Generador de esqueleto: jerarquía fija de cuadrúpedo a partir del ADN.
4. Generador de malla: geometría procedural simple por hueso (cápsulas/cilindros), unión de piezas.
5. Shaders: pixel shading + reducción de detalle para el look "orgánico low-poly" + bloquear la cámara al ángulo oblicuo/dimétrico fijo definitivo (fin de la cámara orbital libre).
6. Animación procedural: respiración, cadena follow-the-leader para cuello/cola, mirada de cabeza hacia un objetivo.
7. Inverse Kinematics de patas (FABRIK o CCD) + ciclo de marcha (paso/trote) independiente del número de patas.
8. Adaptación a terreno: raycast por pata + ajuste de pelvis/columna/cuello.
9. Variedad visual + cinemática realista: color por ADN, geometría real de cuernos/orejas/ojos, rango de proporciones más amplio, terreno con colinas propias, columna/cuello/cola que se doblan de verdad al girar. Fase abierta a propósito — no se avanza a cruce genético hasta que las criaturas se vean y se muevan bien por sí solas (decisión explícita del usuario, 28/07/2026).
10. Cruce genético: mezcla de dos ADN (interpolación de parámetros + herencia de rasgos discretos) + mutación aleatoria. **Bloqueada hasta cerrar la Fase 9.**
11. Exportación (glTF/OBJ) + guardado/carga de ADN + historial genealógico (opcional).

## Fase actual

**Fase 1 — completada y mergeada.** Ventana OpenGL + contexto, cámara orbital (arrastrar rota, scroll hace zoom), panel ImGui vacío, cubo placeholder con shading básico.

**Fase 2 — completada.** Struct de ADN plano + generación reproducible por seed (`std::mt19937`) + panel ImGui con seed/generar/seed aleatorio y listado de parámetros. Verificado: mismo seed reproduce los mismos valores.

**Fase 3 — completada.** Esqueleto de cuadrúpedo generado desde el ADN, dibujado como figura de palo de depuración (líneas + puntos) con la cámara orbital libre de desarrollo. Verificado: las proporciones cambian con cada seed.

**Fase 4 — completada.** Malla procedural (cilindros afinados por hueso + esferas en articulaciones) generada desde el esqueleto, con shader de iluminación básica. El esqueleto de depuración de la Fase 3 pasó a ser un overlay opcional (checkbox "Show skeleton (debug)"), dibujado sin test de profundidad para que no quede oculto dentro de la malla. Verificado: criatura sólida y reconocible que cambia de forma con el seed.

**Fase 5 — completada.** Cámara bloqueada al ángulo oblicuo fijo (yaw 45°/pitch 32°, solo zoom con scroll), shading por bandas planas (cel-shading, 4 escalones) en vez de degradado continuo, y pixel-art real vía render a baja resolución + reescalado `GL_NEAREST` (slider "Pixel scale" ajustable en vivo). Verificado contra las capturas reales de `reference/critter-crosser/`: sin contorno negro (coincide), sombreado por bandas (coincide), pixelado real (coincide en espíritu). Quedan pendientes para más adelante (no era alcance de esta fase): paleta multi-tono por criatura y geometría distinta para cuernos/ojos/orejas (el ADN ya trae esos campos, pero el generador de malla de la Fase 4 solo los usa para el tamaño de la cabeza).

**Fase 6 — completada.** Cadena "follow the leader" con retraso para cuello y cola (cada articulación persigue a la anterior ya retrasada, no snap directo), respiración (el radio de la columna pulsa con una onda seno), y cabeza que se inclina hacia un "look-at target" controlable en vivo desde el panel. Columna y patas se quedan fijas en su pose de reposo (el IK de patas es Fase 7). Verificado: la criatura quieta se ve viva.

**Fase 7 — completada.** Cada pata pasó a tener rodilla (Hip→Knee→Foot, 3 huesos) resuelta con un solver FABRIK real; ciclo de marcha procedural sin estado (`ComputeFootTarget`) con patrón de trote diagonal; la criatura camina en círculo alrededor del target fijo de la cámara sobre un plano de suelo visible. Todo el cálculo de IK/marcha ocurre en espacio local del esqueleto; el movimiento del cuerpo solo afecta el `uModel` de render. Sin raycast/adaptación de terreno todavía (eso es Fase 8, suelo plano por ahora). Verificado: patas dobladas de rodilla visibles, marcha tipo trote.

**Fase 8 — completada.** Terreno con relieve real (heightfield analítico, "raycast" = muestreo directo de altura), inclinación de pelvis/columna ajustada a los 4 puntos de apoyo, y IK de pata cambiado de FABRIK genérico a un solver analítico de 2 huesos (`SolveTwoBoneIK`) tras descubrir que FABRIK podía doblar la rodilla al lado incorrecto en una cadena de solo 2 segmentos. La pose de reposo de las patas ahora tiene flexión de rodilla natural permanente (más alcance total que la altura de pie, `kStandCrouchFactor`), evitando el aspecto rígido tipo pingüino. Se sustituyó el camino circular fijo por seguimiento del ratón (proyectado al suelo) con paredes de límite en los bordes del terreno. Verificado en varias iteraciones con el usuario, incluyendo un terreno diagonal a propósito para confirmar que cada pata se ajusta de forma independiente más allá de la inclinación global del cuerpo.

**Fase 9 — en curso, abierta a propósito.** Objetivo: variedad visual, no cruce genético (ese pasó a Fase 10 y está bloqueado hasta cerrar esta). Hecho hasta ahora:
- Paleta por ADN (`Palette.h/.cpp`, HSV): color base + acento (cuernos/orejas) + color de ojos, todo derivado de `bodyHue`/`accentHueShift`/`colorSaturation`/`colorValue`, transportado por vértice (`MeshVertex::color`) y combinado con `uColor` en el shader.
- Cuernos/orejas como mini cadenas de hueso reutilizando el pipeline de cilindros de las patas (no mallas nuevas); ojos como esferas de color contrastante. Corregido un bug donde ojos/orejas quedaban enterrados dentro de la esfera de la cabeza por usar un offset menor que su propio radio.
- Rango de proporciones de ADN ampliado (antes todas las seeds daban siluetas parecidas).
- Cámara pasada de perspectiva a ortográfica fija, sin zoom, encuadrando el mapa completo (`Camera::FitToGround`).
- Terreno rehecho como terrazas de bloques en vez de heightfield suave (`Terrain.cpp`).
- `kCreatureScale` para que la criatura ocupe ~1 bloque de terreno, como en la referencia, en vez de varios.
- Secciones del cuerpo elípticas (no circulares) en columna/cuello/cabeza: `AppendCylinder` ahora acepta una elipse (ancho y alto independientes) en vez de un único radio, con la normal recalculada correctamente para la elipse (no es la misma dirección que el desplazamiento, a diferencia de un círculo). Solo se aplica donde la base side/up de `PerpendicularBasis` coincide con los ejes reales izquierda-derecha/arriba-abajo del cuerpo (huesos casi horizontales); las patas se quedan circulares porque su base perpendicular no tiene ese significado anatómico.
- Paleta a 3 tonos: `BellyColor` (más clara, menos saturada) se aplica a la mitad inferior de columna/cuello/cola, con transición suave de un segmento (interpolación de color por vértice), imitando el vientre más pálido típico de la referencia.
- Panel ImGui editable en vivo: los campos de ADN (proporciones, detalles, color) pasaron de texto de solo lectura a sliders que escriben directamente sobre `currentDNA`; el esqueleto se reconstruye cada frame desde el ADN actual, así que arrastrar un slider reforma la criatura al instante. Inspirado en el editor de RujiK (capturas que mandó el usuario: pestañas Body/Colr/Limb/Detl), pero manteniendo el enfoque procedural del proyecto — se edita ADN, no se esculpe cada segmento a mano.
- Columna vertebral segmentada: en vez de un único cilindro Pelvis→ChestEnd, ahora son 4 sub-huesos con un perfil de grosor (`SpineProfile` en `Skeleton.cpp`) más lleno en el medio y más estrecho en los extremos — modulado por `bodyFat` — en vez de un tubo de grosor uniforme. Reutiliza el mecanismo existente de `radiusScale` por extremo de hueso (el mismo que ya afinaba la cola), solo con más segmentos en la cadena.
- Movimiento por clic mantenido: la criatura solo se mueve mientras se mantiene pulsado el botón **derecho** del ratón (y el cursor está dentro de la ventana 3D, no sobre el panel), en vez de perseguir el ratón constantemente.
- Cámara devuelta a libre + perspectiva + zoom, y terreno devuelto a colinas suaves (no terrazas) — ver "Decisiones de arquitectura" arriba, sección de cámara y terreno.
- Cinemática de columna/cuello/cola realista — ver "Decisiones de arquitectura" arriba, fue el trabajo más largo de esta fase, iterado varias veces con el usuario hasta eliminar el bamboleo/estiramiento y el "vuelo" de las patas delanteras en giros bruscos.
- `headSize`/`headLength`: campos de ADN propios para la cápsula de cabeza, desacoplados de `eyeSize`.

Sigue abierta — no hay más pendientes concretos anotados ahora mismo, pero la fase no se cierra formalmente hasta que el usuario confirme que las criaturas se ven y se mueven bien por sí solas. Ver `docs/DEVELOPMENT_PLAN.md`.

## Convenciones de trabajo con Claude Code

- Avanzar fase por fase, sin adelantar trabajo de fases posteriores.
- Cada fase debe terminar en algo visible/ejecutable, no solo código sin probar.
- Explicar brevemente cualquier concepto de gráficos 3D/OpenGL la primera vez que aparece en una fase.
- Preguntar antes de introducir dependencias nuevas que no estén en el stack técnico de este documento.
- Antes de tocar cualquier cosa visual (paleta, shaders, forma/silueta de criaturas, look de la UI), revisar las capturas de referencia en `reference/critter-crosser/` y la sección "Referencia visual" de este documento — no improvisar el estilo de memoria.
- Las capturas solo sirven para mood/paleta/composición. Para afirmar cómo está construido algo técnicamente (3D vs 2D, motor, shaders, pipeline), no basta con mirar imágenes — buscar entrevistas/devlogs/declaraciones directas del desarrollador antes de darlo por "confirmado" (ver nota de metodología en "Inspiración y referencia técnica real").
