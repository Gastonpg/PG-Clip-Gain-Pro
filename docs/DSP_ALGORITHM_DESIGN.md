# PG Clip Gain Pro — Diseño del algoritmo DSP

**Estado:** Diseño preliminar v0.1 — pendiente de prototipo y validación con audio  
**Alcance:** análisis offline, Clip Gain y Clip Peak  
**Restricción central:** no compresión, no limitación, no saturación y ningún objetivo absoluto de loudness.

Este documento transforma la filosofía del producto en un flujo de procesamiento verificable. No fija todavía umbrales definitivos: deben calibrarse con pruebas y escucha.

## 1. Flujo general

1. Leer la señal sin modificarla y conservar una copia de referencia.
2. Analizar por bloques con solapamiento, manteniendo continuidad entre bloques.
3. Calcular descriptores de actividad, envolvente, energía, sonoridad, transitorios y continuidad.
4. Proponer regiones vocales y límites candidatos con una puntuación de confianza.
5. Agrupar regiones en frases y conservar límites inciertos para revisión.
6. Calcular estadísticas robustas por frase y por grupos de frases.
7. Construir propuestas independientes de Clip Gain y Clip Peak.
8. Ejecutar verificaciones de seguridad y evitar doble corrección.
9. Presentar las curvas y sugerencias al usuario; no aplicar cambios irreversibles.

## 2. Análisis compartido en dos pasadas\n\nLa adquisición puede requerir reproducir la región para que el host entregue sus bloques. La captura se realiza en el recorrido de audio con operaciones acotadas; el análisis complejo se ejecuta después en un worker, fuera del hilo de audio. La pasada 1 produce el mapa descriptivo; la pasada 2 lo refina con contexto. La definición detallada está en `12_TWO_PASS_SHARED_ANALYSIS_PIPELINE.md`.

### 2.1 Preparación de señal

- Mantener la señal original y trabajar con una representación de análisis mono derivada cuando corresponda.
- Si hay canales estéreo, comprobar correlación y diferencias antes de combinarlos; no asumir que una suma mono es siempre inocua.
- Normalizar solo las escalas internas de cálculo si hace falta; nunca normalizar destructivamente el audio fuente.
- Tratar NaN, infinito, silencio digital y archivos muy cortos de forma explícita.

### 2.2 Descriptores temporales

Calcular por ventanas solapadas:
- pico absoluto;
- RMS/energía;
- envolvente suavizada;
- energía por bandas si aporta robustez;
- medida perceptual basada en ponderación K como descriptor auxiliar;
- crest factor y estadísticas de transitorios;
- ruido de fondo estimado con cautela.

La sonoridad integrada de toda la canción no sustituye a una medida local por frase. La medición inspirada en ITU-R BS.1770 puede aportar información, pero la referencia de Clip Gain debe ser específica de voz y robusta frente a consonantes, respiraciones y silencios.

## 3. Segmentación vocal y límites de frase

### 3.1 Principio

La detección de actividad no debe equivaler a «energía por encima de un umbral». Debe combinar:
- energía y envolvente;
- histéresis de entrada/salida;
- duración mínima de actividad y pausa;
- continuidad entre sílabas;
- ataques y colas;
- contexto temporal alrededor de una pausa;
- señales prosódicas disponibles.

No se debe afirmar detección fonética perfecta si no existe un modelo que la respalde. Los límites son candidatos con incertidumbre.

### 3.2 Estados de actividad

Representar cada ventana con estados posibles:
- inactiva;
- inicio probable;
- actividad estable;
- final probable;
- pausa interna;
- región ambigua.

La histéresis evita alternancias rápidas cerca del umbral. Las duraciones mínimas evitan fragmentar una frase por micro pausas. Las pausas largas son evidencia, no prueba automática de final de frase.

### 3.3 Protección de bordes

Cada región debe mantener margen contextual para:
- consonantes iniciales suaves;
- consonantes finales;
- respiraciones cercanas;
- reverberación/colas capturadas;
- transiciones entre palabras.

No aplicar cortes duros. Los límites de ganancia deben ser continuos y con rampas configurables. Si el límite es incierto, ampliar región protegida o solicitar revisión.

## 4. Estimación de nivel por frase

Para cada frase candidata:
1. Excluir silencios y ventanas de baja confianza.
2. Reducir el peso de transitorios muy breves que no representan el cuerpo de la frase.
3. Evitar que respiraciones aisladas dominen la estadística.
4. Calcular más de un descriptor robusto (por ejemplo mediana y percentiles de energía/sonoridad por ventana).
5. Conservar indicadores de variabilidad interna para detectar frases muy dinámicas.

La medida representativa debe describir el cuerpo vocal, no solo el máximo ni el promedio bruto de toda la región.

## 5. Referencia adaptativa

La referencia se obtiene de la propia toma, sin destino fijo.

Candidato de diseño:
- construir una distribución de niveles representativos de frases fiables;
- identificar el conjunto central de frases estables;
- reducir influencia de outliers, susurros intencionales, gritos y regiones dudosas;
- comprobar consistencia entre grupos/secciones;
- conservar contraste musical cuando esté respaldado por la estructura de la interpretación.

