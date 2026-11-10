# PG Clip Gain Pro — Arquitectura de ejecución y presupuesto de recursos

**Estado:** Diseño técnico v0.1 — propuesta para prototipo.

## 1. Separación de contextos

### Hilo de audio del DAW

Aplica curvas estáticas ya calculadas, con interpolación segura y costo acotado.

No debe analizar frases ni ejecutar modelos; asignar o liberar memoria; hacer disco, red o logging bloqueante; esperar mutex o tareas de UI; crear objetos gráficos o cadenas; ni ejecutar búsquedas de costo ilimitado.

El procesamiento se limita a leer parámetros/curvas preparadas y aplicar la ganancia correspondiente.

### Hilo de análisis offline

Lee y analiza bloques, calcula descriptores, segmenta y genera propuestas. Debe poder cancelarse y ceder recursos. No debe competir innecesariamente con reproducción.

### Hilo de interfaz

Dibuja waveform, regiones y curvas. Recibe snapshots coherentes. No realiza DSP pesado ni bloquea el hilo de audio.

### Coordinación

Usar snapshots inmutables, colas acotadas o mecanismos lock-free apropiados. Evitar contenedores mutables compartidos entre hilos. La solución concreta se medirá en pruebas de estrés.

## 2. Flujo propuesto

1. El host entrega audio según VST3.
2. Una acción explícita inicia la captura de análisis; el usuario reproduce la región desde el inicio cuando el host no ofrece otra vía documentada.
3. El recorrido de audio copia bloques a una cola o búfer preasignado y acotado, sin análisis pesado, disco ni bloqueos.
4. Al completar la cobertura, un worker fuera del hilo de audio ejecuta las dos pasadas compartidas y publica progreso.
5. Clip Gain y Clip Peak generan curvas separadas desde el mismo snapshot.
6. Safety / Coordination evalúa la interacción.
7. La UI recibe un snapshot coherente y representa waveform, regiones y curvas.
8. El usuario revisa, edita, acepta o rechaza.
9. El hilo de audio consume una versión aprobada mediante intercambio seguro y acotado.

Definir durante integración el comportamiento frente a cambios de buffer, transporte, render offline, suspensión y restauración VST3.

## 2.1 Adquisición de audio: límite importante de VST3

El diseño no puede asumir que un plugin insertado recibe una ruta de archivo ni acceso aleatorio a todas las muestras del clip. En el flujo VST3 habitual, el host entrega bloques al procesador. Por tanto, la adquisición de señal debe diseñarse explícitamente:

- **Captura durante reproducción:** el usuario inicia el análisis y reproduce la región. El plugin copia o resume los bloques recibidos en un búfer temporal de análisis, conservando continuidad y posición temporal.
- **Procesamiento offline del host:** si el DAW ejecuta el plugin en un render offline, esos bloques pueden alimentar el mismo analizador, siempre que el host realmente invoque ese recorrido.
- **Acceso extendido del host:** solo puede utilizarse si un host concreto ofrece una API documentada y compatible; nunca será requisito para el funcionamiento VST3 general.

La interfaz debe explicar cuándo necesita reproducir la región para completar el análisis y mostrar progreso, cancelación y cobertura obtenida. Si el host no entregó toda la región, el análisis se marca parcial y no debe fingir que conoce el clip completo.

### Política de almacenamiento

La captura debe tener presupuesto de memoria. Para clips largos, usar almacenamiento temporal por bloques o análisis incremental con descriptores compactos. Si se necesita conservar audio para la segunda pasada, debe existir una política explícita de caché temporal, límite de tamaño, limpieza y recuperación ante cancelación. No escribir archivos temporales sin control ni guardar audio fuente dentro del estado persistente del plugin.

La primera versión debe priorizar un flujo fiable y claro: iniciar captura, reproducir desde el inicio de la región, completar la cobertura, ejecutar las dos pasadas fuera del hilo de audio y luego generar las curvas. La posibilidad de analizar sin reproducir dependerá de capacidades reales del host y no se prometerá de forma universal.

