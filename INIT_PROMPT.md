Vas a ayudarme a construir un laboratorio de criaturas procedurales en C++ (no un juego).

Lee el archivo CLAUDE.md en la raíz del proyecto: contiene el contexto completo, las decisiones de arquitectura ya tomadas y el roadmap de 10 fases. Estamos en la Fase 1.

Antes de escribir código, en este orden:

1. Configura la estructura del repo y un CMakeLists.txt que compile en Linux, con C++20/23.
2. Añade GLFW, GLM, Dear ImGui y stb_image como dependencias (usa FetchContent o vcpkg, lo que consideres más simple de mantener a largo plazo, y explícame por qué eliges esa opción).
3. Crea el punto de entrada: una ventana con contexto OpenGL, un bucle de render básico, limpieza de pantalla con un color de fondo, y una cámara orbital controlable con el ratón.
4. Integra ImGui mostrando un panel vacío de momento (lo usaremos desde la Fase 2 como panel del laboratorio de ADN).

Es la primera vez que programo gráficos por código, así que explica brevemente los conceptos de OpenGL (contexto, VAO/VBO, shaders, matrices de vista/proyección) la primera vez que aparezcan, sin alargarte más de lo necesario.

Ve paso a paso y para a pedirme confirmación antes de dar por cerrada la Fase 1 y pasar a la Fase 2.
