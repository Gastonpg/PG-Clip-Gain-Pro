# PG Clip Gain Pro — Base de implementación v0.3

**Estado:** núcleo DSP C++17 y primer módulo VST3 real integrados en el proyecto. Las seis suites del núcleo ya pasaron localmente en Release y Debug con AddressSanitizer/UndefinedBehaviorSanitizer. La compilación y validación del módulo VST3 están configuradas en CI para Windows y Linux, pero aún no hay resultado remoto confirmado.
**Importante:** el módulo VST3 actual es un shell de compatibilidad transparente (passthrough), no el producto DSP terminado. No se declara todavía compatible con FL Studio hasta ejecutar la validación del SDK y probar el binario en el DAW.

## 1. Núcleo DSP host-independiente

- C++17 y CMake.
- Biblioteca estática PIC, apta para enlazarse dentro del módulo VST3.
- Tipos temporales, validadores, curvas estáticas y conversión dB/amplitud.
- Fixtures sintéticos deterministas.
- Captura acotada y preasignada.
- Descriptores iniciales por ventana y canal: RMS, pico, media absoluta, offset DC, tasa de cruces por cero y factor de cresta; los canales permanecen separados.
- Snapshot compartido inmutable con comprobación de formato, configuración y cobertura.
- Estadísticas contextuales descriptivas con mediana, MAD y rangos.
- Cola SPSC acotada y preasignada para transferir bloques de audio del callback al worker: índices atómicos de 64 bits, sin locks ni asignaciones en push/pop, soporte de entrada float32/float64 (copia de análisis float32), flags de silencio, metadatos temporales y contador de bloques descartados por saturación.
- Las seis suites originales fueron aprobadas localmente en Release y Debug con AddressSanitizer y UndefinedBehaviorSanitizer. Se agregaron dos suites: estrés SPSC con 20.000 bloques y ensamblado de captura desde la cola; además se ampliaron las pruebas existentes de descriptores y snapshots. Los cambios nuevos quedan pendientes de ejecución en CI.

## 2. Primera integración VST3

- SDK oficial Steinberg VST3 3.8.1 fijado al commit `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`, con submódulos recursivos.
- SDK integrado mediante CMake FetchContent; VSTGUI y plugins de ejemplo desactivados para mantener el build reducido. Se conserva el Validator oficial y la generación de moduleinfo.
- Procesador y controlador son componentes separados, enlazados por IDs de clase propios.
- Bus principal mono/estéreo; el procesador rechaza configuraciones no soportadas.
- Soporte de bloques de audio de 32 y 64 bits.
- Procesamiento actual transparente: copia entrada a salida, respeta canales silenciosos y no altera el audio.
- Estado del componente con cabecera mágica y versión explícita.
- Sin asignaciones, análisis, bloqueos ni operaciones de disco en el callback de procesamiento.
- El SDK Validator y la generación de `moduleinfo.json` quedan activados en la configuración de CMake.

Se incluyen los avisos de distribución utilizados por la integración: licencia MIT oficial del SDK en `third_party/licenses/STEINBERG_VST3_SDK_LICENSE.txt` y aviso de la plantilla CMake de versión en `third_party/licenses/STEINBERG_CMAKE_PROJECTVERSION_TEMPLATE_NOTICE.txt`.

Este shell demuestra la integración de API, factory, busses, tamaños de muestra, estado y ruta de procesamiento. No implementa todavía editor gráfico, parámetros de usuario, captura de análisis, curva de ganancia ni motores Clip Gain/Clip Peak.

## 3. Compatibilidad objetivo

La primera entrega objetivo es Windows x64 y VST3 para FL Studio de 64 bits. El SDK oficial declara soporte para Windows 10 x64 con MSVC 2019/2022. El equipo del usuario dispone de CPU x64; el build debe mantenerse eficiente y no depender de GPU.

El paquete de Windows se genera como `PGClipGainPro.vst3`, con la estructura de bundle VST3 de Steinberg. El nombre visible dentro del DAW sigue siendo `PG Clip Gain Pro`. La CI compila además en Linux como plataforma secundaria. La salida Linux no sustituye al binario Windows que necesita FL Studio en el equipo del usuario.

## 4. Validación pendiente

1. Confirmar que CMake obtiene el SDK y sus submódulos fijados.
2. Compilar el módulo en Windows y Linux.
3. Ejecutar el Validator oficial de Steinberg sobre el bundle generado.
4. Revisar que el paquete contenga la arquitectura correcta y `moduleinfo.json`.
5. Probar instalación, escaneo, carga, procesamiento transparente y guardado/restauración de proyecto en FL Studio.
6. Mantener pruebas de regresión para mono/estéreo, float32/float64, buffers variables, silencio y estado.

Un resultado verde de CI no reemplaza la prueba manual dentro de FL Studio.

## 5. Límites actuales

- El VST3 aún no tiene interfaz gráfica.
- La cola SPSC ya está implementada y probada por diseño unitario, pero todavía no está conectada al callback real del VST3 ni a un worker de análisis.
- No captura una región completa del DAW.
- No integra todavía una cola SPSC ni coordinación callback/worker.
- No implementa la segunda pasada contextual ni segmentación vocal.
- No implementa referencia adaptativa.
- No implementa curvas Clip Gain ni Clip Peak.
- No tiene todavía instalador ni paquete final de distribución.

## 6. Próximas etapas

1. Conseguir build y Validator verde en Windows/Linux.
2. Revisar cualquier fallo del Validator y corregirlo antes de añadir DSP al plugin.
3. Diseñar la adquisición VST3 con callback acotado y worker seguro.
4. Completar descriptores y segunda pasada contextual.
5. Validar segmentación con grabaciones vocales y anotación humana.
6. Construir referencia adaptativa y motor Clip Gain.
7. Construir Clip Peak como motor separado.
8. Añadir persistencia real de curvas y edición manual.
9. Crear GUI oscura con waveform real después de validar el DSP.
10. Probar en FL Studio de 64 bits y preparar paquete instalable.

Los cinco jueces de PG Audio Intelligence permanecen fuera de este alcance y no se implementarán sin autorización explícita.
