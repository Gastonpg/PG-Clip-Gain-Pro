# PG Clip Gain Pro

Plugin VST3 de edición vocal inteligente mediante curvas estáticas de Clip Gain. El objetivo es mejorar la consistencia de nivel sin borrar la interpretación ni recurrir a compresión, limitación o saturación.

> Estado actual: diseño técnico fundacional. Todavía no hay motor DSP implementado ni binario VST3 validado.

## Filosofía del producto

- Naturalidad antes que uniformidad.
- Referencia adaptativa obtenida de la propia grabación; sin objetivo absoluto fijo.
- Dos motores independientes: Clip Gain y Clip Peak.
- Curvas estáticas, editables, reversibles y bajo control del usuario.
- Ante evidencia ambigua, proteger el fragmento y pedir revisión.
- Análisis offline; procesamiento de audio en tiempo real mínimo y predecible.

## Documentación

1. [Especificación maestra de PG Audio Intelligence](docs/PG_AUDIO_INTELLIGENCE_MASTER_SPEC.md)
2. [Diseño del algoritmo DSP](docs/DSP_ALGORITHM_DESIGN.md)
3. [Actividad vocal y segmentación de frases](docs/01_VOCAL_ACTIVITY_AND_PHRASE_SEGMENTATION.md)
4. [Sonoridad vocal y referencia adaptativa](docs/02_VOCAL_LOUDNESS_AND_ADAPTIVE_REFERENCE.md)
5. [Diseño de curvas Clip Gain](docs/03_CLIP_GAIN_CURVE_DESIGN.md)
6. [Detección de Clip Peak y sus curvas](docs/04_CLIP_PEAK_DETECTION_AND_CURVES.md)
7. [Análisis compartido y contratos entre motores](docs/05_SHARED_ANALYSIS_AND_ENGINE_CONTRACTS.md)
8. [Protocolo de validación DSP y escucha](docs/06_VALIDATION_AND_TEST_PROTOCOL.md)
9. [Arquitectura de ejecución y recursos](docs/07_PLUGIN_RUNTIME_AND_RESOURCE_ARCHITECTURE.md)
10. [Revisión de base y puertas de implementación](docs/08_FOUNDATION_REVIEW_AND_IMPLEMENTATION_GATES.md)
11. [Glosario y trazabilidad de requisitos](docs/09_GLOSSARY_AND_REQUIREMENTS_TRACEABILITY.md)
12. [Contratos numéricos y modelo de datos DSP](docs/10_DSP_DATA_MODEL_AND_NUMERICAL_CONTRACTS.md)
13. [Fixtures sintéticos y criterios de aceptación](docs/11_SYNTHETIC_FIXTURES_AND_ACCEPTANCE_CRITERIA.md)

## Motores

### Clip Gain
Detecta frases, estima su sonoridad percibida, elige una referencia representativa de la toma y propone correcciones relativas, preservando la dinámica interna y los límites fonéticos.

### Clip Peak
Detecta anomalías locales respecto del contexto y propone atenuaciones puntuales con curvas suaves. No es un compresor ni un limitador.

## Estado y próximos pasos

La documentación define la arquitectura y el protocolo para resolver parámetros mediante pruebas. La siguiente fase es revisar la consistencia de la especificación y luego implementar el núcleo DSP incrementalmente, empezando por análisis y segmentación sin interfaz gráfica.

Los cinco jueces de PG Audio Intelligence son una fase posterior y separada; no se integrarán sin autorización explícita del usuario.
