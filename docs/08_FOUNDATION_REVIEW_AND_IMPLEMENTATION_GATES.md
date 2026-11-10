# PG Clip Gain Pro — Revisión de base y puertas de implementación

**Estado:** Plan de cierre de diseño v0.1.

## 1. Alcance especificado

La base documental cubre:
- filosofía y límites de PG Audio Intelligence;
- motores independientes Clip Gain y Clip Peak;
- actividad vocal y segmentación;
- sonoridad y referencia adaptativa;
- curvas estáticas de ambos motores;
- coordinación y contratos de datos;
- validación DSP y escucha;
- arquitectura de ejecución y presupuesto de recursos.

Esto describe decisiones de diseño. No demuestra que el algoritmo funcione con grabaciones reales.

## 2. Decisiones cerradas

1. No hay objetivo fijo de -18 dBFS, -23 LUFS ni otro nivel absoluto.
2. La referencia se obtiene de la propia grabación y debe resistir frases atípicas.
3. Se busca consistencia musical, no uniformidad matemática.
4. Clip Gain y Clip Peak son independientes.
5. Las curvas son estáticas y editables, no compresión ni limitación.
6. La fuente no se altera destructivamente por defecto.
7. Los casos ambiguos se protegen o marcan para revisión.
8. El usuario conserva aceptación, rechazo, edición y reversión.
9. El análisis complejo ocurre offline; el hilo de audio aplica curvas preparadas.
10. El diseño apunta a hardware modesto sin IA pesada ni nube.

## 3. Decisiones abiertas hasta medir

No fijar por intuición:
- tamaño de ventana y salto;
- filtros y ponderaciones;
- umbrales de actividad;
- histéresis y continuidad;
- mínimo de muestras válidas por frase;
- método definitivo de referencia;
- límites máximos de corrección;
- profundidad, anchura y rampas de Clip Peak;
- confianza y abstención;
- estructuras y persistencia finales.

Cada decisión debe tener una prueba que la resuelva.

## 4. Orden recomendado

**Fase 0 — Revisión de especificación:** consistencia documental, glosario, unidades, matriz de requisitos/pruebas y parámetros experimentales.

**Fase 1 — Núcleo sin GUI:** lectura segura, descriptores, actividad y segmentación, exportación inspeccionable y pruebas sintéticas.

**Fase 2 — Clip Gain:** perfil de frase, referencia adaptativa, correcciones relativas, curva y escucha.

**Fase 3 — Clip Peak:** eventos candidatos, puntuación contextual, curva localizada y pruebas con consonantes/transitorios.

**Fase 4 — Coordinación:** solapamientos, límites combinados, snapshots, aceptación/rechazo/edición y reversión.

**Fase 5 — VST3 mínimo:** hilo de audio seguro, estado persistente, perfil de recursos y prueba en FL Studio y otro host cuando sea posible.

**Fase 6 — Interfaz:** solo tras validar DSP; waveform rellena y legible, zoom, regiones, curvas separadas, edición manual precisa y diseño oscuro sobrio aprobado.

## 5. Qué significa «base lista»

La base documental está cerrada cuando cada requisito tiene sección responsable; ambos motores tienen contratos; el coordinador tiene reglas; cada decisión abierta tiene una prueba; el hilo de audio tiene restricciones claras; el plan separa DSP de UI; y la revisión cruzada no encuentra contradicciones importantes.

Esto no equivale a algoritmo validado, plugin compilable ni calidad profesional demostrada.

## 6. Puerta separada para los cinco jueces

La creación o integración de los cinco jueces de PG Audio Intelligence es una fase distinta. Requiere base DSP y contratos estables, entradas/salidas y límites definidos, criterios de desacuerdo/abstención, conocimiento documentado y trazable, y evaluación de valor real.

No iniciar esa fase sin autorización explícita del usuario. Los jueces no deben depender del hilo de audio ni reemplazar pruebas objetivas.

## 7. Estado honesto

A la fecha:
- existe especificación y diseño documental;
- no hay motor DSP implementado;
- no se han ejecutado pruebas con señales ni voces;
- no existe binario VST3 validado;
- las decisiones numéricas siguen abiertas donde corresponde.

La siguiente etapa técnica es construir el núcleo DSP incrementalmente y medirlo. No declarar éxito antes de tener resultados.
