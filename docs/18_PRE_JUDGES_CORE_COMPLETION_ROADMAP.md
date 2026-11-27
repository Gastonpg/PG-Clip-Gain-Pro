# PG Clip Gain Pro — Hoja de ruta para completar el núcleo antes de los jueces

**Rama de trabajo:** `pre-judges/core-completion-roadmap`  
**Estado:** fase A superada en CI; fase B incluye captura VST3 y publicación de snapshot tras captura válida; fase C tiene una primera orquestación de los dos motores y coordinación de curvas añadida, pendiente de CI y validación en host. No se declara validación perceptual ni integración de UI.  
**Regla de alcance:** no implementar ni entrenar los cinco jueces hasta cerrar las puertas de aceptación de este documento y recibir autorización del usuario.

## 1. Diagnóstico honesto del estado encontrado

La rama de desarrollo contiene una base C++17 host-independiente, un shell VST3 transparente, prototipos de análisis por ventanas, actividad energética, perfiles provisionales, recomendaciones iniciales de Clip Gain, generación de curvas estáticas, detección de eventos Clip Peak, coordinación de curvas y 15 ejecutables de prueba declarados en CMake.

Todavía no se debe presentar como plugin funcional de nivelación vocal. En particular:

- El procesador VST3 copia la entrada a la salida (passthrough). No captura audio del host.
- El controlador no ofrece todavía controles de armado/desarmado ni interfaz de edición.
- El coordinador de captura, la cola, el adaptador planar y el guardián de continuidad existen como módulos separados, pero no están conectados al callback real y a un worker VST3.
- La segunda pasada contextual completa no está implementada.
- La actividad es energética y conservadora; no es reconocimiento de voz ni segmentación fonética.
- Los perfiles y la referencia adaptativa usan RMS como proxy provisional, no una métrica perceptual vocal validada.
- Las curvas Clip Gain y Clip Peak son candidatas/prototipos, no una edición integrada, aprobable, persistente y reversible dentro del host.
- Las 15 suites están configuradas, pero no existe confirmación de una ejecución completa del estado actual.
- No hay validación de escucha con corpus vocal autorizado ni prueba funcional en FL Studio.

## 2. Orden de trabajo obligatorio

### Fase A — Puerta de compilación y pruebas reproducibles

1. Configurar un runner Windows x64 externo sin exigir herramientas de compilación locales al usuario.
2. Mantener el repositorio privado hasta que el usuario decida explícitamente lo contrario; no cambiar visibilidad.
3. Separar build/test del empaquetado de artefactos para no consumir almacenamiento innecesario.
4. Ejecutar las 15 suites actuales y corregir errores antes de añadir complejidad.
5. Ejecutar el Validator oficial de Steinberg sobre el bundle VST3.
6. Guardar logs de compilación, versiones del SDK, configuración y resultados.

**Aceptación alcanzada en el commit `20a4ba7`:** build Windows x64 verde, 15/15 suites verdes, selftest del SDK 51/51, Validator 47/47 y bundle VST3 empaquetado. Un build verde no sustituye la prueba en FL Studio.

### Fase B — Captura real desde VST3

**Implementación inicial publicada, pendiente de CI/host:** parámetro automatizable `Capture`; callback como productor SPSC; adaptador planar para bloques grandes; worker consumidor/ensamblador; continuidad con `projectTimeSamples`, reproducción y ciclo; contador de callbacks activos para quiescencia; presupuesto de PCM de 16 MiB y tope de 60 segundos; passthrough mientras se captura.

1. Validar en CI compilación, pruebas y Validator de esta integración.
2. Confirmar pruebas de inicio del ensamblador con bloques ya encolados y detección de overflow anterior al primer drenaje.
3. Completar pruebas de stop, cancelación, seek, loop, overflow, cambio de formato, cierre y múltiples instancias.
4. Exponer estado/progreso y resultado de captura sin bloquear el hilo de audio.
5. Publicar la captura válida como snapshot inmutable para el analizador.
6. Probar en FL Studio 2026 x64.
7. Mantener passthrough hasta que una curva haya sido aprobada.

**Aceptación:** captura mono/estéreo Float32/Float64, sin asignaciones ni bloqueos en el callback; pruebas de stop, cancelación, seek, loop, overflow, cierre y múltiples instancias; ninguna sesión incompleta se etiqueta como completa.

### Fase C — Análisis compartido completo en dos pasadas

**Primer coordinador añadido, pendiente de CI:** `buildAnalysisPipeline` recibe un único `SharedAnalysisSnapshot`, calcula un mapa de actividad común, perfiles y referencia adaptativa; entrega ese mismo contexto a Clip Gain y Clip Peak; conserva las curvas independientes y produce una curva combinada con límites de seguridad. Se agregó una suite específica de integración. Esta versión usa la detección energética y RMS proxy existente; no equivale al refinamiento fonético contextual completo ni a protección validada de consonantes/respiraciones.

**Pasada 1 — descripción neutral**
- Validación de la captura y cobertura.
- Descriptores por ventana/canal con unidades, alineación y validez.
- Envolvente, energía, pico, crest factor y transitorios candidatos.
- Estimación cautelosa de fondo y contaminación.
- Mapa de actividad candidata sin generar correcciones.

**Pasada 2 — refinamiento contextual**
- Continuidad temporal y agrupación de regiones.
- Refinamiento de inicios/finales y pausas internas.
- Protección explícita de consonantes, respiraciones y colas.
- Clasificación de incertidumbre y regiones que requieren revisión.
- Perfiles robustos por frase y procedencia de cada métrica.
- Snapshot final versionado, inmutable y compartido por ambos motores.

