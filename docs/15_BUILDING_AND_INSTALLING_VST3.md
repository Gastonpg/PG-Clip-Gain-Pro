# PG Clip Gain Pro — Compilación e instalación VST3 en Windows

**Objetivo de primera entrega:** FL Studio 2026 de 64 bits en Windows x64. Se replica el método de entrega que ya funcionó con PG Vocal Leveler Balanced: compilar en GitHub Actions, descargar el artefacto ZIP, extraer el bundle VST3 completo en la carpeta común y reiniciar FL Studio para que vuelva a escanear. No hace falta compilar en la PC del usuario ni instalar Visual Studio.

## 1. Obtener el paquete

La CI debe terminar correctamente en `windows-latest` x64, incluyendo:
- compilación C++17 del núcleo;
- compilación del bundle VST3 x64;
- ejecución del Validator oficial de Steinberg durante el post-build;
- generación de `moduleinfo.json`;
- siete suites de pruebas del núcleo.

El artefacto de Windows se publica con el nombre `PG-Clip-Gain-Pro-VST3-windows-latest`; GitHub Actions lo entrega como ZIP descargable. No instalar un artefacto de una ejecución fallida o sin el Validator aprobado.

## 2. Instalar

1. Descargar y extraer el artefacto de Windows.
2. Comprobar que contiene la carpeta completa `PGClipGainPro.vst3`, no solamente la DLL interna.
3. Copiar la carpeta completa a:

   `C:\Program Files\Common Files\VST3\`

4. Cerrar y volver a abrir FL Studio 2026.
5. Abrir el administrador de plugins de FL Studio y ejecutar una búsqueda de plugins (rescan).
6. Comprobar que aparece `PG Clip Gain Pro` en la categoría de efectos.

No renombrar la carpeta ni extraer únicamente el archivo binario interno: la estructura del bundle contiene metadatos necesarios.

## 3. Prueba inicial de compatibilidad

La primera compilación es un shell transparente:
- debe cargarse como efecto VST3;
- debe aceptar buses mono y estéreo;
- debe aceptar procesamiento float32 y float64;
- debe entregar audio de salida idéntico a la entrada, salvo los canales marcados como silencio;
- debe permitir guardar y restaurar el estado de versión;
- no debe generar errores al abrir/cerrar el proyecto ni al iniciar/detener reproducción.

Todavía no tiene editor gráfico ni aplica Clip Gain. Esta primera prueba confirma el contrato del plugin y la integración con el host, no la finalización del producto.

## 4. Regresión en FL Studio

Antes de declarar compatibilidad:
1. Probar una pista mono y otra estéreo.
2. Reproducir audio con distintos tamaños de buffer.
3. Confirmar que no hay clics, cortes ni cambio de nivel.
4. Guardar el proyecto, cerrar FL Studio, volver a abrirlo y comprobar que el plugin sigue cargando.
5. Duplicar la instancia y comprobar que ambas funcionan.
6. Cerrar el proyecto y retirar el plugin sin bloquear FL Studio.

Registrar versión de FL Studio, versión de Windows, arquitectura y resultado de cada prueba.

## 5. Limitación de la validación automatizada

El Validator de Steinberg y CI reducen fallos de API y compilación, pero no sustituyen la prueba manual en FL Studio. La compatibilidad final solo se declara después de completar ambas.
