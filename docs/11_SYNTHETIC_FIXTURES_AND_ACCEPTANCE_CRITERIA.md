# PG Clip Gain Pro — Banco de señales sintéticas y criterios de aceptación

**Estado:** Plan de fixtures v0.1 — especificación para implementar en el banco de pruebas; todavía no ejecutado.
**Relación:** complementa `06_VALIDATION_AND_TEST_PROTOCOL.md` y `10_DSP_DATA_MODEL_AND_NUMERICAL_CONTRACTS.md`.

## 1. Objetivo

Crear señales deterministas que permitan probar una propiedad por vez antes de usar grabaciones vocales. Cada fixture debe tener:
- generador y semilla reproducibles;
- frecuencia de muestreo, canales y duración declarados;
- eventos con límites exactos en frames;
- comportamiento esperado;
- tolerancia numérica definida por la naturaleza de la prueba;
- identificador estable y versión.

Las señales sintéticas verifican matemáticas, continuidad, captura y fallos conocidos. No prueban por sí solas musicalidad, inteligibilidad ni calidad profesional.

## 2. Convenciones del banco

- Generar fixtures en memoria; no depender de archivos externos para las pruebas unitarias.
- Mantener una versión PCM de referencia cuando la prueba requiera comparar muestras.
- Anotar todos los eventos como intervalos `[startSample, endSample)`.
- Usar semillas fijas para ruido y variaciones pseudoaleatorias.
- Evitar normalizar cada fixture automáticamente: su amplitud forma parte del caso.
- Comparar resultados entre tamaños de bloque distintos para detectar dependencia del host.
- Separar pruebas unitarias, integración de motores y escucha.

## 3. Fixtures de captura y dominio temporal

| ID | Señal / condición | Resultado exigido |
|---|---|---|
| CAP-001 | Bloques contiguos desde el frame cero | Captura completa, orden y longitud exactos |
| CAP-002 | Un bloque repetido | Detectar solapamiento; no duplicar muestras silenciosamente |
| CAP-003 | Un bloque perdido | Marcar discontinuidad y captura parcial |
| CAP-004 | Bloques recibidos fuera de orden | Rechazar o reordenar solo si el índice de posición lo permite inequívocamente |
| CAP-005 | Seek durante captura | Crear segmento/discontinuidad nueva; no unir tiempos incompatibles |
| CAP-006 | Captura detenida antes del final | Estado parcial; impedir referencia global de toma completa |
| CAP-007 | Misma señal con varios tamaños de bloque | Mismos límites y resultados equivalentes dentro de tolerancia |
| CAP-008 | Varias frecuencias de muestreo | Conversión temporal correcta y sin deriva acumulada |

## 4. Fixtures numéricos

| ID | Señal / condición | Resultado exigido |
|---|---|---|
| NUM-001 | Silencio digital | Estado de silencio; no contamina estadísticas de voz |
| NUM-002 | Seno estable | RMS y pico coinciden con referencia analítica dentro de tolerancia |
| NUM-003 | Señal con NaN | Marcar muestras/ventanas inválidas; no propagar NaN |
| NUM-004 | Señal con infinito | Marcar error numérico; no generar curvas |
| NUM-005 | Valores extremos y subnormales | No desbordar ni bloquear; registrar tratamiento |
| NUM-006 | Señal menor que una ventana | Resultado insuficiente, sin leer fuera de rango |
| NUM-007 | Impulso en primer y último frame | Bordes y límites correctos |
| NUM-008 | Canales estéreo opuestos en polaridad | No declarar silencio por suma mono sin diagnóstico |

## 5. Fixtures de segmentación

| ID | Señal / condición | Resultado exigido |
|---|---|---|
| SEG-001 | Ráfaga con rampas de entrada/salida | Detectar actividad sin recortar el inicio/final real |
| SEG-002 | Serie de sílabas simuladas separadas por pausas breves | No fragmentar por cada micro pausa |
| SEG-003 | Dos frases separadas por pausa larga | Proponer regiones separadas |
| SEG-004 | Actividad débil próxima al ruido | Reducir confianza o abstenerse |
| SEG-005 | Respiración sintética entre frases | Clasificar como candidato protegido, no como cuerpo vocal por defecto |
| SEG-006 | Cola exponencial posterior a actividad | Mantener cola como región contextual; no cortar abruptamente |
| SEG-007 | Ataque impulsivo antes de energía estable | No asumir que todo ataque es inicio de frase ni eliminarlo |
| SEG-008 | Fondo estacionario con transitorios aleatorios | Evitar crear frases a partir de cada transitorio |

Los fixtures sintéticos de segmentación deben llamarse señales proxy, no simulaciones fonéticas realistas.

## 6. Fixtures de sonoridad y referencia

