# 03 — Generación y edición de curvas Clip Gain

Estado: diseño de algoritmo; no fija parámetros definitivos sin validación.  
Proyecto: PG Clip Gain Pro.

## 1. Propósito

Convertir las recomendaciones relativas del análisis de frases en una curva de ganancia de clip editable, suave y reversible. La curva modifica el nivel de la señal mediante ganancia estática variable en el tiempo; no debe comportarse como compresor, limitador, saturador ni procesador dinámico basado en el nivel instantáneo.

El usuario conserva el control final. El análisis propone; la curva representa; la edición manual prevalece.

## 2. Modelo conceptual

La curva se representa como una serie de segmentos y puntos de control temporales expresados en dB de ganancia relativa. La curva debe poder evaluarse en cualquier instante y convertirse a una envolvente lineal de ganancia para el procesamiento de audio.

Conceptualmente:

- La señal original permanece intacta.
- La curva de Clip Gain define una ganancia estática para cada instante.
- Las regiones no seleccionadas o no aprobadas permanecen en ganancia unitaria (0 dB), salvo que el usuario haya dibujado una curva manual.
- Las transiciones se interpolan de forma continua para evitar discontinuidades.
- La curva se guarda como datos editables, no como audio destructivamente procesado.

La interpolación exacta (lineal, curva suave u otra) debe elegirse con pruebas de escucha y de estabilidad.

## 3. De la recomendación a la curva

Para cada frase, el motor recibe:
- Límites temporales y confianza de segmentación.
- Nivel robusto estimado.
- Referencia global o contextual y su confianza.
- Diferencia relativa estimada.
- Intensidad elegida por el usuario: Sutil, Equilibrada o Pronunciada.
- Riesgos expresivos y regiones protegidas.

El motor calcula una corrección propuesta limitada por confianza, riesgo y límites configurables. La intensidad controla la fracción de corrección aplicada a la diferencia relativa; no modifica la referencia ni impone un nivel absoluto.

Si la referencia o la medición no son fiables, la propuesta puede ser cero o quedar pendiente de revisión.

## 4. Conservación de la interpretación

No convertir toda frase en un bloque plano. La corrección de frase debe preservar la dinámica interna y la intención:

- Mantener ataques, consonantes, sílabas enfatizadas y finales expresivos.
- Evitar que una corrección global de frase elimine el crescendo o diminuendo interno.
- No elevar automáticamente respiraciones aisladas ni ruido entre palabras.
- Evitar cambios bruscos justo en límites fonéticos.
- Respetar regiones protegidas por el detector de actividad y por la revisión manual.

La frase es una unidad de contexto para estimar nivel, no una orden para aplicar una ganancia constante a todo su contenido.

## 5. Límites y transiciones

Cada corrección necesita una región de entrada, una región útil y una región de salida. Las rampas deben ser suficientemente suaves para evitar clics y cambios audibles, pero no tan largas que invadan palabras vecinas o borren la intención rítmica.

La duración de las rampas no se fija aquí: dependerá de la distancia entre frases, el tempo implícito, la actividad vocal, los límites fonéticos y la forma de onda.

Reglas:
- No cruzar silenciosamente a otra frase cuando exista una pausa que deba preservarse.
- No crear discontinuidades en el valor de ganancia ni en su derivada cuando la curva lo permita.
- En frases cercanas, evaluar la continuidad conjunta en vez de aplicar dos rampas independientes que se solapen de forma impredecible.
- Mantener los límites de edición visibles y manipulables.
- Si no hay espacio temporal para una transición segura, reducir la corrección o solicitar intervención manual.

## 6. Combinación de frases y secciones

Las propuestas por frase deben convertirse en una curva global coherente. La combinación considera:
- Frases contiguas.
- Pausas y respiraciones entre frases.
- Cambios de sección.
- Diferencias expresivas intencionales.
- Curvas manuales preexistentes.
- Zonas protegidas.

No sumar correcciones de forma ciega cuando se solapan. Debe existir una política explícita de prioridad y composición para evitar que la curva final exceda lo recomendado por cada análisis.

Las ediciones manuales del usuario tienen prioridad sobre las propuestas automáticas. Si un nuevo análisis contradice una edición manual, se debe conservar la edición y mostrar el conflicto, no sobrescribirla.

## 7. Edición manual

El gráfico debe permitir:
- Seleccionar una frase o región.
- Mover la ganancia de una región con control preciso.
- Añadir, mover y eliminar puntos de control.
- Dibujar una curva manual.
- Deshacer y rehacer.
- Comparar curva propuesta y curva aplicada.
- Aceptar, rechazar o restablecer propuestas por fragmento.
- Bloquear una región para que futuros análisis no la modifiquen.

La edición manual debe ser determinista y predecible: el mismo gesto sobre el mismo estado produce el mismo resultado.

## 8. Seguridad y robustez

- Definir límites configurables para evitar ganancias extremas accidentales.
- Evitar NaN, infinitos, valores fuera de rango y discontinuidades.
- Suavizar cambios de curva sin alterar el tiempo del audio.
- Mantener la señal original recuperable.
- Procesar cambios de parámetros sin clics.
- No introducir latencia de análisis en el camino de audio en tiempo real.
- No usar lookahead dinámico ni respuesta automática a la amplitud instantánea: la curva es una automatización de ganancia previamente calculada o editada.

Los límites numéricos concretos quedan pendientes de pruebas y no se deben inventar antes de evaluar tomas reales.

## 9. Relación con Clip Peak

Clip Gain y Clip Peak son motores independientes. Clip Gain resuelve diferencias de nivel entre frases y regiones vocales; Clip Peak propone atenuaciones localizadas para anomalías de pico.

No deben aplicar correcciones duplicadas sobre la misma región sin coordinación. La arquitectura debe mantener curvas separadas y ofrecer una vista combinada que permita al usuario entender el efecto total.

La composición final de ambas curvas debe evitar doble atenuación excesiva y conservar la posibilidad de activar, desactivar, aceptar o rechazar cada motor por separado.

## 10. Validación

Crear pruebas de señal sintética y grabaciones vocales con:
- Frases separadas por pausas cortas y largas.
- Frases adyacentes sin silencio.
- Consonantes explosivas al principio y final de frase.
- Respiraciones y colas reverberantes.
- Crescendos y diminuendos intencionales.
- Frases con grandes diferencias de nivel.
- Cambios de sección con contraste deliberado.
- Edición manual que contradice una propuesta automática.

Medir:
- Ausencia de clics y discontinuidades.
- Continuidad de la envolvente.
- Exactitud temporal de los límites.
- Ausencia de solapamientos inesperados.
- Reversibilidad y estabilidad al deshacer/rehacer.
- Conservación de las ediciones bloqueadas.
- Naturalidad en escucha ciega.
- Concordancia entre la curva visible y la ganancia realmente aplicada.

## 11. Decisiones pendientes

- Tipo de interpolación y continuidad exigida.
- Duración adaptativa de rampas.
- Política matemática de composición de regiones solapadas.
- Límites máximos de ganancia y atenuación.
- Resolución temporal de puntos de control.
- Comportamiento de la curva ante cambios de sample rate y tempo.
- Formato de persistencia y compatibilidad futura de sesiones.

Estas decisiones se cerrarán con pruebas, no por preferencia estética aislada.
