# PG Clip Gain Pro — Integración VST3 de captura y ciclo de vida

**Estado:** especificación de integración previa a código. No afirma que la captura esté conectada ni validada.

## 1. Objetivo de la primera captura funcional

La primera versión debe analizar el audio que realmente atraviesa la instancia VST3. Como VST3 no ofrece acceso genérico al archivo o al clip completo del DAW, el flujo inicial será explícito: el usuario arma la captura, reproduce el tramo desde el inicio y desarma al terminar. La región capturada será exactamente el intervalo que pasó por el plugin mientras estuvo armado.

No se inferirá que se capturó el clip entero si el usuario comenzó tarde, hubo un salto de transporte, un loop, un bloque perdido o el host no proporcionó posición de muestra fiable.

## 2. Contrato de tiempo

- La versión exacta del SDK fijada en CMake fue inspeccionada: `ProcessContext::projectTimeSamples` está documentado como **siempre válido**, por lo que no necesita un flag opcional de validez.
- `ProcessContext::kPlaying` indica transporte en reproducción; `kCycleActive` indica ciclo activo. `continousTimeSamples` es opcional y solo es válido cuando `kContTimeValid` está presente.
- Comparar `projectTimeSamples` de cada bloque con el final esperado del bloque anterior. Un salto, retroceso o solapamiento invalida la captura; si el host entrega subbloques que cruzan un ciclo, el salto se detectará en el siguiente bloque, sin asumir que el host dividió el bloque en el límite exacto.
- La inspección se realizó sobre el submódulo `vst3_pluginterfaces` fijado por el commit del SDK `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`, submódulo commit `4f547e8e102b47de4a8b8aaf343c73b700786372`, archivo `vst/ivstprocesscontext.h`. Referencia: https://github.com/steinbergmedia/vst3_pluginterfaces/blob/4f547e8e102b47de4a8b8aaf343c73b700786372/vst/ivstprocesscontext.h.
- Cada bloque encolado lleva posición inicial, número de frames, canales y formato original. Huecos, solapamientos y retrocesos invalidan la captura completa.

## 3. Máquina de estados y propiedad de datos

El módulo host-independiente `CaptureSessionControl` implementa el primer coordinador atómico de ciclo de vida. Sus estados son:

1. `Disarmed`: ruta transparente.
2. `ArmRequested`: el worker prepara cola y ensamblador.
3. `Armed`: solo ahora `shouldCapture()` permite encolar.
4. `StopRequested`: se bloquea el inicio de nuevos bloques.
5. `CancelRequested`: cancelación pendiente de quiescencia.
6. `Draining`: el productor está quiescente; el worker puede vaciar y validar.
7. `Captured`: el worker confirmó captura completa y válida.
8. `Rejected`: preparación fallida, overflow o discontinuidad.
9. `Cancelled`: cancelación confirmada tras detener al productor.

El callback será el único productor de la SPSC; el worker, el único consumidor. El coordinador solo intercambia estados atómicos: no posee audio, no llama al acumulador y no crea hilos. Los fallos y cancelaciones tampoco publican un estado terminal reutilizable hasta que el productor quedó quiescente y el worker terminó de limpiar la sesión. La suite independiente está registrada, pero todavía no fue ejecutada.

El armado requiere que el worker prepare el ensamblador antes de publicar `Armed`. Al detener o cancelar, el consumidor no debe finalizar ni liberar recursos hasta que el host garantice que el callback dejó de ejecutarse y no puede iniciar otra operación de producción. Ese punto de quiescencia es un contrato externo que la integración VST3 aún debe resolver; el coordinador no lo simula ni lo presume.

## 4. Reglas del callback de audio

- No asignar ni liberar memoria, crear hilos, usar mutex/condition variable, hacer I/O, logging, análisis, FFT, detección de voz o conversión costosa.
- Mantener el audio de salida transparente mientras la captura esté armada; la captura nunca debe alterar el audio en esta fase.
- Encolar mediante `tryPush` sobre buffers preasignados.
- Si el bloque del host supera el máximo del slot, dividirlo en subbloques usando offsets de canal sin asignaciones dinámicas.
- Convertir float64 a float32 solo para la copia de análisis ya que la cola actual almacena float32; el audio de salida permanece en su formato original. Registrar esta reducción de precisión como decisión de análisis, no como modificación del audio.
- Si la cola está llena o el bloque es inválido, marcar la sesión como rechazada. No intentar recuperar silenciosamente una captura incompleta.
- Copiar los flags de silencio y conservar la semántica de salida del procesador.

