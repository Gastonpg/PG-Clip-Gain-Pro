# Especificación 01 — Detección de actividad vocal y segmentación de frases

**Proyecto:** PG Clip Gain Pro  
**Versión:** 0.1 — especificación de diseño  
**Estado:** prototipo conservador de actividad por ventanas implementado; segmentación de frases, calibración y validación auditiva pendientes

## 1. Objetivo

Definir cómo PG identifica regiones vocales y propone límites de frases para que Clip Gain trabaje sobre unidades musicales coherentes.

La segmentación es una de las decisiones de mayor impacto: un límite incorrecto puede cortar una consonante, separar sílabas de una misma frase, alterar una respiración o crear cambios de ganancia audibles.

La prioridad es evitar límites destructivos, incluso si eso significa dejar regiones más largas o pedir revisión manual.

## 2. Salida del detector

El detector no debe devolver únicamente intervalos binarios. Debe producir regiones candidatas con:
- inicio y final estimados;
- márgenes de protección;
- estado de actividad;
- confianza de inicio y final por separado;
- indicios que justifican el límite;
- motivo de incertidumbre;
- recomendación de agrupar, separar o revisar.

Estados conceptuales:
- SILENCE_OR_BACKGROUND: silencio o fondo sin actividad vocal clara;
- POSSIBLE_VOICE_ONSET: inicio posible;
- VOICE_ACTIVE: actividad vocal probable;
- INTERNAL_PAUSE: pausa que podría pertenecer a la misma frase;
- POSSIBLE_PHRASE_END: final posible;
- UNCERTAIN: evidencia insuficiente o contradictoria.

Son etiquetas de análisis, no reconocimiento fonético.

## 3.1. Prototipo actual de actividad por ventanas

La primera implementación en C++ produce etiquetas candidatas por ventana mediante RMS representativo entre canales, cuantiles de nivel no silencioso, umbrales relativos de entrada/salida, histéresis y puente de pausas cortas. Agrupa ventanas sostenidas y pausas puenteadas en regiones candidatas con confianza descriptiva. Si no existe contraste estadístico suficiente, se abstiene y marca la evidencia como incierta. Los parámetros se suministran explícitamente y no hay un preset de producción validado. Este prototipo no reconoce voz ni decide límites fonéticos finales de frase.

## 3. Análisis por ventanas

El análisis se realiza en ventanas solapadas. El tamaño y salto definitivos se decidirán mediante pruebas con diferentes voces, tempos y frecuencias de muestreo.

Por ventana se pueden calcular:
- RMS y energía;
- pico absoluto;
- envolvente suavizada;
- cambio de energía respecto a ventanas vecinas;
- energía por bandas;
- centroide espectral y otros descriptores solo si demuestran utilidad;
- relación entre energía de voz probable y fondo;
- continuidad temporal.

No se debe seleccionar un tamaño de ventana definitivo sin medir el compromiso entre resolución temporal, estabilidad y coste de CPU.

## 4. Detección de actividad: histéresis

Usar dos niveles de decisión conceptuales:
- umbral de entrada para pasar a actividad probable;
- umbral de salida, más permisivo, para evitar cortes por pequeñas caídas de energía.

La histéresis reduce el parpadeo de estados cerca de un único umbral. Los umbrales deben adaptarse al ruido de fondo y a las características de la toma; no deben asumir que todas las grabaciones tienen el mismo nivel.

La energía no es suficiente por sí sola: una respiración, ruido o instrumento puede superar el umbral, mientras que una voz susurrada puede quedar por debajo.

## 5. Pausas y continuidad

Una pausa no equivale automáticamente al final de una frase.

Para evaluar si una pausa divide frases o es interna, considerar:
- duración;
- caída y recuperación de energía;
- continuidad espectral y tímbrica antes/después;
- presencia de respiración;
- contexto de la frase;
- cambios prosódicos observables;
- patrón de pausas de la propia toma.

Una pausa breve dentro de una unidad vocal debe tender a conservar la agrupación. Una pausa más larga aumenta la probabilidad de un nuevo límite, pero nunca debe ser el único criterio.

Si no hay evidencia suficiente, marcar el límite como incierto en vez de inventar una segmentación segura.

## 6. Protección de consonantes, respiraciones y colas

Los límites de una región editable deben diferenciarse de los límites de actividad detectada.

