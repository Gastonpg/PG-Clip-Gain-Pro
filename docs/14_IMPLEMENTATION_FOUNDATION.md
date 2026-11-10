# PG Clip Gain Pro — Base de implementación v0.3

**Estado:** núcleo DSP C++17 y primer módulo VST3 real integrados en el proyecto. La versión anterior del núcleo pasó 6/6 suites localmente en Release y Debug con AddressSanitizer/UndefinedBehaviorSanitizer; las ampliaciones actuales elevan el banco a 8 suites y requieren revalidación. La compilación y validación VST3 están configuradas en CI para Windows x64 y Linux x64, pero aún no hay resultado remoto confirmado.
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
- Ensamblador del lado del worker que consume la cola, reconstruye audio intercalado, verifica continuidad temporal y distingue captura completa, parcial, cancelada o discontinua. Requiere que el llamador vacíe la cola antes de iniciar una sesión nueva. La finalización solo se acepta cuando el productor está detenido y la cola quedó vacía; también puede cerrar una región definida explícitamente por el usuario. Cualquier bloque descartado por saturación invalida la continuidad.
- La versión anterior de esta base pasó 6/6 suites localmente en Release y Debug con AddressSanitizer y UndefinedBehaviorSanitizer. Después se ampliaron los descriptores/snapshots y se agregaron dos suites: estrés SPSC con 20.000 bloques y ensamblado de captura desde la cola. Por esos cambios, las 8 suites del código actual deben reejecutarse; no se afirma que la versión actual ya haya pasado.

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

La primera entrega objetivo es Windows x64 y VST3 para FL Studio 2026 de 64 bits. El flujo de compilación se ejecuta en GitHub Actions con arquitectura x64 explícita, para que el usuario no dependa de instalar un toolchain completo en su PC. El SDK oficial declara soporte para Windows 10 x64 con MSVC 2019/2022. El build debe mantenerse eficiente y no depender de GPU.

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
- La cola SPSC y el ensamblador de captura están implementados como módulos host-independientes, pero todavía no están conectados al callback real del VST3 ni a un hilo worker de análisis.
- No captura una región completa del DAW.
- No hay todavía coordinación real entre el callback del VST3 y un hilo worker; la cola y el ensamblador solo están probados como módulos host-independientes.
- El detector de actividad por ventanas ya produce regiones candidatas y conserva pausas breves, pero sigue siendo una primera pasada energética; no implementa todavía refinamiento fonético ni segmentación final de frases.
- Perfiles provisionales de frase con medianas de RMS, pico, media absoluta y factor de cresta, usando solo ventanas activas válidas.
- Referencia adaptativa experimental por cuantíl de perfiles elegibles y MAD, calculada dentro de la propia grabación; no impone un nivel absoluto.
- Recomendaciones Clip Gain relativas por frase, con intensidad y límites configurables, y abstención por confianza. Son propuestas de ganancia estática, no procesamiento automático ni curvas finales.
- No implementa referencia adaptativa.
- No implementa todavía curvas Clip Gain finales, editor manual ni motor Clip Peak.
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
