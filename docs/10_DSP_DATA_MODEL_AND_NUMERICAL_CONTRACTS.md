# PG Clip Gain Pro — Contratos numéricos y modelo de datos DSP

**Estado:** Especificación de implementación v0.1 — pendiente de prototipo y validación.
**Propósito:** fijar convenciones inequívocas para que captura, análisis compartido, motores, coordinador, editor y procesador de audio interpreten los mismos datos.

## 1. Principios

1. Una muestra tiene una posición exacta; el tiempo en segundos es una conversión.
2. Todo intervalo usa inicio inclusivo y final exclusivo: `[startSample, endSample)`.
3. Ninguna métrica se compara si difieren frecuencia de muestreo, canales, ventana, salto, ponderación o versión de cálculo sin conversión explícita.
4. Los datos inválidos nunca se convierten silenciosamente en evidencia válida.
5. Los valores experimentales se versionan y registran; no se dispersan como constantes mágicas.
6. Los resultados deben ser deterministas para la misma señal, versión y configuración, salvo operaciones cuya variabilidad esté documentada.
7. La confianza es un índice de evidencia hasta que exista calibración empírica; no equivale automáticamente a probabilidad.

## 2. Dominio temporal

### 2.1 Identidad de una captura

Cada captura debe llevar:
- identificador de sesión y de región capturada;
- frecuencia de muestreo efectiva;
- cantidad de canales y disposición conocida;
- cantidad total de frames por canal;
- posición de inicio en la línea temporal del proyecto cuando el host la proporcione;
- cobertura y discontinuidades detectadas;
- versión del formato de captura y del análisis.

En audio intercalado, un frame contiene una muestra por canal. Los índices temporales se expresan en frames, no en el índice bruto del arreglo intercalado.

### 2.2 Intervalos

- `startSample`: entero de 64 bits, inclusivo.
- `endSample`: entero de 64 bits, exclusivo.
- Validez: `0 <= startSample < endSample <= totalFrames`.
- Duración en segundos: `(endSample - startSample) / sampleRate`.
- La conversión desde segundos debe especificar regla de redondeo y hacerse una sola vez en el límite de la API.
- No usar punto flotante como identidad primaria de un límite.

Los bloques capturados deben conservar su posición absoluta dentro de la región. Un bloque perdido, repetido o fuera de orden crea una discontinuidad explícita; nunca se concatena como si fuera continuo.

### 2.3 Transporte y cobertura

El analizador distingue:
- captura completa desde el inicio hasta el final esperado;
- captura parcial;
- captura con huecos;
- captura reiniciada por seek o cambio de región;
- captura cancelada.

Solo una captura completa y coherente puede habilitar decisiones globales sobre referencia de toda la toma. Un análisis parcial debe limitar sus conclusiones a la cobertura disponible y advertirlo.

## 3. Canales

La señal original multicanal se conserva conceptualmente por canal durante la captura y la validación. Para análisis vocal:
- no sumar canales sin comprobar polaridad y correlación;
- no permitir que una cancelación de fase convierta voz válida en silencio;
- calcular descriptores por canal y una representación de análisis vinculada;
- documentar cómo se selecciona o combina evidencia entre canales;
- mantener un mapa entre el resultado de análisis y los canales de audio a los que se aplicará la curva.

La primera implementación puede priorizar mono y estéreo. Cualquier disposición no soportada debe producir estado explícito, no una mezcla improvisada.

## 4. Ventanas y descriptores

Cada descriptor debe declarar:
- nombre y versión;
- unidad y escala;
- rango válido;
- canal o política multicanal;
- longitud de ventana y salto en frames;
- alineación temporal (inicio, centro o final);
- ponderación/filtro aplicado;
- tratamiento de bordes;
- estado de validez y motivo de invalidez.

El centro temporal de una ventana se representa mediante índice entero o racional definido; no se desplaza por redondeos acumulados.

### 4.1 Familias iniciales

- Energía/RMS por ventana.
- Pico de muestra.
- Estimación de pico entre muestras, si se implementa y valida.
- Envolvente suavizada.
- Descriptor de sonoridad vocal.
- Actividad/densidad espectral.
- Pendiente, prominencia y duración del ataque.
- Continuidad y relación local con fondo.
- Estadísticas robustas de frase y contexto.

No se declara que RMS, LUFS integrados, true peak ni una métrica propia sean intercambiables. Si se usa una ponderación inspirada en estándares de sonoridad, se documentan filtro, calibración, integración y límites; si no implementa el estándar completo, no se etiqueta con su nombre.

### 4.2 Valores especiales

- Rechazar o marcar NaN e infinito antes de calcular estadísticas.
- Evitar `log(0)`; definir piso numérico de cálculo separado del ruido real de la grabación.
- El silencio digital tiene estado propio; no recibe una sonoridad artificial que pueda sesgar la referencia.
- Saturación numérica, subnormales y desbordamientos deben tratarse de forma explícita.
- Cada descriptor puede ser válido, insuficiente, no aplicable o inválido.

## 5. Estadísticas robustas y referencia adaptativa

Las estadísticas de frase deben conservar cantidad de ventanas válidas, dispersión y distribución, no solo un promedio. La referencia:
- se calcula únicamente con frases elegibles;
- reduce influencia de outliers y frases atípicas;
- registra qué frases y pesos contribuyeron;
- registra el método y versión;
- puede separarse por grupos contextuales si la evidencia justifica contrastes intencionales;
- informa confianza y motivo de abstención si no existe base suficiente.

No fijar aquí mediana, percentiles, filtros ni ventanas definitivas como constantes universales. Deben compararse mediante corpus controlado, escucha A/B y regresión.