La referencia no debe ser necesariamente la mediana de todas las frases si la grabación contiene secciones intencionalmente distintas. El juez de coherencia puede señalar agrupaciones, pero no debe inventar estructura.

## 6. Cálculo de Clip Gain

Para cada frase fiable:
- comparar su nivel representativo con la referencia local/global;
- obtener una diferencia relativa;
- aplicar un factor de intensidad Sutil / Equilibrada / Pronunciada;
- limitar correcciones máximas y pendientes mediante parámetros conservadores;
- reducir o abstenerse ante indicadores deterministas de variación expresiva y ambigüedad; cualquier evaluación de jueces queda para una fase posterior autorizada;
- abstenerse cuando el resultado dependa de una clasificación incierta.

La corrección debe preservar la dinámica dentro de la frase. No se debe calcular una ganancia distinta para cada sílaba como comportamiento predeterminado.

### 6.1 Curva temporal

Representar cada cambio como una curva continua:
- entrada gradual;
- zona estable;
- salida gradual;
- continuidad entre regiones adyacentes;
- ausencia de saltos abruptos;
- control de pendiente para evitar modulación audible.

La curva debe permitir edición manual por región y recalcular transiciones vecinas sin alterar el resto de la sesión.

## 7. Cálculo de Clip Peak

El detector analiza eventos locales:
- amplitud máxima;
- energía integrada del evento;
- duración;
- diferencia con el nivel circundante;
- forma de ataque/caída;
- repetición;
- contexto probable (consonante, golpe, ruido o anomalía).

Un evento solo se considera candidato si sobresale respecto a su contexto y su forma/energía sugieren una anomalía. La amplitud por sí sola nunca basta.

La corrección se limita al área necesaria con rampas suaves. Debe evitar:
- aplanar consonantes;
- alterar el timbre por cambios demasiado abruptos;
- atenuar todo el cuerpo de una palabra por un pico breve;
- crear un segundo problema al superponerse con Clip Gain.

## 8. Coordinación entre motores

Los motores mantienen mapas separados. Un coordinador verifica:
- si ambos actúan sobre el mismo evento;
- si Clip Gain ya reduce el fragmento;
- si Clip Peak necesita todavía una corrección local;
- si la combinación excede límites conservadores;
- si la suma de curvas genera transiciones o cambios audibles.

La independencia significa que cada motor tiene diagnóstico y curva propios; no significa ignorar el efecto combinado. El coordinador puede reducir, posponer o rechazar una propuesta.

## 9. Papel de los jueces

Los cinco jueces no sustituyen las medidas DSP:
- Interpretación evalúa riesgo expresivo y ambigüedad.
- Nivelación evalúa comparaciones entre frases y referencia.
- Transitorios evalúa anomalías locales.
- Coherencia evalúa relaciones entre secciones.
- Juez principal arbitra, verifica límites y puede abstenerse.

En primera implementación, estos jueces deben ser evaluadores deterministas basados en características y reglas auditables. Un modelo de lenguaje o IA local puede ayudar a explicar o priorizar casos en una fase posterior, pero no debe inventar descriptores ni operar el audio.

## 10. Pruebas unitarias y de escucha

Crear señales sintéticas para verificar límites exactos y grabaciones autorizadas para validar naturalidad.

Matriz mínima:
- silencio digital;
- ruido de fondo estable;
- respiración antes de frase;
- respiración dentro de frase;
- frase con pausa interna;
- consonantes /p/, /t/, /k/ y /s/;
- susurro seguido de voz fuerte;
- palabra aislada;
- final de palabra con cola;
- frase larga con dinámica interna;
- frase corta naturalmente más intensa;
- estribillo intencionalmente más fuerte;
- pico aislado artificial sobre frase estable;
- doble evento Clip Gain + Clip Peak;
- regiones ambiguas donde la salida esperada sea no actuar.

Comprobar:
- continuidad de las curvas;
- ausencia de clics;
- límites fuera de consonantes/colas cuando sea posible;
- ganancia aplicada exactamente donde corresponde;
- no exceder límites;
- comportamiento determinista;
- uso de CPU y memoria en sesiones largas.

## 11. Decisiones pendientes

No fijar valores hasta tener banco de pruebas:
- tamaño y salto de ventana;
- constantes de ataque/caída de envolvente;
- histéresis y duración de pausas;
- reglas de agrupación de frases;
- percentiles/estadísticas de nivel;
- método final de referencia adaptativa;
- límite de ganancia y pendiente;
- duración de rampas;
- umbrales relativos para anomalías de pico;
- reglas de interacción entre motores.

Cada decisión debe tener: hipótesis, alternativas, casos de prueba, mediciones, escucha A/B y justificación.

## 12. Criterio de aceptación del algoritmo

El algoritmo se acepta solo cuando demuestra:
- segmentación suficientemente segura;
- pocas correcciones falsas;
- preservación de consonantes, respiraciones y colas;
- balance perceptual mejorado sin uniformidad artificial;
- detección de picos realmente anómalos;
- curvas suaves y sin artefactos;
- rendimiento aceptable en hardware modesto;
- abstención correcta ante casos ambiguos.

**Estado actual:** diseño de flujo y criterios. No hay parámetros calibrados ni rendimiento medido todavía.