## 5. Worker y finalización

- El worker drena la cola fuera del hilo de audio y alimenta `QueuedCaptureAssembler`.
- La captura inicial no necesita conocer de antemano la longitud final: reservar un límite máximo explícito de frames y finalizar con `finishAtCapturedLength` después de detener el productor y vaciar la cola.
- La capacidad máxima debe derivarse de un presupuesto de memoria visible y validado. No reservar memoria ilimitada por duración arbitraria.
- Una sesión vacía, parcial por capacidad, con discontinuidades o con bloques descartados no puede producir snapshot completo.
- El worker ejecuta análisis solo después de validar el descriptor de captura y la continuidad.
- Cancelar una sesión no puede liberar buffers mientras el callback o el worker todavía los usan.

## 6. Transporte y casos límite

- **Play:** comenzar solo después de confirmar el estado `Armed`.
- **Stop solicitado por usuario:** detener la producción, drenar y finalizar al último frame aceptado.
- **Stop del host mientras sigue armado:** cerrar como captura explícitamente parcial o pedir al usuario que confirme el tramo; no asumir que el clip completo terminó.
- **Seek/loop/retroceso:** detectar cambio de posición no contiguo y rechazar la sesión; no concatenar regiones temporalmente distintas.
- **Cambio de sample rate o canales:** invalidar la sesión y requerir nueva captura.
- **Cambio de tamaño de bloque:** permitido dentro del máximo; subdividir bloques grandes.
- **Render offline:** no asumir que el host ejecutará el mismo ciclo de interacción. Tratarlo como modo separado y habilitarlo solo tras probarlo en hosts reales.
- **Suspensión, cierre y restauración:** parar productor, drenar o cancelar, detener worker y liberar recursos en orden definido.

## 7. Curvas y publicación

La primera integración debe seguir siendo passthrough hasta que exista una curva aprobada. Las curvas aprobadas se publican como snapshots inmutables con vida útil segura (por ejemplo, propiedad compartida fuera del callback y mecanismo de intercambio sin destrucción en el hilo de audio). No usar un `shared_ptr` cuya última liberación pueda ocurrir en el callback. El hilo de audio solo lee una estructura ya preparada y aplica interpolación acotada.

Clip Gain y Clip Peak conservan curvas separadas. No sumar ni fusionar automáticamente propuestas de ambos motores sin la etapa de seguridad/coordinación y una decisión explícita del usuario.

## 8. Secuencia de implementación y puertas

1. Confirmar los puntos seguros de inicialización/finalización del procesador (el contrato de `projectTimeSamples` ya fue inspeccionado).
2. Ejecutar la suite del coordinador atómico y completar pruebas de concurrencia y transiciones.
3. Resolver el punto de quiescencia real del callback en el ciclo de vida VST3.
4. Probar handshake, overflow, huecos, solapamientos, seek y cancelación con productor/consumidor concurrentes.
5. Integrar el callback como productor, sin worker todavía, y verificar passthrough bit-exacto en float32/float64.
6. Integrar el worker con cierre y cancelación deterministas.
7. Ejecutar CI en Windows/Linux y Validator solo después de revisión estática completa.
8. Instalar en FL Studio y probar reproducción, stop, seek, loop, mono/estéreo, varias instancias y cierre del proyecto.
9. Solo entonces habilitar análisis y curvas; hasta ese momento el plugin debe seguir siendo transparente.

## 9. Riesgos que bloquean la integración

- El contrato de `projectTimeSamples` ya fue confirmado en el SDK fijado.
- No existe aún interfaz propia para armar/desarmar; el controlador actual solo serializa estado.
- El coordinador atómico existe, pero no está conectado al callback ni a un worker real.
- El rechazo de una sesión no equivale a que el callback haya quedado quiescente: la integración debe esperar la garantía del host antes de drenar o liberar buffers. Los estados `RejectRequested` y `CancelDraining` impiden rearmar mientras esa limpieza siga pendiente; un fallo durante la preparación pasa también por `Draining`.
- No hay límite de memoria de captura elegido y probado con duraciones reales.
- No hay política implementada para captura cancelada, stop del host y restauración de sesión.

**Criterio de salida:** ninguna captura se presenta como completa salvo que el productor haya terminado, la cola esté drenada, no haya bloques descartados, la secuencia de posiciones sea contigua y el acumulador confirme cobertura válida.
