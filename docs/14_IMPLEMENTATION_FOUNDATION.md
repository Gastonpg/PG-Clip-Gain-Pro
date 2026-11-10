# PG Clip Gain Pro — Base de implementación v0.4

**Estado:** núcleo DSP C++17, prototipos iniciales de análisis y primer módulo VST3 integrados en el proyecto. La versión anterior del núcleo pasó 6/6 suites localmente en Release y Debug con AddressSanitizer/UndefinedBehaviorSanitizer. Desde entonces se ampliaron descriptores y validaciones, y el banco actual tiene 12 suites configuradas; el código actual completo debe revalidarse. La compilación/validación VST3 está configurada en CI para Windows x64 y Linux x64, pero todavía no hay resultado remoto confirmado.
**Importante:** el VST3 actual es un shell transparente (passthrough), no el producto terminado. No se declara compatible con FL Studio hasta pasar el Validator y probar el binario en FL Studio 2026 x64.

## 1. Núcleo DSP host-independiente

- C++17, CMake y biblioteca estática PIC.
- Tipos temporales de 64 bits, rangos semiabiertos, validadores numéricos y curvas estáticas.
- Conversión dB/amplitud lineal.
- Fixtures PCM sintéticos deterministas: silencio, seno, impulso, mono/estéreo y polaridad.
- Captura acotada con memoria preasignada, control de continuidad y estados explícitos.
- Primera pasada por ventana/canal: RMS, pico, media absoluta, offset DC, tasa de cruces por cero y factor de cresta. No se suman canales.
- Snapshot intermedio inmutable: valida captura completa, formato, configuración exacta, grilla de ventanas, cobertura y coherencia de métricas.
- Estadísticas contextuales descriptivas: mediana, MAD y rangos por canal.
- Cola SPSC preasignada: transferencia de bloques planar a intercalado, índices atómicos lock-free de 64 bits, sin asignaciones ni bloqueos en push/pop, soporte float32/float64 y contador de descartes.
- Ensamblador worker-side que reconstruye capturas, comprueba continuidad, detecta desbordamientos y distingue completa/parcial/cancelada/discontinua. La finalización exige productor detenido y cola vacía; también admite que el usuario defina explícitamente el final de la región.
- Detector energético conservador: cuantiles relativos, histéresis, puente de pausas breves y regiones candidatas. Si no hay contraste suficiente, se abstiene.
- Perfiles provisionales de frase: medianas de RMS, pico, media absoluta y factor de cresta, calculadas solo con ventanas activas válidas.
- Referencia adaptativa experimental: cuantíl de niveles RMS de frases elegibles y MAD, siempre derivada de la propia grabación.
- Recomendaciones Clip Gain relativas por frase, con intensidad, límites de boost/cut y abstención por confianza.

**Alcance sonoro actual:** los perfiles usan RMS en dB como proxy energético, no LUFS ni una medida perceptual final. Las recomendaciones son propuestas estáticas y editables en concepto; todavía no se convierten en curvas aplicadas al audio. No hay compresión, limitación ni procesamiento dinámico.

## 2. Pruebas del núcleo

CMake define 10 ejecutables:
1. contratos y validadores;
2. fixtures PCM;
3. captura acotada;
4. descriptores por ventana;
5. snapshot y cobertura;
6. estadísticas contextuales;
7. cola SPSC y orden concurrente (20.000 bloques);
8. ensamblado de captura desde la cola;
9. detector de actividad y abstención;
10. perfiles, referencia adaptativa y recomendaciones Clip Gain.

La versión anterior de seis suites pasó localmente en Release y Debug con AddressSanitizer y UndefinedBehaviorSanitizer. Después se modificaron las suites de descriptores/snapshot y se añadieron cuatro suites nuevas. **Las 12 suites actuales deben ejecutarse de nuevo**; no se afirma que esta versión ampliada ya haya pasado.

## 3. Primera integración VST3

