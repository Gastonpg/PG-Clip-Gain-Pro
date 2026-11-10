# 04 — Motor Clip Peak: detección y atenuación localizada

Estado: diseño de algoritmo; criterios y parámetros sujetos a validación.  
Proyecto: PG Clip Gain Pro.

## 1. Propósito y límites

Clip Peak identifica eventos de pico local que puedan resultar problemáticos dentro de una interpretación vocal y propone una atenuación breve, localizada y suave. No es un limitador, compresor, de-esser ni un sistema que persigue eliminar todos los picos altos.

Un pico alto puede ser una consonante, una plosiva, un ataque expresivo o una parte legítima de la interpretación. La amplitud máxima por sí sola no demuestra que exista un problema.

## 2. Evidencia para detectar anomalías

El motor combina contexto local y descriptores complementarios:

- Pico absoluto y pico relativo al entorno inmediato.
- Energía en ventanas vecinas y duración del evento.
- Factor de cresta y forma de la envolvente.
- Pendiente de ataque y caída.
- Relación con el cuerpo de la sílaba o frase.
- Clasificación contextual procedente del análisis de actividad vocal.
- Posible presencia de consonantes, plosivas, respiraciones o ruido.
- Confianza y riesgo de alterar la interpretación.

Los umbrales no se fijan aquí. Deben probarse con distintas voces, micrófonos, distancias, estilos y niveles de grabación.

## 3. Anomalía frente a transitorio legítimo

El motor no debe señalar automáticamente:
- Cada consonante explosiva.
- Cada ataque fuerte de una palabra.
- Cada nota aguda o intensa.
- Cada pico aislado que forme parte de una interpretación expresiva.
- Cada respiración marcada.

Un candidato gana relevancia cuando su nivel sobresale del contexto de manera consistente y existe evidencia de que el evento puede desequilibrar la señal o dificultar el procesamiento posterior.

Cuando la clasificación sea ambigua, se marca para revisión manual o se abstiene.

## 4. Propuesta de atenuación

Cada candidato produce una recomendación localizada con:
- Inicio, centro y final del evento.
- Pico y descriptores de contexto.
- Nivel de atenuación sugerido.
- Forma y duración de la curva propuesta.
- Confianza.
- Riesgo para consonantes, inteligibilidad, ataque y naturalidad.
- Explicación de por qué se considera anómalo.

La curva debe reducir el evento sin crear un hueco audible ni cambiar innecesariamente el cuerpo de la palabra. La atenuación se aplica mediante una curva de ganancia estática y suave, nunca mediante una reacción dinámica al nivel instantáneo.

## 5. Forma de la curva

La región afectada debe incluir rampas de entrada y salida adaptadas al evento y a la fonética. Las rampas deben:
- Evitar discontinuidades y clics.
- Preservar el ataque útil cuando sea posible.
- Evitar atenuar consonantes vecinas que no sean parte del problema.
- No invadir otras sílabas o palabras sin evidencia.
- Ser editables por el usuario.

No se establecen tiempos ni pendientes definitivos hasta validar con material real.

## 6. Independencia y convivencia con Clip Gain

Clip Peak conserva su propia curva y su propio estado de aceptación. Clip Gain no debe absorber ni ocultar sus decisiones.

Cuando ambas curvas coincidan en tiempo:
- Mostrar ambas de forma diferenciada.
- Calcular la ganancia compuesta de manera explícita.
- Evitar doble atenuación excesiva.
- Informar al usuario qué motor aporta cada parte de la corrección.
- Permitir escuchar y desactivar cada motor por separado.

Las ediciones manuales bloqueadas prevalecen. Una nueva detección no sobrescribe una corrección manual.

## 7. Controles del usuario

El usuario puede:
- Revisar candidatos individualmente.
- Aceptar o rechazar cada evento.
- Ajustar profundidad y extensión temporal.
- Dibujar o retocar la curva.
- Bloquear regiones.
- Desactivar Clip Peak sin afectar Clip Gain.

Los controles de intensidad deben modular la propuesta, no convertir el motor en un limitador automático.

## 8. Pruebas obligatorias

Incluir:
- Plosivas fuertes y suaves.
- Consonantes explosivas en distintos lugares de la palabra.
- Ataques expresivos.
- Notas sostenidas con picos de inicio.
- Respiraciones.
- Ruido impulsivo externo.
- Saturación ya presente en la grabación.
- Frases con gran dinámica.
- Eventos de pico superpuestos a correcciones de Clip Gain.

Evaluar:
- Detección correcta de anomalías reales.
- Tasa de falsas alarmas sobre transitorios legítimos.
- Conservación de inteligibilidad y ataque.
- Ausencia de clics.
- Naturalidad en escucha ciega.
- Coherencia entre visualización y resultado audible.
- Comportamiento al combinar ambos motores.

## 9. Reglas de abstención

No proponer corrección automática confiada cuando:
- No se distingue el evento del ruido de fondo.
- El pico puede ser una consonante esencial o un ataque expresivo.
- La segmentación es dudosa.
- El evento está recortado o saturado de origen y no puede repararse con Clip Gain.
- La curva invadiría una región protegida.
- La combinación con Clip Gain generaría una reducción desproporcionada.

En esos casos, mostrar el evento como incierto o no actuar.

## 10. Decisiones pendientes

- Descriptores y umbrales de anomalía.
- Ventanas temporales y contexto local.
- Curvas y rampas apropiadas por tipo de evento.
- Límites de atenuación.
- Reglas de composición con Clip Gain.
- Métricas y corpus de validación.

No se debe afirmar que el motor detecta plosivas o anomalías con precisión profesional hasta demostrarlo mediante pruebas repetibles y evaluación auditiva.
