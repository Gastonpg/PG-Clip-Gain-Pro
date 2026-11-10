# PG Audio Intelligence — Especificación maestra inicial

**Proyecto:** PG Clip Gain Pro  
**Estado:** Diseño técnico v0.1 — documento fundacional, no implementación validada  
**Idioma de trabajo:** Español  
**Principio rector:** *No queremos que el plugin haga más procesamiento. Queremos que tome mejores decisiones.*

---

## 1. Propósito

PG Audio Intelligence es el sistema de análisis, evaluación y recomendación que acompaña a los motores DSP de PG Clip Gain Pro. Su objetivo es proponer ediciones de Clip Gain y Clip Peak cuidadosas, justificables y reversibles, con criterios inspirados en prácticas profesionales de mezcla vocal.

No debe presentarse como una emulación de la mente, el oído ni el estilo personal de ningún ingeniero. Los ingenieros y mezcladores estudiados son referencias educativas para extraer principios técnicos y musicales documentados. Las reglas, pruebas, código y decisiones de PG deben ser originales y verificables.

## 2. Principios no negociables

1. **Naturalidad antes que uniformidad.** Consistencia de nivel no significa borrar la dinámica interpretativa.
2. **No corregir por corregir.** Si la evidencia es débil o ambigua, recomendar no modificar.
3. **Clip Gain puro.** No introducir compresión, limitación, saturación ni procesamiento dinámico automático.
4. **Dos motores independientes.** Clip Gain y Clip Peak tienen objetivos y curvas separados.
5. **Análisis offline.** El análisis puede realizarse antes de reproducir/procesar; no se colocan agentes de IA en el camino crítico de audio en tiempo real.
6. **Control humano.** Las sugerencias son revisables, editables, aceptables o rechazables. Ningún juez altera audio directamente.
7. **Decisiones trazables.** Toda sugerencia debe incluir evidencia, magnitud, zona afectada, confianza y posibles riesgos.
8. **Conservación de la intención.** Ante duda razonable entre error y expresión, preservar la interpretación.
9. **Sin objetivo absoluto prefijado.** No imponer -18 dBFS ni otro nivel universal. La referencia se obtiene de la propia toma.
10. **Eficiencia local.** Diseñar para CPU modesta y memoria limitada; evitar modelos grandes y dependencias de nube.

## 3. Arquitectura general

El sistema se divide en cuatro capas:

### A. Análisis DSP compartido

El análisis compartido se ejecuta en dos pasadas offline: primero describe la señal y genera candidatos; después refina regiones, frases, protecciones y perfiles contextuales. El snapshot común resultante lo consumen ambos motores y, en una fase futura autorizada, los jueces:

- actividad vocal y regiones candidatas;
- límites probables de palabras/frases y pausas;
- envolvente de energía y medidas de sonoridad por ventanas;
- estadísticas robustas por fragmento;
- ataques, consonantes, transitorios y picos locales;
- continuidad temporal, respiraciones y colas;
- estructura relativa de secciones cuando pueda inferirse con suficiente confianza.

La pasada 1 no genera correcciones. La pasada 2 tampoco decide magnitudes de ganancia: entrega un snapshot común e inmutable. El análisis no debe depender únicamente de un umbral de energía ni tratar cada silencio como un límite de frase. Debe combinar histéresis, duración, continuidad, contexto y señales prosódicas. Si no hay suficiente evidencia para clasificar una región, debe marcarla como incierta.

### B. Motores de edición independientes

**Motor Clip Gain**
- Detecta y agrupa fragmentos vocales sin cortar consonantes, respiraciones ni colas.
- Estima una referencia robusta representativa de la grabación.
- Compara fragmentos y secciones con esa referencia.
- Calcula correcciones relativas y conservadoras.
- Conserva la dinámica interna y suaviza transiciones.
- Expone controles Sutil / Equilibrada / Pronunciada como cantidad de corrección, nunca como destino de loudness.
- Permite editar manualmente cada región y su ganancia.

**Motor Clip Peak**
- Busca excursiones locales anómalas, no simplemente los valores máximos.
- Compara amplitud y energía del evento con su contexto inmediato.
- Distingue picos anómalos de consonantes explosivas y transitorios expresivos.
- Propone atenuación localizada con curvas suaves y ajustables.
- Permite corrección manual con herramienta de dibujo.
- No funciona como compresor ni limitador.

### C. Cinco jueces especializados

Los jueces reciben descriptores DSP y propuestas preliminares. No sustituyen el análisis acústico ni inventan información que no esté en los datos.

### D. Juez principal y registro de decisiones

El juez principal revisa conflictos, aplica las reglas de seguridad y produce una decisión final: aprobar, reducir, pedir revisión manual o abstenerse. Cada decisión queda registrada para pruebas y auditoría.

---

## 4. Contrato común de los jueces