Para cada frase:
- detectar un inicio probable;
- detectar un final probable;
- añadir márgenes de protección configurables;
- evitar que las rampas de ganancia invadan ataques o finales importantes;
- conservar respiraciones cuando formen parte de la interpretación;
- conservar colas naturales y reverberación capturada;
- evitar cortes abruptos.

No clasificar automáticamente todas las respiraciones como ruido. La decisión de atenuarlas, conservarlas o tratarlas de forma distinta pertenece a una etapa posterior y debe ser explícita.

## 7. Agrupación en frases

La agrupación se realiza después de detectar actividad y pausas candidatas.

Reglas:
1. No separar una frase solo porque haya una caída breve de energía.
2. No fusionar automáticamente eventos separados por pausas largas.
3. Mantener contexto suficiente alrededor de palabras y respiraciones.
4. Si el límite es dudoso, preferir una región más amplia o revisión manual.
5. No afirmar que se detectaron palabras individuales si el sistema solo dispone de señales acústicas generales.
6. Evitar que una región instrumental o de ruido se convierta en frase vocal sin evidencia suficiente.

La segmentación en palabras, si se incorpora en el futuro, será una capacidad separada que requerirá validación específica.

## 8. Interfaz con Clip Gain

Cada región entregada al motor debe incluir:
- muestras de inicio y final;
- márgenes protegidos;
- estadísticas de nivel por ventanas;
- porcentaje de ventanas fiables;
- límites con confianza separada;
- indicador de pausa interna;
- advertencias por ruido, solapamiento o ambigüedad.

Clip Gain no debe tratar un límite incierto como exacto. Debe reducir la agresividad de la corrección o abstenerse cuando el error potencial sea alto.

## 9. Casos de prueba obligatorios

Crear señales sintéticas controladas y reunir grabaciones autorizadas con anotaciones humanas.

| Caso | Comportamiento esperado |
|---|---|
| Voz susurrada | No perder actividad por nivel bajo |
| Consonante inicial suave | No cortar el ataque |
| Consonante explosiva | No confundir el pico con inicio de frase aislado |
| Pausa breve entre palabras | Mantener continuidad cuando corresponda |
| Respiración entre frases | Conservarla y no forzar un corte |
| Respiración dentro de frase | No convertirla automáticamente en final |
| Final de palabra suave | Preservar la cola |
| Reverberación larga | No confundir toda la cola con nueva actividad vocal |
| Ruido de fondo | Evitar regiones vocales falsas |
| Instrumental filtrándose | Marcar incertidumbre cuando no se pueda distinguir |
| Dos frases separadas | Proponer límites sin recortar finales |
| Palabra aislada | Crear una región válida sin exigir frase larga |
| Silencio digital | No crear actividad |
| Frase con caída de volumen interna | No fragmentar por una caída expresiva |

## 10. Métricas de evaluación

Evaluar por separado:
- error temporal de inicio;
- error temporal de final;
- porcentaje de consonantes/colas cortadas;
- falsos inicios y falsos finales;
- pausas internas clasificadas erróneamente;
- regiones vocales omitidas;
- regiones no vocales incluidas;
- tasa de abstención en casos ambiguos.

No optimizar únicamente una métrica promedio: los errores graves de corte deben tener un peso mucho mayor que pequeñas diferencias temporales inocuas.

Las anotaciones humanas deben permitir límites alternativos razonables cuando la interpretación no tenga un único límite objetivo.

## 11. Secuencia de implementación propuesta

1. Implementar visualización y registro de envolvente/descriptores sin editar audio.
2. Implementar detector inicial de actividad con histéresis.
3. Añadir contexto de pausas y agrupación de regiones.
4. Añadir márgenes protegidos y límites inciertos.
5. Construir pruebas unitarias con señales sintéticas.
6. Evaluar con grabaciones reales autorizadas.
7. Ajustar parámetros y repetir escucha ciega.
8. Solo después conectar la segmentación al cálculo de Clip Gain.

## 12. Criterios para avanzar

No automatizar ganancia sobre frases hasta que:
- los casos de prueba mínimos estén cubiertos;
- los errores de corte estén cuantificados;
- las regiones dudosas se identifiquen correctamente;
- las consonantes, respiraciones y colas estén razonablemente protegidas;
- el comportamiento sea estable en distintas tomas;
- el coste computacional sea aceptable.

**Importante:** esta especificación no declara resuelto el algoritmo. Define el comportamiento que debe investigarse, implementarse y demostrarse mediante pruebas.
