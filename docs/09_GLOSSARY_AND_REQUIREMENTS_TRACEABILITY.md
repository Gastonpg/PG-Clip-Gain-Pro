# PG Clip Gain Pro — Glosario y trazabilidad de requisitos

**Estado:** Documento de control de especificación v0.1.

## 1. Glosario

- **Clip Gain:** ganancia estática aplicada a una región temporal de la señal, representada como una curva editable.
- **Clip Peak:** motor que propone atenuaciones estáticas y localizadas para eventos de pico anómalos.
- **Frase:** unidad contextual vocal usada para estimar sonoridad y continuidad; no implica ganancia constante.
- **Región:** intervalo temporal clasificado por el análisis, como voz probable, respiración, pausa o transitorio.
- **Referencia adaptativa:** nivel representativo calculado desde la propia grabación o grupo contextual, sin target absoluto.
- **Abstención:** decisión explícita de no proponer una corrección por falta de evidencia o riesgo musical.
- **Shared Analysis:** conjunto versionado de regiones, descriptores, estadísticas y máscaras de protección compartido por ambos motores.
- **Curva estática:** función temporal de ganancia que no reacciona dinámicamente a la amplitud instantánea como un compresor.
- **Snapshot:** estado completo e inmutable que puede transferirse entre hilos sin exponer datos parcialmente modificados.
- **Hilo de audio:** contexto de ejecución llamado por el host para procesar bloques; debe mantener tiempo de ejecución acotado y evitar operaciones bloqueantes.
- **Captura de análisis:** recopilación de los bloques de audio que entrega el host para poder analizar la región completa cuando VST3 no ofrece acceso directo al archivo.

## 2. Requisitos trazables

| ID | Requisito | Documento principal | Verificación |
|---|---|---|---|
| PG-001 | No imponer un nivel absoluto de salida | 02, 08 | Casos con tomas de distinto nivel y referencia interna |
| PG-002 | Preservar dinámica expresiva | Master, 02, 03 | Corpus vocal y escucha A/B |
| PG-003 | Proteger consonantes, respiraciones y colas | 01, 03, 04 | Anotaciones temporales y falsos positivos |
| PG-004 | Clip Gain y Clip Peak independientes | 05 | Activar/desactivar y comparar curvas |
| PG-005 | Evitar doble corrección | 05 | Pruebas de solapamiento y límites combinados |
| PG-006 | Toda propuesta incluye evidencia y confianza | Master, 05 | Validación de esquema y revisión |
| PG-007 | Permitir aceptación, rechazo, edición y reversión | 03, 04, 05 | Pruebas de interacción y persistencia |
| PG-008 | Abstenerse ante ambigüedad | 01, 02, 04, 05 | Casos difíciles etiquetados |
| PG-009 | No bloquear ni asignar memoria en hilo de audio | 07 | Instrumentación y pruebas de estrés |
| PG-010 | No asumir acceso directo al archivo fuente desde VST3 | 07 | Prueba en hosts y flujo de captura |
| PG-011 | Análisis eficiente en hardware modesto | 06, 07 | Perfil de CPU y memoria |
| PG-012 | Resultados reproducibles y versionados | 05, 06, 07 | Repetición con mismos datos y parámetros |
| PG-013 | Waveform y edición gráfica se implementan después de validar DSP | 08 | Puerta de fase y revisión de arquitectura |\n| PG-014 | Todos los módulos comparten unidades, intervalos y estados numéricos inequívocos | 10 | Pruebas de dominio temporal, canales, valores especiales y curvas |\n| PG-015 | El banco sintético es determinista y reproducible | 11 | Ejecución repetida de fixtures con semillas y configuraciones fijas |\n| PG-016 | Capturas parciales o discontinuas no habilitan conclusiones globales no justificadas | 07, 10, 11 | Fixtures CAP-002 a CAP-006 y verificación de estado de cobertura |\n| PG-017 | La descripción base y el refinamiento contextual son pasadas separadas y reproducibles | 12 | Pruebas de salida por pasada, determinismo y equivalencia entre tamaños de bloque |

## 3. Convenciones de unidades

- Tiempo interno: muestras enteras con frecuencia de muestreo asociada.
- Tiempo de interfaz: segundos o formato musical derivado, sin reemplazar el índice exacto de muestras.
- Ganancia: dB para presentación; representación lineal para multiplicación de muestras.
- Confianza: valor normalizado con definición documentada; no debe presentarse como probabilidad calibrada hasta demostrarlo.
- Sonoridad: indicar siempre descriptor, ventana, ponderación y unidad. No llamar LUFS a una métrica propia que no implemente el estándar correspondiente.

## 4. Reglas de nomenclatura

- Usar “Clip Gain” y “Clip Peak” para los dos motores.
- Usar “referencia adaptativa” para el nivel representativo de la grabación.
- Usar “propuesta” o “recomendación” hasta que el usuario la acepte.
- No llamar “IA entrenada” a un conjunto de reglas DSP o rúbricas.
- No llamar “compresión” a una curva estática de Clip Gain.
- Diferenciar pico de muestra de true peak estimado.

## 5. Regla de cambios

Todo cambio de comportamiento debe:
1. identificar requisitos afectados;
2. actualizar el documento responsable;
3. añadir o modificar pruebas;
4. registrar si el cambio es conceptual, experimental o validado;
5. preservar compatibilidad o documentar migración de sesiones si afecta datos guardados.

La matriz es una guía de cobertura documental; no implica que los requisitos estén implementados o superados.
