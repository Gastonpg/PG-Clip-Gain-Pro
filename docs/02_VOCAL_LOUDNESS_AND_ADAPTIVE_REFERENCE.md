# 02 — Estimación de sonoridad vocal y referencia adaptativa

Estado: diseño de algoritmo; parámetros pendientes de validación con audio real.  
Proyecto: PG Clip Gain Pro.

## 1. Objetivo

Estimar qué fragmentos vocales son perceptualmente más fuertes o más débiles dentro de la propia grabación y elegir una referencia interna representativa. El sistema no persigue un nivel absoluto en dBFS ni impone un objetivo universal como -18 dBFS.

La salida de este módulo es una descripción fiable de la sonoridad de cada frase, una referencia interna con confianza y una recomendación relativa de corrección para el motor Clip Gain. No modifica audio ni decide por sí solo qué expresión debe conservarse.

## 2. Principios

- Comparar la voz consigo misma, no contra un nivel de mezcla prefijado.
- Consistencia sin uniformidad: preservar diferencias expresivas justificadas.
- No confundir amplitud de pico con sonoridad percibida.
- Evitar que silencios, respiraciones aisladas, plosivas o transitorios breves dominen la medición.
- Considerar el contexto local y el contexto musical/global.
- Si la evidencia no alcanza para distinguir un error de una intención artística, reducir la corrección o abstenerse.
- Mantener separados el análisis, la recomendación y la aplicación de ganancia.

## 3. Descriptores por ventana

El análisis parte de las regiones candidatas detectadas por el módulo de actividad vocal. Para cada ventana útil se podrán calcular, según la validación:

- Energía RMS y nivel equivalente.
- Sonoridad ponderada perceptualmente, usando K-weighting como descriptor posible, no como única verdad.
- Envolvente y percentiles de amplitud.
- Factor de cresta y relación entre pico y energía.
- Duración y proporción de actividad vocal.
- Indicadores de ruido, respiración, plosivas y transitorios.
- Confianza de que la ventana representa voz útil y no un evento aislado.

Las ventanas cortas no deben tratarse como mediciones intercambiables con la sonoridad integrada de un programa completo. La medición perceptual se adapta a la duración del fragmento y se contrasta con descriptores temporales más robustos.

## 4. Resumen de cada frase

Cada frase recibe un perfil, no un único número ciego. El perfil debe contemplar:

1. Nivel central robusto de las ventanas vocales válidas.
2. Distribución interna de niveles (mediana y percentiles robustos).
3. Variación dinámica dentro de la frase.
4. Presencia de ataques o picos que no representan el cuerpo de la frase.
5. Respiraciones y colas detectadas, que se conservan como eventos protegidos o se describen por separado.
6. Confianza y causas de incertidumbre.

La mediana y los percentiles son candidatos preferibles a una media simple cuando existen eventos extremos. La selección final de estadísticos y ventanas se determinará con pruebas de escucha y mediciones.

## 5. Referencia adaptativa dentro de la grabación

La referencia debe representar el nivel habitual de las frases fiables de esa toma. No debe ser necesariamente la frase más fuerte, la más débil ni el promedio aritmético de todas.

Proceso propuesto:

1. Excluir del conjunto de referencia frases con detección dudosa, ruido dominante, duración insuficiente o eventos extremos no representativos.
2. Agrupar o contextualizar frases por secciones cuando la estructura y la evidencia lo permitan.
3. Estimar la distribución de sonoridad robusta de las frases fiables.
4. Identificar una zona central representativa, no un outlier.
5. Evaluar si hay más de un régimen vocal legítimo (por ejemplo, verso íntimo y estribillo potente).
6. Elegir una referencia global o referencias contextuales solo si la evidencia justifica esa separación.
7. Asignar confianza a la referencia y documentar qué frases la sostienen.

No se fuerza una referencia única cuando la grabación contiene contrastes intencionales claros o cuando los datos son insuficientes.

## 6. Contrastes expresivos y contexto

El motor debe evitar igualar automáticamente:

- Susurros o finales suaves que forman parte de la interpretación.
- Subidas deliberadas de intensidad en estribillos.
- Palabras enfatizadas y ataques expresivos.
- Frases habladas, cantadas, aireadas o gritadas que cumplen funciones diferentes.
- Pausas dramáticas, respiraciones y colas naturales.

