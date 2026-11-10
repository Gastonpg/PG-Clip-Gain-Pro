# PG Clip Gain Pro — Base de implementación v0.1

**Estado:** base C++17 compilada localmente con CMake/GCC en Release; 5/5 ejecutables de prueba aprobados.
**Alcance:** contratos de dominio independientes del host VST3. No contiene todavía captura de audio, análisis de dos pasadas, segmentación ni motores Clip Gain/Clip Peak.

## 1. Decisión de implementación

- C++17 como base del núcleo DSP y del banco de pruebas.
- Biblioteca estática independiente del SDK del host.
- CMake para configurar compilación y pruebas.
- Sin dependencias externas en el primer banco de pruebas.
- El núcleo no incluye interfaz gráfica ni código específico de FL Studio.

## 2. Primeros contratos implementados

- Índices temporales enteros de 64 bits (FrameIndex).
- Intervalos [start, end): inicio incluido y final excluido.
- Formato de audio con frecuencia de muestreo y cantidad de canales.
- Estado explícito de captura y bandera de discontinuidades.
- Configuración versionada de ventana y salto.
- Puntos de curva estática expresados en dB.
- Validadores que rechazan rangos vacíos, límites fuera de captura, formatos no soportados, configuraciones inválidas, ganancias no finitas y puntos de curva desordenados o duplicados.
- Conversión de dB a amplitud lineal mediante 10^(dB/20).
- Generadores PCM deterministas en memoria para silencio, seno e impulso, con soporte mono/estéreo y canal derecho invertido para pruebas de polaridad.
- Acumulador de captura por bloques con memoria preasignada, longitud esperada explícita, verificación de continuidad, detección de huecos/solapamientos, límite de capacidad y estados completo/parcial/cancelado.
- Primera pasada numérica inicial: descriptores por ventana y por canal (RMS, pico de muestra, validez finita y silencio digital), con cobertura de cola parcial y sin sumar canales.
- Constructor de snapshot compartido e inmutable: exige captura completa/continua, cobertura total, coincidencia de formato y configuración, y métricas coherentes. Conserva ventanas inválidas marcadas para que los motores puedan abstenerse localmente.
- Primera pasada numérica inicial: descriptores por ventana y por canal (RMS, pico de muestra, validez finita y silencio digital), con cobertura de cola parcial y sin sumar canales.

## 3. Límites deliberados

La primera versión admite únicamente mono y estéreo en el contrato inicial. No se mezclan canales para análisis sin una política explícita. No se fija todavía un máximo de ganancia, una ventana de análisis, un umbral de actividad ni una pendiente máxima de curva: esos parámetros requieren pruebas y escucha.

Los validadores son defensas de entrada, no sustituyen las pruebas del algoritmo ni garantizan por sí solos calidad sonora. El analizador de ventanas opera offline sobre una vista PCM cuya memoria y longitud declara el llamador; no toca el hilo de audio ni realiza segmentación o decisiones musicales. El analizador de ventanas opera offline sobre una vista PCM cuya memoria y longitud declara el llamador; no toca el hilo de audio ni realiza segmentación o decisiones musicales.

## 4. Pruebas iniciales

tests/core/test_types.cpp contiene comprobaciones deterministas activas también en Release para formatos, rangos temporales, estados de captura, configuración de análisis, integridad de curvas y conversión de ganancia. La ejecución local terminó con 5/5 pruebas aprobadas. El snapshot no se construye si la captura es parcial, hay huecos, falta cobertura, cambia la frecuencia de muestreo o no coincide la configuración usada en el análisis. La captura solo se marca completa cuando coincide exactamente con la longitud esperada; una detención temprana nunca habilita conclusiones globales. Los descriptores RMS/pico son mediciones básicas de la primera pasada, no una métrica final de sonoridad percibida ni un detector de voz. Las comprobaciones usan una función de verificación propia, no assert, para seguir activas en Release.

## 5. Próximas etapas

1. Verificar la compilación automatizada en Windows y Linux; la compilación local en Linux ya pasó.
2. Ampliar pruebas de límites, overflow y datos inválidos.
3. Ampliar fixtures PCM deterministas con envolventes, transitorios y señales proxy para segmentación y sonoridad.
4. Integrar el acumulador con el ciclo de vida real del host, incluidos seek, loop, stop y cambios de región.
5. Ampliar la primera pasada con descriptores espectrales/transitorios versionados; después definir la segunda pasada contextual sobre el snapshot común.

Los cinco jueces de PG Audio Intelligence permanecen fuera de este alcance y no se implementarán sin autorización explícita.