| ID | Señal / condición | Resultado exigido |
|---|---|---|
| LOU-001 | Frases idénticas a distintos niveles | Orden de nivel correcto y diferencias relativas coherentes |
| LOU-002 | Una frase extrema entre frases estables | El outlier no arrastra por sí solo la referencia |
| LOU-003 | Frase corta con un impulso alto | El impulso no domina automáticamente el perfil |
| LOU-004 | Susurro separado de frases normales | No elevarlo automáticamente sin evidencia contextual |
| LOU-005 | Dos grupos con niveles distintos y estructura declarada | Permitir referencias contextuales sin igualar los grupos |
| LOU-006 | Solo una frase fiable | No inventar distribución; confianza limitada o abstención |
| LOU-007 | Silencio añadido antes/después | La referencia no cambia por duración de silencio |
| LOU-008 | Respiración aislada de energía alta | No convertirla en referencia del cuerpo vocal |

El generador no puede etiquetar intención artística. Los casos de intención solo se validan con anotación humana en audio real.

## 7. Fixtures de Clip Gain

| ID | Señal / condición | Resultado exigido |
|---|---|---|
| CG-001 | Frases con diferencias de nivel conocidas | Propuesta relativa en sentido correcto |
| CG-002 | Todas las frases con el mismo nivel | Curva neutra salvo edición manual |
| CG-003 | Cambio gradual dentro de una frase | Preservar la forma dinámica interna |
| CG-004 | Frases contiguas | Transición continua sin salto en límites |
| CG-005 | Región protegida | No modificarla automáticamente |
| CG-006 | Intensidades Sutil/Equilibrada/Pronunciada | Orden monotónico de fuerza, sin cambiar referencia |
| CG-007 | Corrección manual bloqueada y nuevo análisis | Mantener edición manual y registrar conflicto |
| CG-008 | Frase de confianza insuficiente | Cero corrección automática o estado de revisión |

## 8. Fixtures de Clip Peak

| ID | Señal / condición | Resultado exigido |
|---|---|---|
| CP-001 | Impulso anómalo sobre cuerpo estable | Candidato localizado si el contexto lo justifica |
| CP-002 | Transitorio alto declarado legítimo | No forzar atenuación por amplitud sola |
| CP-003 | Ataque con rampa y decaimiento | Mantener ataque si no hay evidencia de anomalía |
| CP-004 | Evento largo de energía elevada | No tratarlo como pico breve automáticamente |
| CP-005 | Evento en región protegida | Abstenerse o pedir revisión |
| CP-006 | Evento saturado de origen | No afirmar que la atenuación repara clipping |
| CP-007 | Corrección manual bloqueada | Preservar la curva manual |
| CP-008 | Evento ambiguo | Confianza reducida y abstención/revisión explícita |

## 9. Fixtures de coordinación

| ID | Señal / condición | Resultado exigido |
|---|---|---|
| CO-001 | Curvas independientes sin solapamiento | Composición no altera regiones ajenas |
| CO-002 | Clip Gain y Clip Peak coinciden temporalmente | Detectar solapamiento y calcular resultado compuesto |
| CO-003 | Resultado compuesto excede límite configurado | Bloquear, reducir o pedir revisión según política explícita |
| CO-004 | Desactivar Clip Peak | Clip Gain permanece idéntico |
| CO-005 | Desactivar Clip Gain | Clip Peak permanece idéntico |
| CO-006 | Cambiar snapshot durante procesamiento | El hilo de audio ve estado completo anterior o nuevo, nunca mezcla parcial |
| CO-007 | Deshacer y rehacer | Recuperar exactamente estados previos |

## 10. Fixtures de curva y procesamiento

- Curva neutra: salida idéntica a entrada dentro de tolerancia.
- Curva constante: factor de amplitud coincide con conversión dB.
- Rampa de entrada/salida: sin discontinuidad de valor.
- Punto de cambio exactamente en frontera de bloque: sin salto.
- Seek hacia delante/atrás: evaluación temporal correcta.
- Loop: repetición coherente sin acumular estado.
- Cambio de tamaño de bloque: curva audible y numéricamente equivalente.
- Valores fuera de límites: rechazo controlado antes de llegar al hilo de audio.

## 11. Criterios de aceptación del banco

El banco se considera implementado cuando:
1. cada fixture tiene generador determinista y prueba automatizada;
2. los tests reportan ID, configuración, versión y motivo de fallo;
3. las tolerancias se justifican por el algoritmo y la precisión numérica;
4. existe prueba de regresión para cada defecto corregido;
5. los resultados son reproducibles en ejecución repetida;
6. los tests no dependen del orden de ejecución;
7. se separan claramente resultados sintéticos y conclusiones de escucha vocal.

No se declara aprobado un motor por pasar fixtures sintéticos. La aprobación requiere además corpus vocal autorizado, anotación, escucha A/B y pruebas de rendimiento en hosts reales.

## 12. Siguiente paso de ingeniería

Implementar el generador de señales y las pruebas unitarias de dominio temporal y numérico antes de implementar heurísticas vocales. Esto crea una base verificable para detectar errores de captura, unidades y curvas desde el primer commit de código DSP.
