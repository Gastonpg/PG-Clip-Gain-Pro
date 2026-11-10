# PG Clip Gain Pro — Plan de los cinco jueces: conocimiento, entrenamiento y evaluación

**Estado:** plan de preparación. Los jueces no están implementados ni entrenados. Este documento no atribuye recomendaciones no verificadas a profesionales concretos.

## 1. Propósito

Los cinco jueces serán evaluadores especializados que revisan propuestas de Clip Gain y Clip Peak. No procesan audio directamente, no modifican curvas y no reemplazan la decisión del usuario. Su trabajo es aportar evidencia, detectar riesgos y recomendar aprobar, reducir, rechazar o abstenerse.

## 2. Cinco responsabilidades independientes

1. **Interpretación musical:** distingue una variación expresiva de un error probable. Prioriza intención, fraseo, énfasis, dinámica y naturalidad. Se abstiene cuando no hay contexto musical suficiente.
2. **Nivelación vocal:** evalúa consistencia percibida entre palabras, frases y secciones; selecciona referencias internas robustas sin imponer un objetivo fijo de dBFS.
3. **Transitorios y picos:** identifica anomalías locales comparadas con su contexto; protege consonantes, ataques, plosivas intencionales y respiraciones.
4. **Coherencia global:** evalúa continuidad entre secciones y cambios de energía intencionales; no uniforma versos, coros o dobles por defecto.
5. **Árbitro principal:** revisa evidencia y desacuerdos; puede aprobar, reducir, rechazar, pedir revisión humana o abstenerse. No fuerza consenso artificial.

## 3. Qué significa entrenarlos

No confundir tres niveles distintos:

- **Base de conocimiento:** principios documentados, glosario, ejemplos y contraejemplos. Sirve para formular criterios, pero no es entrenamiento de un modelo.
- **Jueces deterministas:** reglas y evaluadores reproducibles con métricas, umbrales calibrados y razones legibles. Es la primera etapa recomendada.
- **Modelo entrenado:** requiere corpus de audio con licencias y consentimiento adecuados, decisiones etiquetadas por personas, desacuerdos documentados, particiones de entrenamiento/validación/prueba, evaluación ciega y ciclos de calibración. No basta con escribir prompts ni cargar entrevistas.

## 4. Ficha obligatoria de cada decisión

Cada juez devuelve una estructura común:
- identificador del fragmento y rango temporal;
- motor y versión de análisis;
- evidencia observada y métricas utilizadas;
- interpretación o hipótesis, separada de los hechos medidos;
- acción propuesta y magnitud en dB;
- confianza calibrada y motivos de incertidumbre;
- riesgo para expresión, consonantes, respiraciones, colas y transitorios;
- decisión: aprobar, reducir, rechazar, revisión humana o abstención;
- explicación breve y reproducible.

Ningún juez puede inventar métricas no disponibles ni presentar una inferencia como certeza.

## 5. Fuentes y atribución profesional

Se podrá estudiar material público de ingenieros de mezcla (entrevistas, charlas, artículos y demostraciones). Para cada principio:
- guardar URL, autor, fecha y contexto;
- resumir con palabras propias;
- separar cita documentada de interpretación del equipo;
- no reproducir cursos, libros o materiales protegidos;
- no atribuir una regla a un profesional si la fuente no la respalda;
- no sugerir aval, colaboración ni emulación oficial.

Los nombres de profesionales sirven como referencias de investigación, no como identidades de modelos ni como marketing.

## 6. Cómo crear el conjunto de entrenamiento

1. Definir consentimiento, privacidad, derechos y condiciones de uso de cada grabación.
2. Reunir voces diversas y condiciones de grabación variadas; no filtrar solo tomas limpias.
3. Crear ejemplos de errores reales, decisiones expresivas, casos ambiguos y casos donde no se debe tocar nada.
4. Etiquetar cada evento con varios evaluadores humanos independientes.
5. Registrar el motivo del desacuerdo; no convertir el voto mayoritario en verdad automática.
6. Separar por sesión/artista/grabación para evitar que fragmentos de la misma toma aparezcan en entrenamiento y prueba.
7. Mantener un conjunto de prueba bloqueado y ejemplos adversariales.
8. Medir falsos positivos, falsos negativos, magnitud de corrección, estabilidad, calibración de confianza y tasa de abstención.
9. Comparar contra reglas deterministas y contra una referencia humana.
10. Rechazar cualquier versión que mejore consistencia a costa de destruir expresión o consonantes.

## 7. Evaluación y puertas de seguridad

- **Interpretación:** medir cuántas variaciones expresivas fueron modificadas indebidamente.
- **Nivelación:** medir consistencia percibida y desviación de la referencia elegida, sin premiar uniformidad absoluta.
- **Picos:** medir detección de anomalías reales y daño a transitorios/consonantes.
- **Coherencia:** medir preservación de contrastes intencionales entre secciones.
- **Árbitro:** medir decisiones correctas de abstención, detección de conflictos y reducción de acciones riesgosas.

Pruebas obligatorias: voz susurrada, voz proyectada, frase emocional, consonantes fuertes, plosivas, respiraciones, colas, dobles, armonías, silencios, ruido de fondo, saturación de origen y cambios intencionales de energía.

## 8. Protocolo de trabajo con el usuario

Cuando llegue el momento, te voy a enseñar el proceso paso a paso:
1. elegir ejemplos de audio que tengas derecho a usar;
2. construir una tabla sencilla de decisiones humanas;
3. escuchar y etiquetar cada caso con criterios claros;
4. comparar los cinco jueces con tus decisiones;
5. revisar desacuerdos y corregir criterios;
6. crear un conjunto de prueba separado;
7. evaluar si realmente mejoran respecto del DSP determinista.

No necesitás saber programar ni entrenar redes neuronales para participar en el etiquetado y la evaluación. La parte de preparación técnica la organizaremos juntos y con instrucciones en español.

## 9. Criterio de éxito

Los jueces no se consideran buenos por generar muchas correcciones ni por coincidir siempre. Deben detectar cuándo intervenir, cuantificar el riesgo, explicar sus decisiones y abstenerse cuando la evidencia no alcance. Una tasa alta de abstención puede ser correcta si evita dañar la interpretación.