Las diferencias entre secciones se evalúan con continuidad musical, repetición de patrones, estructura de frase y consistencia de la interpretación. Una diferencia grande no es por sí sola un error.

Cuando no pueda inferirse la intención, el sistema debe marcar revisión manual o proponer una corrección conservadora con confianza baja, nunca presentar la decisión como certeza.

## 7. Cálculo de corrección relativa

Para cada frase aceptada, el motor calcula la diferencia entre su estimación robusta y la referencia seleccionada. Esa diferencia es una propuesta relativa, no una orden de aplicar toda la compensación.

La intensidad Sutil / Equilibrada / Pronunciada controla cuánto de la diferencia se propone corregir. No cambia la referencia absoluta ni convierte el proceso en compresión.

La propuesta final debe considerar:
- Confianza de segmentación y medición.
- Distancia respecto de la referencia.
- Riesgo expresivo.
- Dinámica interna que conviene preservar.
- Transiciones entre frases.
- Límites de seguridad y edición configurables, que se definirán tras las pruebas.

El motor debe poder proponer cero corrección.

## 8. Casos especiales y abstención

| Caso | Comportamiento esperado |
|---|---|
| Una sola frase fiable | No inventar una referencia estadística; usar análisis interno limitado y señalar confianza baja |
| Muy pocas frases | Ampliar contexto solo si es válido; si no, abstenerse |
| Verso y estribillo contrastantes | Evaluar referencias contextuales sin borrar el contraste |
| Susurro intencional | Protegerlo cuando haya evidencia contextual |
| Grito o nota sostenida | No dejar que un evento extremo sesgue toda la referencia |
| Ruido o bleed | Reducir confianza; evitar correcciones basadas en mediciones contaminadas |
| Frase cortada o detección incompleta | No proponer corrección automática confiada |
| Respiración dominante | Medirla aparte y evitar que determine el nivel del cuerpo vocal |
| Plosiva aislada | No interpretarla como sonoridad representativa de la frase |

## 9. Salida del módulo

Cada frase entrega al motor Clip Gain una estructura conceptual con:

- Identificador y límites temporales.
- Sonoridad robusta estimada y descriptores complementarios.
- Referencia utilizada y tipo (global, contextual o no disponible).
- Diferencia relativa estimada.
- Confianza de medición y de referencia.
- Factores de riesgo expresivo.
- Recomendación de corrección o abstención.
- Explicación legible para el usuario.

Los cinco jueces, cuando se autorice su implementación, podrán evaluar esta evidencia. No reemplazan las mediciones DSP ni aplican ganancia directamente.

## 10. Validación antes de fijar parámetros

Construir un conjunto de pruebas que incluya:
- Voz cercana y consistente.
- Versos y estribillos con contraste intencional.
- Susurros, voz aireada, voz hablada y gritos.
- Frases cortas y largas.
- Respiraciones marcadas y finales suaves.
- Plosivas y consonantes fuertes.
- Ruido de sala y bleed moderado.
- Interpretaciones con dinámica amplia y con dinámica reducida.

Evaluar tanto medidas como escucha:
- Estabilidad de la referencia ante outliers.
- Repetibilidad del análisis.
- Falsas correcciones de expresiones intencionales.
- Detección de frases realmente desparejas.
- Conservación de respiraciones, consonantes y colas.
- Naturalidad antes/después en pruebas ciegas.
- Coherencia entre sugerencia y edición manual de referencia.

No fijar tamaños de ventana, umbrales, límites de ganancia ni porcentajes definitivos hasta disponer de corpus de prueba y resultados comparables.

## 11. Secuencia de implementación

1. Implementar extracción de descriptores offline sobre ventanas.
2. Validar estimación robusta por frase con archivos sintéticos y voces reales.
3. Implementar selección de referencia con confianza y abstención.
4. Evaluar contrastes por sección y casos ambiguos.
5. Conectar las propuestas relativas con el generador de curvas Clip Gain.
6. Recién después, evaluar el comportamiento conjunto con Clip Peak y el sistema de jueces.

## 12. Decisiones pendientes

- Ventanas y solapamiento apropiados para diferentes duraciones de frase.
- Combinación de K-weighting, RMS y otros descriptores.
- Criterios de agrupación por sección.
- Confianza mínima para proponer correcciones.
- Límites y suavizado de las curvas de ganancia.
- Dataset de referencia y protocolo de escucha.

Estas decisiones quedan abiertas hasta que las pruebas permitan justificar valores concretos.
