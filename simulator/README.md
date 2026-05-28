# quanvia_simulator

## Ejecución del programa
### Para instalar las librerías correctas se debe lanzar el comando:
pip install -r requirements.txt

### Ejemplo de comandos de ejecución:
python run.py --num_simulations 10 --min_duration 200 --max_duration 600 --output_dir custom_output --random_seed 123 --generate_plots
python run.py --num_simulations 1 --randomize_start --random_seed 42 --min_duration 0 --max_duration 4


## Programa para simular la obtención de medidas de un sistema de navegación
- Acelerómetro (3 ejes)
- Giroscopio (3 ejes)
- Magnetómetro (3 ejes)
- Altímetro o barómetro (escalar)
- GPS (lat, long)
El vector obtenido por los sensores tiene 12 features. 

En las simulaciones se recogen las medidas verdadera pero también se simulan mediciones ruidosas (por ahora se supone un ruido Gaussiano muy pequeño e igual para todos los sensores).
A partir de las mediciones instantáneas de los sensores, conociendo el dt (parámetro de la simulación), se puede integrar para obtener las velocidades y la variaciçon de la posición.


## Posibles mejoras en el código
- Individualizar el ruido para cada sensor.
- Crear un contenedor en Docker.

## Dudas
- La posición de trabajo como debería ser? Relativa a alguna referencia o absolutas (algo tipo WGS)? De tipo XYZ o (LAT,LONG,ALT)?
- El magnetómetro sirve para saber la orientación del sistema en todo momento pq es capaz de detectar el magnetismo terrestre (dirección del norte y de la gravedad)?

## A futuro
- El propio sistema es quien debe correr el modelo predictivo para conocer siempre su posición? O la computación se hace a distancia? En ese caso, que pasa si se pierde la comunicación total, _i.e._ no se recibe info tampoco de los sensores? Si los datos no se envían, de que sirve exactamente conocer localmente en todo momento la posición exacta del GPS?
- El tiempo que se tarde entre una predicción y la siguiente es crucial, ya que una lectura de sensores hecha en un momento concreto puede ser demasiado instantánea. Por ello, es posible que ese movimiento no se corresponde con los valores reales durante el resto del tiempo que hubo entre mediciones, puede que incluso sea momentáneamente contrario a la dirección promedio real (podría ser por ejemplo el caso de una ráfaga de viento en el caso de un dron).
- Supongo que el sistema debe estar compuesto por diversos módulos. Por ejemplo, uno de ellos podría resolver el problema anterior si se encarga de, durante el tiempo entre predicciones, leer los valores de los sensores con mucha frecuencia e ir calculando una media, que luego se enviará al predictor en el momento adecuado como si fuese un batch. Una especie de buffer que acumule las medidas y esté directamente conectado al módulo con el sistema predictor.

## Posible arquitectura del sistema
- El sistema final tendrá una arquitectura tipo LSTM o GRU, que será previamente entrenada con datos, bien de simulación o bien reales, a los que se le eliminará arbitrariamente medidas del GPS (y altura?), que serían las variables objetivo de la predicción.
- Una vez entrenado el modelo, en etapa de inferencia, su input será el vector con las medidas de los sensores, incluyendo el GPS. Cuando la señal GPS no se encuentre disponible, un switch hará que el output o predicción del modelo, sea realimentado a su entrada, sustituyendo en el vector de los sensores a las medidas GPS que no están disponibles.
- En los momentos en los que el GPS si que esté activo, se puede comparar la señal real con la predicha, para obtener una señal de error que puede ser aprovechada de alguna manera por el sistema (reentrenando el modelo sobre la marcha, realizando correcciones online sobre las medidas...)
- También sería interesante estudiar como se podría acoplar un filtro de Kalman o Bayesiano al sistema, sobre todo si nos encontramos con que el modelo LSTM se ve demasiado perjudicado por el ruido de los sensores (el filtro de Kalman es ideal ya que es capaz de modelar el ruido de los sensores).