- SDK oficial Steinberg VST3 3.8.1 fijado al commit `3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96`, con submódulos recursivos.
- Integración CMake FetchContent.
- VSTGUI y plugins de ejemplo desactivados; Validator oficial y generación de `moduleinfo.json` activados.
- Procesador y controlador separados, enlazados por IDs propios.
- Bus principal mono/estéreo.
- Procesamiento float32 y float64.
- Ruta actual transparente: copia entrada a salida y respeta canales marcados como silencio.
- Estado de procesador/controlador con cabecera mágica y versión.
- El callback no realiza análisis, asignaciones, bloqueos ni operaciones de disco.

Se incluyen los avisos de distribución en:
- `third_party/licenses/STEINBERG_VST3_SDK_LICENSE.txt`
- `third_party/licenses/STEINBERG_CMAKE_PROJECTVERSION_TEMPLATE_NOTICE.txt`

El shell demuestra la estructura de factory, busses, tamaños de muestra, estado y procesamiento básico. Todavía no tiene editor gráfico, parámetros de captura ni motores conectados al procesador.

## 4. Compatibilidad objetivo y entrega

Objetivo principal: Windows x64 y VST3 para FL Studio 2026 de 64 bits. GitHub Actions compila con arquitectura x64 explícita; el usuario no necesita instalar Visual Studio ni compilar en su PC. Linux x64 se usa como plataforma secundaria de CI.

El bundle se genera como `PGClipGainPro.vst3`; el nombre visible en el DAW es `PG Clip Gain Pro`. El workflow ejecuta el Validator oficial como parte del build y publica el bundle completo dentro de un ZIP descargable. El método de entrega replica el flujo que ya funcionó con PG Vocal Leveler Balanced: copiar el bundle completo a `C:\Program Files\Common Files\VST3\`, reiniciar FL Studio y volver a escanear plugins.

La CI está configurada, pero no hay resultado remoto confirmado todavía. Un build verde tampoco sustituye la prueba manual en FL Studio.

## 5. Límites actuales

- El shell VST3 aún no está validado por CI ni probado en FL Studio.
- No tiene GUI ni controles de usuario.
- La cola SPSC y el ensamblador existen como módulos host-independientes, pero todavía no están conectados al callback real ni a un hilo worker dentro del VST3.
- No captura todavía una región real del DAW ni gestiona seek/loop/stop desde el transporte.
- La actividad es energética y candidata; no reconoce voz ni refina límites fonéticos.
- No protege todavía consonantes, respiraciones y colas mediante una segunda pasada validada.
- El perfil RMS es provisional; falta una métrica de sonoridad perceptual validada.
- La referencia adaptativa es un prototipo; necesita calibración con voces reales.
- Las recomendaciones Clip Gain aún no generan ni aplican curvas finales.
- Clip Peak ya genera eventos y curvas de atenuación estática candidatas; todavía no están integradas al VST3 ni se aplican al audio.
- No hay persistencia de curvas, edición manual, instalador ni paquete final de producto.

## 6. Próximas etapas

1. Conseguir compilación Windows x64 y Linux x64, 10/10 suites y Validator verde.
2. Instalar el artefacto Windows en FL Studio 2026 x64 y probar mono/estéreo, float32/float64, reproducción, duplicación de instancias y restauración de proyecto.
3. Integrar callback VST3 → cola SPSC → worker, con ciclo de vida seguro y transporte del host.
4. Resolver captura de regiones largas y estrategia de memoria/caché temporal.
5. Completar la segunda pasada contextual y refinar límites con protección fonética.
6. Validar actividad y segmentación con grabaciones vocales reales y anotación humana.
7. Sustituir el proxy RMS por medición perceptual validada y calibrar la referencia adaptativa.
8. Generar curvas Clip Gain estáticas, suaves, editables y aprobables por el usuario.
9. Implementar Clip Peak como motor independiente.
10. Añadir persistencia, undo, GUI oscura y waveform real después de validar el DSP.

Los cinco jueces de PG Audio Intelligence permanecen fuera de este alcance y no se implementarán sin autorización explícita.
