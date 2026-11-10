# PG Clip Gain Pro — Integración VST3 de captura y ciclo de vida

**Estado:** especificación de integración previa a código. No afirma que la captura esté conectada ni validada.

## 1. Objetivo de la primera captura funcional

La primera versión debe analizar el audio que realmente atraviesa la instancia VST3. Como VST3 no ofrece acceso genérico al archivo o al clip completo del DAW, el flujo inicial será explícito: el usuario arma la captura, reproduce el tramo desde el inicio y desarma al terminar. La región capturada será exactamente el intervalo que pasó por el plugin mientras estuvo armado.

No se inferirá que se capturó el clip entero si el usuario comenzó tarde, hubo un salto de transporte, un loop, un bloque perdido o el host no proporcionó posición de muestra fiable.

## 2. Contrato de tiempo

- Usar `ProcessData::processContext` y `ProcessContext::projectTimeSamples` solo cuando el bit `kProjectTimeMusicValid`? No: la validez de posición de muestras debe verificarse con el flag de tiempo de proyecto definido por la versión exacta del SDK VST3 integrada. No asumir que un flag de tiempo musical valida muestras.
- Antes de implementar, inspeccionar el encabezado real del SDK fijado por CMake y confirmar el nombre exacto del flag de validez para `projectTimeSamples`.
- Si el host no proporciona posición absoluta válida, no inventar una. La instancia puede mantener un contador monotónico local desde el primer bloque capturado, pero no podrá detectar todos los seeks/loops externos. En ese modo la UI debe advertir la limitación y la captura solo será aceptable si la continuidad local está garantizada por el host y el usuario.
- Cada bloque encolado lleva posición inicial, número de frames, canales y formato original. Huecos, solapamientos y retrocesos invalidan la captura completa.

## 3. Máquina de estados y propiedad de datos

Estados conceptuales:
1. `Disarmed`: el callback solo procesa audio transparente.
2. `ArmRequested`: el worker prepara el acumulador y confirma que la cola está vacía; el callback todavía no encola.
3. `Armed`: el callback fija el primer sample position válido y comienza a encolar bloques.
4. `StopRequested`: el callback deja de encolar; el worker drena todos los bloques pendientes.
5. `Captured`: captura contigua finalizada; el worker puede analizar.
6. `Rejected`: overflow, discontinuidad, formato no soportado o cobertura inválida; no se publican curvas.

El callback es el único productor de la SPSC; el worker es el único consumidor. El worker es propietario del ensamblador, del acumulador y de los buffers de análisis. La UI/control comunica órdenes mediante estado atómico o una cola de comandos acotada; nunca llama directamente a métodos del acumulador mientras el worker los utiliza.

El armado requiere un handshake: el worker prepara el ensamblador mientras la producción está detenida; solo después publica `Armed`. El callback no debe encolar antes de esa confirmación. Al detener, el callback deja de producir antes de que el worker finalice el ensamblador.

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

1. Confirmar en el SDK fijado los flags y campos exactos de `ProcessContext`, y los puntos seguros de inicialización/finalización del procesador.
2. Implementar y probar la máquina de estados independiente del SDK.
3. Probar handshake de armado/detención, overflow, huecos, solapamientos, seek y cancelación con productor/consumidor concurrentes.
4. Integrar el callback como productor, sin worker todavía, y verificar passthrough bit-exacto en float32/float64.
5. Integrar el worker con cierre y cancelación deterministas.
6. Ejecutar CI en Windows/Linux y Validator solo después de revisión estática completa.
7. Instalar en FL Studio y probar reproducción, stop, seek, loop, mono/estéreo, varias instancias y cierre del proyecto.
8. Solo entonces habilitar análisis y curvas; hasta ese momento el plugin debe seguir siendo transparente.

## 9. Riesgos que bloquean la integración

- Falta confirmar el flag VST3 exacto que valida `projectTimeSamples`.
- No existe aún interfaz propia para armar/desarmar; el controlador actual solo serializa estado.
- No existe worker real ni handshake entre control, callback y consumidor.
- No hay límite de memoria de captura elegido y probado con duraciones reales.
- No hay política implementada para captura cancelada, stop del host y restauración de sesión.

**Criterio de salida:** ninguna captura se presenta como completa salvo que el productor haya terminado, la cola esté drenada, no haya bloques descartados, la secuencia de posiciones sea contigua y el acumulador confirme cobertura válida.
