# PG Clip Gain Pro — Protocolo de validación DSP y escucha

**Estado:** Plan de pruebas v0.1 — todavía no ejecutado.

## 1. Qué debemos demostrar

1. Detectar frases y límites sin cortar consonantes, respiraciones útiles ni colas.
2. Elegir una referencia representativa de la propia toma, sin nivel absoluto obligatorio.
3. Reducir diferencias involuntarias sin borrar intención expresiva.
4. Identificar picos anómalos sin atenuar ataques vocales normales.
5. Mantener curvas suaves, estables, reversibles y editables.
6. Analizar sin cortes, clics ni carga excesiva durante reproducción.
7. Abstenerse cuando la señal no permita una decisión confiable.

## 2. Niveles de prueba

### A — Señales sintéticas

Generar silencio digital, ruido estacionario, seno, ruido rosa, ráfagas con rampas, impulsos, transitorios de distintas duraciones, cambios graduales de nivel, regiones con igual pico y distinta energía, picos altos intencionales y señales con valores no finitos.

Validan matemáticas, límites, estabilidad y casos extremos. No sustituyen la voz real.

### B — Casos vocales controlados

Preparar clips anotados de:
- voz hablada neutra y canto suave/fuerte;
- susurros;
- consonantes oclusivas y fricativas;
- respiraciones que deben conservarse y respiraciones discretas;
- finales con reverberación o decaimiento;
- palabras aisladas y frases ligadas;
- dobles, ad-libs y pausas irregulares;
- voz previamente comprimida y voz de amplia dinámica;
- ruido de sala, bleed y música de fondo;
- intención expresiva deliberada;
- distancia variable al micrófono.

Las anotaciones incluyen límites, eventos protegidos y eventos corregibles. Deben permitir desacuerdo y ambigüedad: no forzar una única verdad artística.

### C — Sesiones reales

Probar intérpretes, micrófonos, salas, géneros y técnicas diferentes. Evaluar en contexto musical cuando esté disponible. Conservar siempre una copia intacta.

## 3. Métricas

**Segmentación:** precisión/recall de actividad, error temporal de límites, consonantes/respiraciones/colas recortadas, falsos fragmentos, frases fusionadas y abstenciones apropiadas.

**Clip Gain:** dispersión robusta de sonoridad entre frases antes/después, desviación de intención anotada, cambio de nivel intrafrase, discontinuidad en límites, aceptación/modificación/rechazo y calidad de abstención.

**Clip Peak:** precisión de anomalías, falsos positivos sobre consonantes, error temporal, profundidad y duración de atenuación, inteligibilidad y naturalidad.

**Rendimiento:** tiempo de análisis por minuto, memoria máxima, latencia y carga de processBlock, asignaciones y bloqueos en hilo de audio, estabilidad con distintos buffers y frecuencias, ausencia de NaN, infinito, clics y pops.

No fijar objetivos numéricos definitivos hasta obtener una línea base del prototipo.

## 4. Escucha A/B

1. Conservar la toma original.
2. Crear versiones con Clip Gain solamente, Clip Peak solamente y ambos.
3. Igualar nivel global de escucha para evitar preferencia por volumen.
4. Comparar en contexto y en solo.
5. Escuchar límites, consonantes, respiraciones, finales y transitorios.
6. Probar a volumen bajo y moderado.
7. Registrar mejoras, daños y zonas que deberían haberse dejado intactas.

No evaluar solo medidores. Naturalidad, dicción, fraseo e intención son criterios centrales.

## 5. Regresión

Cada error se convierte en un caso reproducible. Antes de cambiar el algoritmo, ejecutar el conjunto completo, registrar resultados, cambiar una familia de parámetros por vez cuando sea posible, repetir y comparar. Una mejora métrica no debe ocultar una regresión musical.

Mantener un corpus versionado con permisos adecuados. No subir grabaciones privadas ni material de terceros al repositorio sin autorización.

## 6. Bloqueos de calidad

No avanzar si se recortan consonantes o colas sistemáticamente; si silencios o ruido se clasifican como voz; si una frase atípica sesga la referencia; si Clip Peak reduce ataques normales; si la combinación es excesiva; si los resultados no son reproducibles; si el hilo de audio bloquea/asigna memoria; o si el usuario no puede revertir propuestas.

## 7. Registro por caso

Guardar identificador y condiciones del clip, anotaciones y zonas protegidas, versión y parámetros, salida de Shared Analysis, curvas independientes/combinadas, métricas, escucha, decisión humana y motivo.

## 8. Puerta de calidad

La calidad se declara solo tras superar un corpus diverso, obtener resultados repetibles, mantener naturalidad en escucha y dejar el control al usuario. Documentar fallos y casos no resueltos con honestidad.
