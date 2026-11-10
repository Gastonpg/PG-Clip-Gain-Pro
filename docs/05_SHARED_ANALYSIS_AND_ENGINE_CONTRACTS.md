# PG Clip Gain Pro — Contratos de análisis compartido y motores

**Estado:** Especificación de arquitectura v0.1 — conceptual, pendiente de implementación y validación.

## 1. Separación de responsabilidades

El análisis compartido describe lo que hay en la grabación. No decide cuánto corregir. Los motores consumen esa descripción y producen propuestas independientes:

- **Shared Analysis:** observa y etiqueta evidencia temporal.
- **Clip Gain Engine:** estima diferencias de sonoridad entre frases y propone una curva relativa.
- **Clip Peak Engine:** busca anomalías de pico locales y propone atenuaciones puntuales.
- **Safety / Coordination:** comprueba interacciones, límites y zonas protegidas.
- **Editor:** presenta resultados y permite modificarlos.

Ningún motor escribe directamente sobre el archivo fuente. Las salidas son datos editables y reversibles.

## 2. Contrato de Shared Analysis

Cada sesión debe identificar versión del esquema, frecuencia de muestreo, canales, cantidad de muestras, duración, configuración de análisis, validez y advertencias. Debe entregar regiones candidatas, frases, pausas, eventos transitorios, descriptores, estadísticas robustas, máscaras de protección y confianza.

Los tiempos deben tener una unidad inequívoca. La posición en muestras será la referencia exacta; los segundos serán una representación derivada. Nunca se mezclan índices de muestras de distintas frecuencias sin conversión explícita.

### 2.1 Regiones

Cada intervalo contiene:
- startSample inclusivo y endSample exclusivo;
- clase semántica;
- confianza normalizada;
- procedencia de detección;
- indicadores de protección;
- motivo de revisión cuando corresponda.

Clases iniciales: voz probable, consonante/ataque, respiración probable, pausa, cola/reverb probable, transitorio, ruido/ambiente y desconocido. Son hipótesis, no verdades absolutas.

### 2.2 Descriptores

- RMS y energía por ventana;
- pico de muestra y estimación entre muestras cuando sea pertinente;
- envolvente suavizada;
- descriptor perceptual de sonoridad vocal;
- densidad o actividad espectral;
- pendiente y prominencia del ataque;
- continuidad temporal;
- relación local con el fondo;
- estadísticas robustas por región y frase.

Cada descriptor incluye unidad, escala, ventana y validez. No se comparan valores calculados con configuraciones incompatibles.

## 3. Contrato de Clip Gain

**Entrada:** regiones y frases compartidas, descriptores válidos, agrupación contextual, intensidad del usuario, límites de seguridad, regiones protegidas y correcciones manuales existentes.

**Salida:** curva estática de ganancia temporal, referencia interna seleccionada y explicación, ganancia por frase, transiciones, confianza, estado de revisión, abstenciones y procedencia automática/manual.

Debe conservar la dinámica dentro de cada frase salvo edición manual explícita. Sutil / Equilibrada / Pronunciada escala la fuerza de corrección relativa; no representa un objetivo LUFS o dBFS.

## 4. Contrato de Clip Peak

**Entrada:** transitorios y descriptores compartidos, contexto local anterior y posterior, regiones protegidas, curva Clip Gain propuesta y sensibilidad del usuario.

**Salida:** eventos candidatos con evidencia, clasificación tentativa, confianza, profundidad y anchura sugeridas, curva localizada, abstenciones y procedencia.

Un pico alto aislado no es automáticamente un problema. Se evalúan contexto, forma, duración y posible función fonética o expresiva.

## 5. Coordinación y prevención de doble corrección

Los motores calculan propuestas por separado. El coordinador:
1. conserva ambas curvas originales y su procedencia;
2. evalúa la ganancia combinada prevista;
3. detecta solapamientos entre corrección de frase y atenuación de pico;
4. comprueba límites configurables de seguridad;
5. reduce confianza y solicita revisión ante una combinación excesiva o incierta;
6. permite desactivar un motor sin alterar el otro;
7. evita modificar destructivamente la fuente por defecto.

La suma en dB de curvas estáticas es matemáticamente válida, pero su validez musical depende del contexto. Dos propuestas válidas por separado no son necesariamente válidas al combinarse.

## 6. Estados de recomendación

- Propuesta: calculada, aún no revisada.
- Lista para revisión: evidencia suficiente, requiere decisión humana.
- Aceptada: aprobada por el usuario.
- Rechazada: descartada.
- Modificada: ajustada manualmente.
- Abstención: no recomienda actuar.
- Bloqueada: conflicto con protección o límite.

Los cambios manuales tienen prioridad y quedan identificados como tales.

## 7. Versionado y reproducibilidad

Registrar versión del algoritmo y esquema, parámetros, frecuencia de muestreo y configuración de canales. Un cambio de algoritmo debe distinguir el análisis anterior del nuevo; nunca reinterpretar silenciosamente curvas antiguas con reglas nuevas.

## 8. Criterios de aceptación

- Los motores se ejecutan y desactivan independientemente.
- Ambos consumen el mismo análisis compartido.
- Ninguna recomendación carece de evidencia, confianza o motivo de abstención.
- El usuario puede aceptar, rechazar, editar y deshacer cada curva.
- La combinación respeta límites de seguridad y advierte conflictos.
- Las correcciones manuales sobreviven a revisiones automáticas según reglas explícitas.
- Un proyecto puede reconstruir las curvas con la misma versión y parámetros.

Los nombres definitivos de clases, serialización y límites numéricos quedan para el prototipo y las mediciones.
