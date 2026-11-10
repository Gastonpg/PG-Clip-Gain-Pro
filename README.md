# PG Clip Gain Pro

Plugin VST3 de edición vocal inteligente mediante curvas estáticas de Clip Gain. El objetivo es mejorar la consistencia de nivel sin borrar la interpretación ni recurrir a compresión, limitación o saturación.

> Estado actual: núcleo C++17 y shell VST3 de compatibilidad transparente integrados. La versión anterior del núcleo pasó 6/6 suites localmente en Release y Debug con sanitizadores. Desde entonces se ampliaron descriptores/snapshots y se añadieron seis suites nuevas: SPSC, ensamblado de captura, actividad por ventanas, base Clip Gain, aplicación de curvas y Clip Peak con curvas de atenuación y coordinador de seguridad de ganancia combinada; las 12 suites del código actual deben revalidarse. El shell VST3 está configurado para compilarse y pasar el Validator de Steinberg en CI para Windows x64 y Linux x64, pero todavía no hay resultado remoto confirmado ni prueba en FL Studio. Falta integrar la captura al transporte real del DAW, conectar los prototipos de ambos motores al VST3, completar la segunda pasada perceptual/fonética y construir la GUI.

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
17. [Compilación e instalación VST3 en Windows](docs/15_BUILDING_AND_INSTALLING_VST3.md)
18. [Plan de integración de captura VST3](docs/16_VST3_CAPTURE_INTEGRATION_PLAN.md)
19. [Plan de conocimiento, entrenamiento y evaluación de los cinco jueces](docs/17_FIVE_JUDGES_TRAINING_AND_EVALUATION_PLAN.md)

## Motores

### Clip Gain
Detecta frases, estima su sonoridad percibida, elige una referencia representativa de la toma y propone correcciones relativas, preservando la dinámica interna y los límites fonéticos.

### Clip Peak
Detecta anomalías locales respecto del contexto y propone atenuaciones puntuales con curvas suaves. No es un compresor ni un limitador.

## Estado y próximos pasos

La documentación define la arquitectura y el protocolo para resolver parámetros mediante pruebas. La base host-independiente tiene 12 suites configuradas, todavía pendientes de una ejecución completa confirmada. La siguiente fase es implementar el handshake seguro de captura VST3, conectar cola y worker, y validar el ciclo de transporte antes de aplicar curvas. La interfaz gráfica sigue postergada hasta validar el comportamiento DSP.

Los cinco jueces de PG Audio Intelligence son una fase posterior y separada; no se integrarán sin autorización explícita del usuario.
