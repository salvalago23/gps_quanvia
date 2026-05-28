# GPS Quanvia — Sistema de Navegación por Dead Reckoning con Deep Learning

Sistema de navegación inercial que predice la posición GPS a partir de datos de sensores locales (IMU, magnetómetro, barómetro) cuando la señal GPS no está disponible. Combina redes neuronales recurrentes (LSTM/GRU) con filtros de Kalman para estimar la trayectoria en tiempo real.

---

## Motivación

La señal GPS puede perderse en túneles, edificios, zonas de interferencia o entornos de navegación autónoma. Este proyecto investiga cómo sustituir esa señal mediante un modelo predictivo entrenado sobre datos reales de sensores inerciales, permitiendo al sistema seguir estimando su posición con precisión razonable durante los períodos sin cobertura.

---

## Simulador de datos sintéticos

Dado que en las etapas iniciales del proyecto no se disponía de un prototipo físico, se desarrolló un simulador para generar datos sintéticos de sensores que permitieran avanzar con el desarrollo y validación de los modelos.

El simulador está integrado en este repositorio bajo `simulator/` (originalmente en [quanvia_simulator](https://github.com/salvalago23/quanvia_simulator)). Genera trayectorias 3D parametrizables con ruido gaussiano y produce los mismos vectores de sensores que el hardware real (acelerómetro, giroscopio, magnetómetro, GPS y altímetro). Consulta `simulator/README.md` para instrucciones de uso y ejemplos de ejecución.

---

## Estructura del repositorio

```
gps_quanvia/
├── simulator/                  # Simulador de datos sintéticos (ver simulator/README.md)
│   ├── src/
│   │   ├── simulator.py        # Lógica de simulación (trayectorias, sensores, ruido)
│   │   ├── config.py           # Parámetros configurables
│   │   ├── utils.py            # Logger y manejo de errores
│   │   └── main.py
│   ├── run.py                  # Punto de entrada CLI
│   └── requirements.txt
├── lstm_models/
│   ├── data/
│   │   ├── raw_data/           # Datos crudos del prototipo (11 trayectorias reales)
│   │   ├── clean_data/         # Datos normalizados y preprocesados
│   │   ├── train/              # Partición de entrenamiento (8 archivos)
│   │   └── test/               # Partición de test (3 archivos)
│   ├── models/                 # Modelos entrenados en formato .keras
│   ├── csv_processing.ipynb    # Preprocesamiento y normalización
│   ├── lstm.ipynb              # Modelo LSTM
│   ├── gru.ipynb               # Modelo GRU
│   ├── dense.ipynb             # Red densa (MLP)
│   ├── kalman.ipynb            # Filtro de Kalman + LSTM (GPS siempre disponible)
│   ├── kalman_gps_loss.ipynb   # Filtro de Kalman + LSTM (simulando pérdida de GPS)
│   └── plotter.ipynb           # Visualización de trayectorias y señales
├── src/
│   ├── gpstesteo/              # Sketch Arduino para lectura de GPS (TinyGPSPlus)
│   ├── Magnetometro_MotionCal/ # Calibración del magnetómetro
│   └── libraries/              # Bibliotecas de sensores (Adafruit BMP3XX, etc.)
├── requirements.txt
├── Informe_tecnico.pdf
└── Prototipo_sistema_navegación_Quanvia.pdf
```

---

## Datos de sensores

Cada registro de datos tiene una frecuencia de muestreo aproximada de 1 Hz y contiene las siguientes columnas:

| Columna | Descripción | Rango |
|---|---|---|
| `Acc_X/Y/Z` | Acelerómetro triaxial (g) | ±16 g |
| `Gyro_X/Y/Z` | Giroscopio triaxial (°/s) | ±2000 °/s |
| `Tilt_X/Y/Z` | Ángulos de inclinación (°) | ±180° |
| `Mag_X/Y/Z` | Magnetómetro triaxial (µT) | ±3000 µT |
| `Head` | Rumbo magnético (°) | 0–360° |
| `Temp` | Temperatura (°C) | −40–85°C |
| `Press` | Presión barométrica (hPa) | 300–1250 hPa |
| `Alt` | Altitud barométrica (m) | — |
| `Lat / Lng` | Posición GPS | — |

Las variables objetivo del modelo son `delta_Lat` y `delta_Lng`: el incremento de posición GPS en cada paso temporal.

---

## Preprocesamiento (`csv_processing.ipynb`)

1. Se calculan los deltas de posición (`delta_Lat`, `delta_Lng`, `delta_Alt`) como diferencias entre pasos consecutivos.
2. Las variables de entrada se normalizan con min-max basado en los rangos teóricos de cada sensor (no en estadísticos del dataset), lo que permite estabilidad en inferencia ante distribuciones no vistas.
3. Las columnas de tiempo y metadatos GPS (`Time`, `msDelta`, `Speed`, `HeadGPS`, etc.) se descartan del conjunto de entrenamiento.

---

## Modelos implementados

### LSTM (`lstm.ipynb`)
Red recurrente LSTM que recibe secuencias de `N` pasos temporales de los 16 features de sensores y predice los deltas de posición GPS del siguiente instante:

```
Entrada: secuencia (N, 16) → LSTM(128) → Dense(64, leaky_relu) → Dense(64) → Salida (2)
```

Se experimentó con distintas longitudes de secuencia (3, 5, 10, 20, 35 pasos) y distintos números de épocas. Los modelos entrenados se guardan en `models/` en formato `.keras`.

### GRU (`gru.ipynb`)
Variante más ligera de red recurrente:

```
Entrada: secuencia (N, 16) → GRU(64) → Dense(32, relu) → Salida (2)
```

### Red densa / MLP (`dense.ipynb`)
Experimento con arquitectura no recurrente que incluye en el vector de entrada los deltas de posición de los `k` pasos previos (`delta_Lat_t-1`, `delta_Lng_t-1`, ...) como features adicionales.

### Filtro de Kalman + LSTM (`kalman.ipynb`)
El LSTM predice los deltas de posición y el filtro de Kalman fusiona esa predicción con la medición GPS real cuando está disponible, suavizando la trayectoria y reduciendo la deriva acumulada.

- **Paso de predicción**: usa los deltas del LSTM para propagar el estado.
- **Paso de corrección**: cuando llega medición GPS, actualiza el estado con la ganancia de Kalman óptima.

### Filtro de Kalman + LSTM con pérdida de GPS (`kalman_gps_loss.ipynb`)
Extiende el notebook anterior simulando pérdida de señal GPS estocástica (parámetro `p_loss`). Cuando la señal no está disponible, el sistema opera en modo dead reckoning puro, usando únicamente la predicción del LSTM realimentada a través del filtro de Kalman.

---

## Arquitectura del sistema en inferencia

```
Sensores → [buffer de N muestras] → LSTM → delta_Lat, delta_Lng
                                              ↓
                                    Filtro de Kalman
                                              ↓
                              ┌───── GPS disponible? ─────┐
                              │ Sí: corrección con medición│
                              │ No: solo predicción LSTM  │
                              └────────────────────────────┘
                                              ↓
                                    Posición estimada
```

---

## Hardware (prototipo)

El firmware de adquisición está en `src/` como sketches de Arduino:

- **`gpstesteo.ino`**: lectura de módulo GPS via UART (`TinyGPSPlus`), extrayendo latitud, longitud, altitud, velocidad, rumbo y número de satélites a 1 Hz.
- **`Magnetometro_MotionCal.ino`** + `MotionCal.exe`: calibración del magnetómetro para compensar interferencias del entorno.

Las bibliotecas de sensores incluidas en `src/libraries/` cubren el barómetro/altímetro BMP3XX de Adafruit.

---

## Dependencias Python

Los notebooks requieren un entorno con:

```
tensorflow / keras
numpy
pandas
matplotlib
plotly
scikit-learn
```

---

## Documentación técnica

En la raíz del repositorio se encuentran dos documentos PDF con la descripción técnica del prototipo y el informe del sistema de navegación:

- `Prototipo_sistema_navegación_Quanvia.pdf`
- `Informe_tecnico.pdf`