## 3. Presupuesto para hardware modesto

Diseñar para CPU de pocos núcleos y memoria limitada:
- análisis incremental por bloques;
- una sola lectura cuando sea viable y descriptores compactos;
- evitar copias redundantes del audio;
- no recalcular descriptores por motor;
- cancelar trabajos obsoletos al cambiar de clip o parámetros;
- limitar concurrencia a una tarea principal por instancia salvo evidencia favorable;
- evitar modelos grandes, GPU obligatoria o servicios externos;
- priorizar algoritmos deterministas y SIMD opcional;
- permitir análisis más lento antes que afectar reproducción.

No prometer cifras de CPU o memoria hasta medir clips largos y múltiples instancias.

## 4. Memoria y duración

El tamaño depende de duración, frecuencia, canales, paso de análisis y densidad de puntos. Guardar índices y descriptores compactos, no objetos por muestra.

La forma de onda visual puede usar pirámide min/max o representación multirresolución. Debe conservar picos y transitorios al alejar zoom sin almacenar un punto gráfico por muestra.

## 5. Aplicación de curvas

Las curvas son ganancia estática temporal. Por bloque:
- localizar el segmento de curva correspondiente;
- interpolar con regla continua;
- mantener continuidad entre bloques;
- manejar límites y posición coherente con el host;
- definir comportamiento para seeks, loops, transporte detenido y render offline.

La curva aprobada llega al hilo de audio como snapshot completo. Nunca actualizar una curva parcialmente mientras el hilo la recorre.

## 6. Persistencia

Guardar curvas automáticas/manuales, estado de aceptación/rechazo, parámetros, versión del algoritmo y esquema, y metadatos necesarios para restaurar edición.

No duplicar audio fuente dentro del estado del plugin. Si el DAW pierde o cambia la fuente, mostrar estado claro y no aplicar una curva desfasada.

## 7. Robustez

Cubrir cambios de frecuencia y tamaño de bloque; mono, estéreo y multicanal compatible; clips vacíos, cortos y largos; cambios de estado durante reproducción; cancelación/reinicio; múltiples instancias; automatización; suspensión, cierre y restauración.

## 8. Puerta de rendimiento

Antes de declarar listo el motor:
- perfilar CPU en DAW real;
- probar reproducción continua;
- probar análisis mientras reproduce;
- comprobar asignaciones y bloqueos en hilo de audio;
- medir memoria en clips largos;
- probar varias instancias;
- verificar que la UI no interrumpe audio.

La prioridad es estabilidad. Si el análisis consume demasiado, se ralentiza o pausa voluntariamente; nunca se degrada reproducción para terminar antes.


## 9. Estado del prototipo de captura

La implementación inicial contiene un acumulador C++ preasignado y acotado, independiente de VST3. Acepta bloques contiguos con posición explícita, detecta huecos y solapamientos, y solo declara completa una región cuando recibe exactamente la longitud esperada.

Límites actuales:
- todavía no está conectado al procesador VST3 ni a FL Studio;
- la cola SPSC acotada y el ensamblador worker-side que reconstruye capturas completas/parciales ya existen como módulos host-independientes. La finalización exige productor detenido y cola vacía; el ensamblador compara el contador de bloques descartados. Aún no están conectados al callback VST3 ni a un hilo worker real;
- no resuelve por sí mismo seek, loop, stop, cambios de región ni render offline;
- el acumulador no es thread-safe: el llamador debe serializar las operaciones y asegurar que el callback esté quiescente durante begin/finish/cancel;
- el analizador de ventanas y las estadísticas contextuales funcionan offline sobre vistas PCM y snapshots ya construidos.

La integración real deberá incorporar un mecanismo de transferencia acotado y probado para el callback, identificar discontinuidades del transporte y asegurar que el análisis pesado ocurra fuera del hilo de audio. La implementación actual no debe presentarse como captura VST3 lista para producción.
