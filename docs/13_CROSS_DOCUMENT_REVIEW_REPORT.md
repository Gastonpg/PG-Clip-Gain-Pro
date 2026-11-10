# PG Clip Gain Pro — Informe de revisión cruzada de la base

**Estado:** Revisión documental v0.2 — revisión estática completada; sin validación DSP ejecutada.
**Alcance:** coherencia entre especificación maestra, diseño DSP, segmentación, medición, curvas, contratos, validación, runtime, modelo numérico, fixtures y flujo de dos pasadas.

## 1. Resultado ejecutivo

La revisión estática detectó y corrigió inconsistencias documentales. La arquitectura queda alineada en estos puntos:
- captura de audio entregado por el host VST3, sin asumir acceso directo al archivo;
- análisis complejo fuera del hilo de audio;
- flujo compartido en dos pasadas;
- snapshot común, versionado e inmutable;
- motores Clip Gain y Clip Peak independientes;
- coordinación posterior que verifica el resultado combinado;
- curvas estáticas, reversibles y sujetas a aprobación del usuario;
- jueces como fase posterior separada, bloqueada hasta autorización explícita.

**Resultado:** no quedan contradicciones documentales críticas conocidas dentro del alcance revisado. Esto no significa que el algoritmo esté implementado, que las decisiones DSP estén calibradas ni que el plugin haya sido probado en un DAW.

## 2. Hallazgos y correcciones aplicadas

### REV-001 — Una pasada frente a dos pasadas
**Hallazgo:** la especificación maestra describía una pasada común mientras el documento específico definía dos.
**Corrección:** la maestra y el diseño DSP ahora describen pasada 1 (descripción neutral y candidatos) y pasada 2 (refinamiento contextual y perfiles). Ninguna pasada genera curvas.

### REV-002 — Captura VST3 frente a análisis offline
**Hallazgo:** podía interpretarse que el análisis offline accedía a la región completa sin explicar cómo llegan las muestras.
**Corrección:** el flujo define captura durante reproducción cuando el host no ofrece una vía documentada alternativa. El recorrido de audio solo copia bloques a un búfer/cola preasignado y acotado; el worker ejecuta el análisis después de completar la cobertura.

### REV-003 — Matriz de requisitos mal formada
**Hallazgo:** cuatro requisitos se habían guardado como secuencias literales de barra invertida y n dentro de una sola fila.
**Corrección:** PG-014 a PG-017 ahora son filas independientes de la tabla.

### REV-004 — Dependencia prematura de los jueces
**Hallazgo:** el diseño DSP mencionaba decisiones que parecían depender de jueces aún no autorizados.
**Corrección:** el primer prototipo debe funcionar con DSP y reglas deterministas de seguridad. Los jueces son una fase posterior y no se inician sin permiso explícito del usuario.

## 3. Contratos que ahora deben permanecer estables

1. Índices temporales en frames enteros; intervalos con inicio inclusivo y final exclusivo.
2. Frecuencia de muestreo y disposición de canales asociadas a cada captura.
3. Cobertura y discontinuidades explícitas; una captura parcial no habilita referencia global de toda la toma.
4. Descriptores con unidad, ventana, salto, alineación, canal, versión y validez.
5. Confianza como puntuación de evidencia no calibrada, no como probabilidad.
6. Clip Gain y Clip Peak producen curvas separadas.
7. Las curvas se publican como snapshots completos; nunca se mutan mientras el hilo de audio las consume.
8. La curva combinada se evalúa antes de aprobarla.
9. Los cambios manuales tienen procedencia y prioridad definidas.
10. La GUI final espera a que el DSP esté probado.

## 4. Decisiones abiertas correctamente pendientes de medición

No son contradicciones; son parámetros que no deben fijarse por intuición:
- longitud y salto de ventana;
- filtros y ponderaciones perceptuales;
- política multicanal definitiva;
- umbrales e histéresis de actividad;
- duración de pausas y agrupación de frases;
- estadísticas robustas y método de referencia adaptativa;
- límites de ganancia y pendientes de curva;
- interpolación definitiva y rampas;
- detector contextual de anomalías Clip Peak;
- límites combinados de seguridad;
- capacidad y política de caché para clips largos;
- presupuesto de CPU/memoria y tamaños de bloque reales en hosts.

Cada decisión debe resolverse con fixtures, mediciones, escucha A/B y registro de parámetros/versiones.

## 5. Riesgos técnicos que requieren prototipo temprano

### RISK-001 — Captura completa de región
El plugin no puede garantizar que el host entregue la región entera en una sola reproducción. Debe detectar inicio, fin, seek, loop, discontinuidad y detención. La UI debe explicar cuándo falta cobertura.

### RISK-002 — Seguridad de la captura en callback
La copia de bloques debe usar memoria preasignada y una política de overflow explícita. Si se llena el búfer, el análisis se marca incompleto; nunca se bloquea el hilo de audio para salvar la captura.

### RISK-003 — Audio fuente para la segunda pasada
Debe decidirse mediante perfilado si se conserva audio en memoria, caché temporal o una estrategia incremental. No guardar el audio original en el estado persistente del plugin.

### RISK-004 — Tiempo de la curva respecto al host
El prototipo debe verificar mapeo de posición de proyecto, seeks, loops, transporte, cambios de sample rate y render offline en FL Studio y al menos otro host compatible cuando sea posible.

### RISK-005 — Evidencia musical
Las señales sintéticas verifican propiedades matemáticas y de captura, no intención ni naturalidad. Hace falta corpus vocal autorizado y anotación humana antes de declarar precisión musical.

## 6. Orden de implementación recomendado

1. Proyecto VST3 mínimo y harness de pruebas independiente del host.
2. Tipos y validadores del dominio temporal/numerérico.
3. Generadores de fixtures y pruebas unitarias.
4. Captura segura de bloques y detección de cobertura/discontinuidad.
5. Descriptores de pasada 1.
6. Refinamiento de pasada 2.
7. Exportación de mapas de diagnóstico para inspección.
8. Segmentación vocal calibrada con corpus.
9. Perfil de sonoridad y referencia adaptativa.
10. Curva Clip Gain y pruebas de escucha.
11. Detector y curva Clip Peak.
12. Coordinación, snapshots, persistencia y reversión.
13. Pruebas VST3 en hosts reales.
14. GUI después de validar el comportamiento sonoro.

Los jueces no están incluidos en este orden y requieren autorización separada.

## 7. Criterios para declarar lista la base documental

- [x] Filosofía y límites del producto definidos.
- [x] Contratos independientes de ambos motores.
- [x] Modelo temporal y numérico común.
- [x] Flujo compartido de dos pasadas.
- [x] Captura VST3 y separación de hilos documentadas.
- [x] Protocolo de validación y fixtures deterministas especificados.
- [x] Requisitos trazables a pruebas.
- [x] Jueces separados de la fase DSP y sujetos a autorización.
- [ ] Prototipo y pruebas ejecutadas: todavía pendientes.
- [ ] Corpus vocal y escucha A/B: todavía pendientes.
- [ ] Perfilado real en DAW: todavía pendiente.

## 8. Conclusión

La base documental queda suficientemente coherente para iniciar una implementación incremental del núcleo DSP. El primer objetivo no es nivelar voces todavía: es demostrar que captura, unidades, límites, estados y fixtures funcionan correctamente y de forma reproducible.

No se debe declarar el algoritmo profesionalmente validado hasta superar las pruebas técnicas, las pruebas de escucha y la evaluación en hosts reales.