**Aceptación:** determinismo para misma captura/configuración, resultados independientes del tamaño de bloque, cancelación sin publicación obsoleta, límites protegidos y evidencia explícita. No afirmar reconocimiento fonético perfecto sin un sistema que lo respalde.

### Fase D — Sonoridad vocal y referencia adaptativa

1. Evaluar métricas perceptuales adecuadas para voz y ventanas locales; documentar filtros, ponderaciones y unidades.
2. Evitar que consonantes breves, plosivas, respiraciones o silencios dominen la medición.
3. Comparar descriptores candidatos contra anotaciones humanas y escucha.
4. Seleccionar una referencia interna robusta de la propia toma.
5. Identificar outliers y secciones intencionalmente distintas sin forzarlas a una media global.
6. Mantener abstención cuando no exista un conjunto representativo fiable.

**Aceptación:** no existe un destino absoluto fijo; la referencia es explicable, estable y musicalmente validada con ejemplos variados.

### Fase E — Motor Clip Gain

1. Consumir exclusivamente el snapshot final compartido.
2. Proponer correcciones relativas entre frases, preservando la dinámica interna.
3. Aplicar intensidad Sutil/Equilibrada/Pronunciada como escala de intervención, no como loudness objetivo.
4. Generar curvas estáticas suaves con límites de pendiente, rampas y prevención de clics.
5. Permitir edición por región, aceptación, rechazo, undo y reversión.
6. Mantener la edición no destructiva y guardar procedencia.

**Aceptación:** A/B ciego demuestra mayor consistencia sin pérdida inaceptable de expresión, consonantes, respiraciones o colas.

### Fase F — Motor Clip Peak

1. Distinguir anomalías locales de consonantes y transitorios naturales.
2. Usar contexto local, duración, forma, energía y protecciones.
3. Proponer atenuaciones localizadas, no compresión ni limitación.
4. Crear curvas separadas y editables.
5. Marcar casos dudosos para revisión en lugar de atenuarlos automáticamente.

**Aceptación:** falsos positivos medidos y controlados; no se dañan ataques vocales; curva independiente del motor Clip Gain.

### Fase G — Coordinación, persistencia y seguridad

1. Evaluar la combinación de curvas y evitar doble corrección perjudicial.
2. Definir prioridad de edición manual frente a propuestas automáticas.
3. Publicar snapshots inmutables con vida útil segura para el hilo de audio.
4. Garantizar que ninguna liberación de memoria o bloqueo ocurra en el callback.
5. Añadir estado persistente versionado, undo y recuperación ante proyecto restaurado.
6. Probar sesiones largas, múltiples instancias y cambios de transporte.

**Aceptación:** cambios reversibles, consistentes y libres de carreras; el audio original permanece intacto y el usuario controla qué curvas se aplican.

### Fase H — Validación vocal y de host

Preparar un corpus de grabaciones con permiso de uso y etiquetas humanas. Incluir susurros, voz proyectada, frases suaves/fuertes, plosivas, sibilantes, consonantes finales, respiraciones, colas, pausas internas, dobles, armonías, coros, ruido, filtraciones, saturación de origen y casos ambiguos.

Separar sesiones/artistas entre desarrollo y prueba para evitar contaminación. Hacer escucha A/B ciega, registrar desacuerdos y conservar casos donde la salida correcta sea no intervenir.

Probar en FL Studio 2026 x64 y, si es posible, un segundo host VST3: carga, reproducción, stop, seek, loop, cambio de sample rate, guardado/restauración, varias instancias y cierre.

**Aceptación:** métricas DSP, evaluación humana y escucha coinciden suficientemente; no declarar calidad profesional solo por pasar pruebas sintéticas.

### Fase I — Interfaz gráfica y entrega

Solo después de validar el DSP:
- waveform real y navegación;
- regiones/frases detectadas;
- curvas Clip Gain y Clip Peak claramente separadas;
- edición manual precisa;
- zoom y desplazamiento;
- controles de intensidad;
- revisión de advertencias/confianza;
- aceptar/rechazar/undo;
- tema oscuro PG y rendimiento adecuado.

**Aceptación:** UI conectada al motor real, sin datos ficticios ni controles decorativos; instalador/paquete y guía de uso.

## 3. Política de jueces

Los cinco jueces siguen siendo una fase posterior. Antes de iniciarla deben existir:

- snapshot final estable y versionado;
- entradas/salidas y unidades cerradas;
- propuestas DSP reproducibles;
- conjunto de evaluación vocal autorizado y etiquetado;
- métricas de falsos positivos, falsos negativos, naturalidad y abstención;
- contrato para desacuerdos y árbitro;
- demostración de que un juez añade valor frente a reglas DSP deterministas.

No se entrenará un modelo ni se afirmará que los jueces están entrenados por el mero hecho de crear prompts, nombres o documentos. La primera implementación futura será un conjunto de evaluadores deterministas, si el usuario lo autoriza.

## 4. Límites de recursos y privacidad

- No exigir que el usuario instale compiladores, SDK ni toolchains grandes en su PC.
- Mantener la compilación en la nube mientras el repositorio y las cuotas lo permitan.
- No cambiar el repositorio a público sin autorización explícita.
- No subir voces privadas a servicios externos sin consentimiento informado.
- No depender de una API de IA ni de la nube durante la reproducción.
- Mantener el DSP central local, determinista y liviano.

## 5. Próximo paso concreto

El primer trabajo de implementación será cerrar la puerta de compilación reproducible y ejecutar las 15 suites en Windows x64. Después se abordará la integración de captura VST3, porque sin audio real que atraviese el plugin no puede existir un análisis vocal real.

**Estado final de esta hoja de ruta:** plan creado; no se afirma que las fases A–I estén completadas.
