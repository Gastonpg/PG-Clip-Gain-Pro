# PG Clip Gain Pro — Base de implementación v0.1

**Estado:** primer esqueleto de código creado y compilado localmente con CMake/GCC; pruebas iniciales aprobadas en configuración Release.
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

## 3. Límites deliberados

La primera versión admite únicamente mono y estéreo en el contrato inicial. No se mezclan canales para análisis sin una política explícita. No se fija todavía un máximo de ganancia, una ventana de análisis, un umbral de actividad ni una pendiente máxima de curva: esos parámetros requieren pruebas y escucha.

Los validadores son defensas de entrada, no sustituyen las pruebas del algoritmo ni garantizan por sí solos calidad sonora.

## 4. Pruebas iniciales

tests/core/test_types.cpp contiene comprobaciones deterministas activas también en Release para formatos, rangos temporales, estados de captura, configuración de análisis, integridad de curvas y conversión de ganancia. La ejecución local terminó con 1/1 pruebas aprobadas.

## 5. Próximas etapas

1. Verificar la compilación automatizada en Windows y Linux; la compilación local en Linux ya pasó.
2. Ampliar pruebas de límites, overflow y datos inválidos.
3. Implementar fixtures PCM deterministas y métricas de referencia.
4. Diseñar el acumulador de captura por bloques con detección de huecos, solapamientos y cambios de posición.
5. Implementar la primera pasada de descriptores sin segmentación heurística prematura.

Los cinco jueces de PG Audio Intelligence permanecen fuera de este alcance y no se implementarán sin autorización explícita.