Cada juez debe devolver una evaluación estructurada con:

- identificador del fragmento/evento;
- dominio evaluado;
- observaciones basadas en datos disponibles;
- evidencia y métricas utilizadas;
- hipótesis de problema (si existe);
- propuesta de acción o abstención;
- magnitud sugerida y límites;
- confianza calibrada;
- riesgo de alterar intención, consonantes, respiraciones o continuidad;
- razones para no intervenir;
- versión de reglas y parámetros que generó la decisión.

Los jueces no pueden:
- inventar palabras, intención emocional o estructura musical;
- inferir que un pico es malo solo por ser alto;
- convertir una preferencia estilística en una regla universal;
- proponer cambios fuera de su especialidad sin declararlo;
- aprobar una corrección sin evidencia;
- cambiar el audio directamente.

La confianza no es una probabilidad válida hasta que se calibre con un conjunto de evaluación etiquetado. En la primera versión debe tratarse como una puntuación interna y documentada, no como certeza estadística.

## 5. Los cinco jueces

### Juez 1 — Interpretación y preservación expresiva

**Misión:** proteger la intención musical y detectar cuándo una diferencia de nivel puede ser expresiva.

Evalúa:
- cambios de intensidad dentro de la frase;
- contraste entre palabras y finales de frase;
- continuidad de respiraciones y colas;
- señales disponibles de énfasis y dinámica interpretativa;
- ambigüedad entre variación expresiva y desequilibrio accidental.

Regla central: no afirmar que conoce la intención del cantante. Solo puede identificar indicios y nivel de incertidumbre.

Salida típica:
- preservar;
- permitir una corrección reducida;
- pedir revisión humana;
- abstenerse por falta de evidencia.

### Juez 2 — Nivelación vocal

**Misión:** evaluar diferencias relativas de sonoridad entre palabras, frases y bloques.

Evalúa:
- medidas perceptuales por ventanas;
- estadísticas robustas que reduzcan la influencia de consonantes y respiraciones;
- referencia interna representativa;
- diferencias locales y globales;
- intensidad de corrección seleccionada por el usuario.

Regla central: no perseguir igualdad matemática ni fijar un nivel absoluto. Debe proponer el menor ajuste que resuelva una diferencia claramente problemática.

### Juez 3 — Transitorios y picos

**Misión:** encontrar anomalías locales sin eliminar ataques útiles.

Evalúa:
- pico y energía local;
- relación con el entorno temporal;
- duración y forma del evento;
- repetición y contexto;
- probabilidad de que sea una consonante o transitorio natural.

Regla central: un pico alto no es automáticamente un pico defectuoso. La acción, si procede, debe ser local y limitada.

### Juez 4 — Coherencia global

**Misión:** comprobar la relación de nivel entre secciones y a lo largo de la canción.

Evalúa:
- distribución de sonoridad entre grupos de frases;
- contraste relativo entre estrofas, pre-estribillos y estribillos cuando la estructura pueda inferirse;
- consistencia sin borrar la progresión musical;
- interacciones entre ajustes locales y balance global.

Regla central: no tratar cada sección como una isla ni forzar que todas tengan el mismo nivel.

### Juez 5 — Juez principal / árbitro

**Misión:** integrar las evaluaciones y evitar decisiones contraproducentes.

Responsabilidades:
- detectar contradicciones entre jueces;
- priorizar preservación expresiva y seguridad;
- rechazar acciones con evidencia insuficiente;
- reducir magnitudes excesivas;
- resolver conflictos entre balance local y global;
- impedir que Clip Gain y Clip Peak corrijan dos veces el mismo evento de forma perjudicial;
- solicitar revisión humana cuando la ambigüedad sea relevante.

Decisiones permitidas:
- aprobar;
- aprobar con magnitud reducida;
- rechazar;
- solicitar revisión manual;
- abstenerse.

El juez principal no puede convertir desacuerdos en una falsa certeza.

---

## 6. Influencias y biblioteca de conocimiento profesional

Se estudiarán materiales públicos y legítimamente accesibles de ingenieros y educadores de mezcla vocal, entre ellos Jaycen Joshua, Chris Lord-Alge, Manny Marroquin, Serban Ghenea, Dave Pensado, Andrew Scheps, Leslie Brathwaite, Michael Brauer, Tony Maserati, Tom Elmhirst y Spike Stent, además de otras referencias pertinentes.

La biblioteca debe registrar para cada principio:
- fuente, autor, fecha y enlace cuando corresponda;
- si es una declaración directa, demostración, análisis técnico o interpretación del equipo PG;
- principio extraído;
- contexto y límites de aplicación;
- juez al que aporta;
- ejemplo de uso y contraejemplo;
- prueba que permita verificarlo.

