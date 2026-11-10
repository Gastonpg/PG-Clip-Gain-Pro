# PG Clip Gain Pro — Flujo compartido de análisis en dos pasadas

**Estado:** Diseño de flujo v0.1 — parcialmente prototipado. El código actual implementa descriptores numéricos iniciales por ventana, snapshot compartido validado y estadísticas contextuales descriptivas; no implementa todavía actividad vocal, límites de frase, protección fonética ni la segunda pasada completa.
**Objetivo:** definir qué hace cada pasada del análisis común y qué datos entrega a Clip Gain y Clip Peak, sin mezclar sus decisiones.

## 1. Por qué dos pasadas

Una sola evaluación local no dispone de todo el contexto de la toma. Para proteger límites fonéticos, reconocer continuidad, construir perfiles de frase y elegir una referencia robusta, el sistema necesita primero describir la señal y luego interpretar esa descripción con contexto temporal.

Las dos pasadas son offline respecto del procesamiento crítico del DAW. No significan dos procesos de audio en tiempo real ni dos agentes de IA.

## 2. Entrada y precondiciones

La entrada es una captura de región con:
- frecuencia de muestreo y canales conocidos;
- índices temporales coherentes;
- cobertura completa o estado parcial explícito;
- discontinuidades registradas;
- configuración de análisis versionada.

Si la captura es parcial o tiene huecos, el flujo puede generar diagnósticos locales, pero debe limitar las conclusiones globales y advertirlo. Nunca rellena huecos con silencio ficticio para fingir continuidad.

## 3. Pasada 1 — Descripción de señal y candidatos

**Objetivo:** describir la señal de forma neutral, sin decidir aún las correcciones de los motores.

### 3.1 Procesamiento

1. Validar metadatos, rango numérico, canales y cobertura.
2. Recorrer la captura en bloques con continuidad de estado entre bloques.
3. Calcular descriptores por ventanas y por canal.
4. Estimar envolvente y actividad candidata mediante evidencia complementaria.
5. Detectar cambios, ataques y eventos transitorios candidatos.
6. Estimar el fondo con cautela y marcar ventanas contaminadas o ambiguas.
7. Registrar regiones candidatas y protección preliminar.
8. Acumular estadísticas compactas y referencias a los datos temporales necesarios para la pasada 2.

### 3.2 Salida

Un **Coarse Analysis Map** versionado que contiene:
- descriptores por ventana;
- estados candidatos de actividad;
- ataques/transitorios candidatos;
- estimaciones de fondo y validez;
- indicadores de continuidad;
- advertencias y cobertura;
- configuración exacta de cálculo.

No contiene todavía curvas finales ni referencia adaptativa definitiva.

## 4. Pasada 2 — Refinamiento contextual y perfiles

**Objetivo:** convertir candidatos en una representación temporal coherente, con incertidumbre explícita.

### 4.1 Procesamiento

1. Revisar actividad candidata en contexto anterior y posterior.
2. Aplicar histéresis, continuidad y reglas de duración validadas.
3. Agrupar regiones en frases candidatas sin asumir detección fonética perfecta.
4. Ajustar límites para proteger ataques, consonantes, respiraciones y colas.
5. Clasificar pausas internas, respiraciones y finales como hipótesis con confianza.
6. Calcular perfiles robustos de frase a partir de ventanas válidas.
7. Determinar elegibilidad para referencia adaptativa y agrupación contextual.
8. Generar el **Shared Analysis Snapshot** completo, con estados, máscaras, métricas y procedencia.

### 4.2 Salida

El snapshot final contiene:
- regiones y frases refinadas;
- límites temporales exactos;
- perfiles robustos y número de ventanas válidas;
- transitorios y contexto local;
- zonas protegidas y razones;
- confianza, incertidumbre y abstenciones;
- versión de esquema, algoritmo y parámetros.

## 5. Consumo por los motores

Solo después de publicar el snapshot completo:
- Clip Gain calcula su referencia y su curva relativa.
- Clip Peak evalúa anomalías locales y sus curvas.
- Safety / Coordination comprueba la interacción de las propuestas.

Los motores no reescriben el Shared Analysis Snapshot. Si un motor detecta que necesita evidencia que no existe, solicita una nueva configuración/análisis o se abstiene; no inventa datos.

## 6. Memoria y ejecución en hardware modesto

La pasada 1 debe producir datos compactos. La pasada 2 opera sobre esos descriptores y, cuando sea necesario, sobre una caché temporal de audio con límites definidos.

Política inicial:
- una tarea de análisis principal por instancia;
- bloques acotados y cancelables;
- descriptores reutilizados por ambos motores;
- sin duplicar capturas por motor;
- evitar una estructura por muestra cuando bastan ventanas y eventos;
- permitir que el análisis tarde más antes que competir con la reproducción;
- limpiar caché temporal al completar, cancelar o cerrar según estado seguro.

La elección entre mantener la captura en memoria o usar almacenamiento temporal por bloques se resolverá con mediciones de duración, frecuencia, canales y memoria disponible. No se fija una duración máxima arbitraria sin pruebas.

## 7. Determinismo y cancelación

La misma captura, configuración y versión debe producir el mismo snapshot. El orden de llegada de bloques no debe alterar el resultado una vez reconstruida la secuencia temporal válida.

Si cambia la captura o la configuración:
- cancelar el trabajo obsoleto;
- no publicar resultados parciales como definitivos;
- identificar cada trabajo con un ID;
- descartar resultados tardíos de trabajos anteriores;
- conservar el último snapshot aprobado hasta que exista uno nuevo válido.

La cancelación no debe dejar recursos temporales abiertos ni publicar curvas incompletas.

## 8. Estados de progreso

Estados sugeridos:
1. esperando captura;
2. capturando;
3. validando cobertura;
4. pasada 1 — descripción;
5. pasada 2 — refinamiento;
6. motores independientes;
7. coordinación y validación;
8. listo para revisión;
9. parcial / requiere repetir captura;
10. cancelado;
11. error.

El progreso debe reflejar trabajo real. No mostrar 100 % mientras falte la coordinación o validación.

## 9. Pruebas específicas

- La pasada 1 no genera correcciones.
- La pasada 2 no altera los descriptores base sin dejar procedencia.
- La salida es idéntica con distintos tamaños de bloque.
- La pérdida de bloques cambia el estado de cobertura y restringe referencias globales.
- La misma señal/configuración produce el mismo snapshot.
- La cancelación impide publicar resultados obsoletos.
- Clip Gain y Clip Peak reciben exactamente el mismo snapshot.
- Ejecutar o desactivar un motor no cambia el snapshot ni la salida del otro.
- El consumo de memoria permanece acotado al aumentar duración.
- La caché temporal se limpia correctamente en éxito, cancelación y error.

## 10. Puerta de aceptación

El flujo se considera implementable cuando:
- las entradas y salidas de ambas pasadas están tipadas y versionadas;
- las responsabilidades no se duplican;
- los estados parciales y errores son explícitos;
- existen fixtures deterministas para captura, continuidad y cancelación;
- la política de memoria se valida con perfiles reales;
- los dos motores consumen un snapshot común e inmutable.

La especificación no demuestra todavía precisión de detección ni naturalidad musical. Esas propiedades requieren prototipo, corpus vocal y escucha.