## 6. Ganancia y curvas

### 6.1 Convenciones

- La interfaz muestra ganancia en dB.
- La aplicación de audio utiliza factor lineal de amplitud.
- Conversión: `linearGain = 10^(gainDb / 20)`.
- El valor neutro es 0 dB / factor 1.
- Las curvas están definidas sobre la línea temporal de la captura y vinculadas a su frecuencia de muestreo.
- Cada punto o segmento conserva origen automático, manual o combinado.

### 6.2 Interpolación

La curva debe ser continua y determinista entre bloques. La especificación de cada segmento declara:
- tiempo o muestra inicial/final;
- valor de ganancia;
- tipo de interpolación;
- tangentes o parámetros adicionales si aplica;
- región protegida asociada;
- límites de pendiente y continuidad aplicables.

La interpolación en dB y la interpolación lineal de amplitud no son equivalentes. El prototipo comparará opciones y fijará una política única, con pruebas de continuidad y audición. No cambiar de dominio de interpolación entre segmentos sin especificarlo.

### 6.3 Composición

Clip Gain y Clip Peak conservan curvas independientes. Si se componen en dB, la suma equivale al producto de factores lineales para ganancias estáticas coincidentes en el tiempo; el coordinador debe verificar el resultado combinado, sus transiciones y límites antes de publicarlo.

La curva aprobada se publica como snapshot completo e inmutable. Nunca se modifica un arreglo mientras el hilo de audio lo está leyendo.

## 7. Modelo lógico de datos

El siguiente esquema es conceptual; los nombres y tipos definitivos se implementarán en C++ después de verificar requisitos de host y serialización.

### AnalysisSession
- schemaVersion
- algorithmVersion
- captureId
- sampleRate
- channelCount / channelLayout
- totalFrames
- timelineStart (si existe)
- coverageState
- discontinuities[]
- analysisConfigId
- warnings[]

### Region
- id
- startSample / endSample
- semanticClass
- confidence
- detectorId
- protectionFlags[]
- validity
- reviewReason

### PhraseProfile
- id
- startSample / endSample
- regionIds[]
- loudnessDescriptor
- robustStatistics
- validWindowCount
- contextualGroup
- referenceEligibility
- confidence
- abstentionReason

### PeakEvent
- id
- startSample / endSample
- peakMetrics
- localContextMetrics
- tentativeClass
- confidence
- protectionFlags[]
- abstentionReason

### GainCurve
- id
- engineId
- captureId
- sampleRate
- pointsOrSegments[]
- source (automatic/manual/mixed)
- status
- algorithmVersion
- parameterSetVersion

### Recommendation
- id
- engineId
- targetRegionId
- evidence[]
- proposedAction
- confidence
- riskFlags[]
- status
- explanation
- abstentionReason

Cada colección debe tener orden temporal definido, IDs estables dentro de la sesión y validación de límites antes de ser consumida.

## 8. Errores y estados

Los errores se distinguen de una abstención musical:
- **Error de captura:** faltan bloques, discontinuidad o transporte incoherente.
- **Error de formato:** frecuencia, canales o disposición no soportados.
- **Error numérico:** datos no finitos o rango imposible.
- **Evidencia insuficiente:** señal demasiado corta, baja relación con fondo o pocos datos válidos.
- **Ambigüedad musical:** la diferencia podría ser intencional.
- **Conflicto de motores:** propuestas independientes incompatibles al combinarse.
- **Cancelación:** el usuario o el host interrumpió el análisis.

Solo los últimos tres pueden ser abstenciones o solicitudes de revisión; los errores técnicos deben mostrarse como fallos de análisis, no como decisiones musicales.

## 9. Reproducibilidad y serialización

Guardar junto a cada resultado:
- versión de esquema y algoritmo;
- configuración completa y versión;
- frecuencia y canales;
- identificador/hash de captura cuando sea viable sin exponer audio privado;
- cobertura;
- estado y procedencia de cada corrección;
- decisiones manuales.

Una versión nueva no debe reinterpretar silenciosamente una curva anterior. Si el esquema cambia, se migra explícitamente o se marca como incompatible. Los datos desconocidos de versiones futuras no se descartan silenciosamente.

## 10. Pruebas obligatorias derivadas

1. Conversión de frames a segundos y vuelta en varias frecuencias de muestreo.
2. Intervalos vacíos, invertidos, fuera de rango y límites exactos.
3. Bloques contiguos, repetidos, perdidos, reordenados y tras seek.
4. Mono, estéreo en fase, estéreo fuera de fase y multicanal no soportado.
5. Ventanas al principio/final y clips menores que una ventana.
6. Silencio digital, NaN, infinito, subnormales y valores extremos.
7. Conversión dB/factor lineal y retorno dentro de tolerancia.
8. Continuidad de curva en fronteras de bloque y seeks.
9. Composición independiente y combinada de Clip Gain / Clip Peak.
10. Serialización, restauración, migración y reproducibilidad.
11. Misma señal/configuración en distintos tamaños de bloque: resultados equivalentes.
12. Captura parcial: impedir referencia global y advertir limitación.

## 11. Puerta de aceptación

Este contrato estará listo para implementación cuando:
- todos los módulos usen las mismas unidades y límites;
- las estructuras puedan validarse sin depender de la GUI;
- cada error tenga estado definido;
- las pruebas anteriores estén automatizadas;
- no haya constantes experimentales sin versión ni procedencia.

La aprobación documental no implica que las métricas, curvas o decisiones musicales ya estén validadas.