No atribuir a un ingeniero una regla que no haya expresado o demostrado públicamente. No copiar cursos, libros, presets, sesiones ni material protegido. No usar nombres, imágenes ni reputaciones como aval comercial implícito.

El objetivo no es mezclar estilos personales de forma indiscriminada, sino extraer principios compatibles con la filosofía de PG: musicalidad, control, moderación, claridad y respeto por la interpretación.

## 7. Relación con EGM

EGM puede estudiarse como referencia para organizar revisión de especificaciones, crítica independiente, síntesis de desacuerdos y control de calidad del desarrollo de software.

EGM no es un motor de análisis de audio ni aporta jueces que ya sepan mezclar. Su posible adaptación pertenece al flujo de ingeniería del proyecto. PG Audio Intelligence es un sistema separado, definido por descriptores DSP, reglas especializadas y validación con audio.

## 8. Diseño del algoritmo: aspectos que aún requieren especificación y prueba

No se considera cerrado el algoritmo DSP hasta definir y validar:

1. **Detección vocal y segmentación:** combinación de envolvente, histéresis, pausas, continuidad y señales prosódicas; límites seguros alrededor de consonantes, respiraciones y colas.
2. **Medición de sonoridad por fragmento:** ventanas y ponderación perceptual; medidas robustas que no permitan que una explosiva o respiración domine la estimación.
3. **Referencia adaptativa:** selección de fragmentos representativos de la propia toma; tratamiento de outliers y secciones intencionalmente distintas.
4. **Mapa de ganancia:** interpolación, rampas, límites de pendiente, prevención de clics y preservación de dinámica interna.
5. **Detector de picos:** definición de anomalía relativa al contexto, duración, energía y tipo probable de transitorio.
6. **Interacción de motores:** evitar doble corrección y conservar independencia entre Clip Gain y Clip Peak.
7. **Coste computacional:** memoria, tiempo de análisis y comportamiento en sesiones largas en hardware modesto.

Las decisiones de implementación se documentarán junto con sus alternativas y pruebas. No elegir parámetros definitivos por intuición sin escuchar y medir resultados.

## 9. Plan de validación

Construir un corpus de pruebas con grabaciones autorizadas y casos sintéticos controlados. Cada caso debe tener anotación humana y una explicación del resultado esperado.

Casos mínimos:
- susurro seguido de frase fuerte;
- palabra aislada;
- consonantes explosivas;
- respiraciones cortas y largas;
- finales de frase y colas;
- frases de distinta intensidad;
- pico anómalo sobre una frase correctamente nivelada;
- estrofa frente a estribillo;
- subida emocional intencional;
- pausa interna que no debe dividir una frase;
- ruido o instrumental filtrándose en la toma;
- fragmentos ambiguos donde la salida correcta sea abstenerse.

Métricas:
- falsos positivos y falsos negativos de segmentación;
- errores de clasificación de picos;
- desviación de ganancia respecto a anotaciones expertas;
- porcentaje de intervenciones rechazadas por revisión humana;
- preservación de respiraciones, consonantes y colas;
- naturalidad y consistencia mediante escucha ciega;
- tiempo de análisis, memoria y carga de CPU.

Una mejora numérica no se considera éxito si degrada la naturalidad en pruebas de escucha.

## 10. Fases de desarrollo

**Fase 1 — Especificación:** cerrar contratos de datos, segmentación, medición, referencia y curvas.

**Fase 2 — Prototipo DSP:** implementar análisis offline y generar visualizaciones/datos de diagnóstico sin GUI final.

**Fase 3 — Banco de pruebas:** probar casos controlados y comparar con anotaciones humanas.

**Fase 4 — Jueces deterministas (requiere autorización del usuario):** implementar primero reglas y evaluadores especializados, reproducibles y económicos. No forma parte de la base DSP ni puede iniciarse sin permiso explícito.

**Fase 5 — Arbitraje y sugerencias:** incorporar juez principal, explicaciones y aceptar/rechazar/editar.

**Fase 6 — IA opcional:** evaluar si un modelo local pequeño aporta valor real en casos ambiguos. Nunca depender de él para el procesamiento de audio ni para la seguridad básica.

**Fase 7 — GUI y producto:** construir la interfaz definitiva después de validar el comportamiento sonoro.

## 11. Criterio de salida

PG Audio Intelligence no se considerará listo por tener cinco nombres, prompts o agentes funcionando. Se considerará listo cuando:
- sus decisiones sean reproducibles;
- sus límites estén documentados;
- se abstenga correctamente ante ambigüedad;
- supere pruebas técnicas y de escucha;
- no perjudique la interpretación;
- funcione localmente con recursos modestos;
- el usuario pueda entender y controlar cada sugerencia.

---

**Estado actual:** base conceptual consolidada. Todas las métricas, umbrales, magnitudes y reglas operativas quedan pendientes de validación experimental.