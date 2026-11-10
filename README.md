# PG Clip Gain Pro

Plugin VST3 de edición vocal inteligente mediante curvas estáticas de Clip Gain. El objetivo es mejorar la consistencia de nivel sin borrar la interpretación ni recurrir a compresión, limitación o saturación.

> Estado actual: núcleo C++17 independiente del host, compilado localmente en Release y Debug con sanitizadores; 6/6 suites pasan en ambos modos. Incluye captura acotada, descriptores iniciales, snapshot compartido y estadísticas contextuales. Todavía no hay integración VST3, segmentación vocal, motores Clip Gain/Clip Peak completos ni binario validado.

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
14. [Flujo compartido de análisis en dos pasadas](docs/12_TWO_PASS_SHARED_ANALYSIS_PIPELINE.md)
15. [Informe de revisión cruzada de la base](docs/13_CROSS_DOCUMENT_REVIEW_REPORT.md)
16. [Base de implementación y contratos iniciales](docs/14_IMPLEMENTATION_FOUNDATION.md)

## Motores

### Clip Gain
Detecta frases, estima su sonoridad percibida, elige una referencia representativa de la toma y propone correcciones relativas, preservando la dinámica interna y los límites fonéticos.

### Clip Peak
Detecta anomalías locales respecto del contexto y propone atenuaciones puntuales con curvas suaves. No es un compresor ni un limitador.

## Estado y próximos pasos

La documentación define la arquitectura y el protocolo para resolver parámetros mediante pruebas. La base host-independiente ya tiene pruebas locales; la siguiente fase es integrar captura con el ciclo de vida VST3 y completar descriptores/continuidad antes de implementar la segmentación contextual y los motores. La interfaz gráfica sigue postergada hasta validar el comportamiento DSP.

Los cinco jueces de PG Audio Intelligence son una fase posterior y separada; no se integrarán sin autorización explícita del usuario.
