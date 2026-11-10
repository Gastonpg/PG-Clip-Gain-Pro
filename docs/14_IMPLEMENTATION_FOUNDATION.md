# PG Clip Gain Pro — Base de implementación v0.2

**Estado:** núcleo C++17 compilado localmente con CMake/GCC en Release y Debug con AddressSanitizer/UndefinedBehaviorSanitizer; 6/6 suites aprobadas en ambos modos.
**Alcance:** base DSP independiente del host. Incluye contratos, fixtures sintéticos, acumulador de captura acotado, descriptores iniciales por ventana, snapshot compartido y estadísticas contextuales. Todavía no es un plugin VST3 ni implementa la segmentación vocal o los motores completos.

## 1. Decisiones de implementación

- C++17 para el núcleo y el banco de pruebas.
- Biblioteca estática sin dependencia del SDK VST3.
- CMake y pruebas unitarias sin dependencias externas.
- DSP y análisis offline separados de la interfaz gráfica.
- Parámetros musicales y umbrales pendientes de validación con corpus vocal y escucha; no se fijan constantes arbitrarias.

## 2. Código implementado

### Contratos y validación
- Índices temporales enteros de 64 bits.
- Intervalos [start, end), con inicio incluido y final excluido.
- Formatos mono/estéreo para el primer prototipo.
- Estados de captura y discontinuidades explícitos.
- Configuración de análisis versionada.
- Validación de rangos, formatos, ventanas, saltos, curvas y valores numéricos.
- Conversión dB a amplitud lineal mediante 10^(dB/20); los valores no finitos se mantienen detectables y no se silencian de forma encubierta.

### Fixtures sintéticos
- Generación determinista en memoria de silencio, seno e impulso.
- Casos mono/estéreo y polaridad invertida para probar que el análisis no suma canales de forma peligrosa.
- Validaciones de frecuencia, amplitud y límites temporales.

### Captura acotada
- Memoria preasignada y capacidad fija.
- Longitud esperada explícita para cada región.
- Aceptación de bloques contiguos con posición temporal.
- Detección de huecos y solapamientos.
- Estados completo, parcial, discontinuo y cancelado.
- Una región solo se marca completa si se recibieron exactamente todos los frames esperados.

El acumulador es independiente de VST3 y no es thread-safe. El llamador debe serializar sus operaciones y mantener el callback quiescente durante begin/finish/cancel. La integración de transporte, seek, loop, stop y cola real del callback sigue pendiente.

### Primera pasada numérica inicial
- Descriptores por ventana y por canal: RMS, pico de muestra, validez finita y silencio digital.
- Cobertura de la cola parcial final.
- Canales analizados por separado; no se hace downmix.
- Las ventanas con muestras no finitas quedan marcadas como evidencia inválida, sin contaminar las métricas numéricas.

Estos descriptores son una base matemática inicial, no una métrica final de sonoridad percibida, un detector de voz ni un detector de transitorios profesional.

### Snapshot compartido
- Publicación como objeto compartido de solo lectura.
- Requiere captura completa y continua.
- Verifica identidad de formato, número de frames y configuración exacta usada en el análisis.
- Comprueba orden, cobertura, canales y coherencia de métricas.
- Conserva ventanas inválidas señaladas para que los motores puedan abstenerse localmente.

### Estadísticas contextuales descriptivas
- Cuenta ventanas válidas, inválidas, silenciosas y no silenciosas por canal.
- Calcula mediana, desviación absoluta mediana (MAD) y rangos de RMS/pico.
- No establece referencia adaptativa, umbrales de actividad, frases ni intención musical.

## 3. Validación realizada

Seis ejecutables de pruebas cubren:
1. contratos numéricos y curvas;
2. fixtures PCM;
3. captura contigua, huecos, solapamientos, capacidad y truncamiento;
4. cobertura, RMS, pico, silencio y muestras no finitas;
5. construcción y rechazo de snapshots inconsistentes;
6. estadísticas contextuales, silencio y evidencia inválida.

**Resultado local:** 6/6 suites aprobadas en Release y 6/6 en Debug con AddressSanitizer y UndefinedBehaviorSanitizer.

La CI de GitHub está configurada para compilar y ejecutar CTest en Ubuntu y Windows, pero todavía no hay una ejecución remota confirmada. La compilación verificada hasta ahora es local en Linux/GCC.

## 4. Límites actuales

- No hay todavía proyecto/descriptor VST3 ni binario instalable.
- No se conectó la captura al callback real de un DAW.
- No existe aún cola SPSC ni coordinación worker/callback.
- No están resueltos los cambios de transporte, seek, loop, render offline ni restauración del host.
- La primera pasada todavía no incorpora todos los descriptores espectrales/transitorios definidos.
- No existe aún segmentación vocal, protección fonética refinada ni segunda pasada completa.
- No se implementaron Clip Gain, Clip Peak, curvas finales ni interfaz gráfica.
- No se crearon los cinco jueces de PG Audio Intelligence.

## 5. Próximas etapas

1. Integrar y probar la adquisición VST3 con un ciclo de vida seguro y una transferencia acotada callback/worker.
2. Ampliar fixtures para envolventes, transitorios, ruido, respiraciones proxy y colas.
3. Ampliar descriptores de primera pasada y validar invariancia ante distintos tamaños de bloque.
4. Desarrollar la segunda pasada contextual: continuidad, actividad candidata, pausas, límites y zonas protegidas.
5. Validar segmentación con grabaciones vocales reales y anotación humana.
6. Implementar perfiles de frase y referencia adaptativa, sin objetivo absoluto fijo.
7. Implementar curvas Clip Gain y probarlas mediante escucha A/B.
8. Implementar Clip Peak como motor independiente.
9. Integrar snapshots, coordinación, persistencia y pruebas en DAWs.
10. Diseñar la GUI después de validar el comportamiento DSP.

Los cinco jueces de PG Audio Intelligence permanecen fuera de este alcance y no se implementarán sin autorización explícita.
