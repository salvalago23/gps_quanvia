import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from scipy.spatial.transform import Rotation as R
import os
from datetime import datetime

from src.config import config
from src.utils import logger, error_handler, SimulationError

def get_csv_filepath(duration):
    # Create sim_data directory in the project root if it doesn't exist
    sim_data_dir = config.OUTPUT_DIR
    if not os.path.exists(sim_data_dir):
        os.makedirs(sim_data_dir)

    # Create a directory for today's date if it doesn't exist
    today = datetime.now().strftime('%d-%m-%Y')
    today_dir = os.path.join(sim_data_dir, today)
    if not os.path.exists(today_dir):
        os.makedirs(today_dir)

    # Create a filename with timestamp
    timestamp = datetime.now().strftime('%d%m%Y_%H%M%S')
    filename = f'sim_data_{timestamp}_duration_{duration:.2f}.csv'
    
    return os.path.join(today_dir, filename)

def format_lat_lon(lat, lon, alt):
    def format_coord(value, pos, neg):
        return f"{abs(value):.4f}°{pos if value >= 0 else neg}"
    
    lat_str = format_coord(lat, "N", "S")
    lon_str = format_coord(lon, "E", "W")
    return f"{lat_str}, {lon_str}, alt: {alt:.2f}m"

class Simulator:
    def __init__(self, dt=config.DT, noise_level=config.NOISE_LEVEL, mag_inclination=config.MAG_INCLINATION, mag_declination=config.MAG_DECLINATION):
        self.dt = dt
        self.noise_level = noise_level
        # Magnetic field parameters (in degrees)
        self.mag_inclination = np.radians(mag_inclination)
        self.mag_declination = np.radians(mag_declination)

    def initialize(self, duration):
        self.duration = duration
        self.time = np.arange(0, self.duration, self.dt)
        self.num_steps = len(self.time)
        
        # Initialize data arrays 
        self.position = np.zeros((self.num_steps, 3))
        self.velocity = np.zeros((self.num_steps, 3))
        self.acceleration = np.zeros((self.num_steps, 3))
        self.orientation = np.zeros((self.num_steps, 3))
        self.angular_velocity = np.zeros((self.num_steps, 3))
        
        # Initialize with realistic values
        if config.RANDOMIZE_START:
            initial_lat = np.random.uniform(config.MIN_LATITUDE, config.MAX_LATITUDE)
            initial_lon = np.random.uniform(config.MIN_LONGITUDE, config.MAX_LONGITUDE)
            initial_alt = np.random.uniform(config.MIN_ALTITUDE, config.MAX_ALTITUDE)
        else:
            initial_lat = config.INITIAL_LATITUDE + np.random.uniform(-config.POSITION_VARIATION, config.POSITION_VARIATION)
            initial_lon = config.INITIAL_LONGITUDE + np.random.uniform(-config.POSITION_VARIATION, config.POSITION_VARIATION)
            initial_alt = config.INITIAL_ALTITUDE + np.random.uniform(-config.POSITION_VARIATION * 1000, config.POSITION_VARIATION * 1000)

        self.position[0] = [initial_lat, initial_lon, initial_alt]
        
        # Sensor readings
        self.acc_readings = np.zeros((self.num_steps, 3))
        self.gyro_readings = np.zeros((self.num_steps, 3))
        self.mag_readings = np.zeros((self.num_steps, 3))
        self.gps_readings = np.zeros((self.num_steps, 2))
        self.alt_readings = np.zeros(self.num_steps)

        # Initialize GPS and altimeter readings
        self.gps_readings[0] = [initial_lat, initial_lon]
        self.alt_readings[0] = initial_alt

        logger.info(f"Simulation initialized at LAT: {initial_lat:.4f}°, LON: {initial_lon:.4f}°, alt: {initial_alt:.2f}m")

    def generate_path(self):
        # Generate a 3D path with turns AND elevation changes
        t = self.time

        # Generate variations around the initial position
        lat_variation = config.LAT_AMPLITUDE * np.sin(config.LAT_FREQUENCY * t) + config.LAT_AMPLITUDE/2 * np.sin(2*config.LAT_FREQUENCY * t)
        lon_variation = config.LON_AMPLITUDE * np.cos(config.LON_FREQUENCY * t) + config.LON_AMPLITUDE/2 * np.cos(2*config.LON_FREQUENCY * t)
        alt_variation = config.ALT_AMPLITUDE * np.sin(config.ALT_FREQUENCY * t)
        
        # Add variations to the initial position
        self.position[:, 0] = self.position[0, 0] + lat_variation + np.random.normal(0, 0.0001, self.num_steps)
        self.position[:, 1] = self.position[0, 1] + lon_variation + np.random.normal(0, 0.0001, self.num_steps)
        self.position[:, 2] = self.position[0, 2] + alt_variation + np.random.normal(0, 0.1, self.num_steps) # Z-axis (elevation)

        # Calculate velocity and acceleration
        self.velocity = np.gradient(self.position, self.dt, axis=0)
        self.acceleration = np.gradient(self.velocity, self.dt, axis=0)
        
        # Calculate orientation (roll, pitch, yaw)
        self.orientation[:, 2] = np.arctan2(np.gradient(self.position[:, 1], self.dt), np.gradient(self.position[:, 0], self.dt))  # yaw
        self.orientation[:, 1] = np.arctan2(-np.gradient(self.position[:, 2], self.dt), np.sqrt(np.gradient(self.position[:, 0], self.dt)**2 + np.gradient(self.position[:, 1], self.dt)**2))  # pitch
        self.orientation[:, 0] = np.random.normal(0, 0.1, self.num_steps)  # Add some random roll
        
        # Calculate angular velocity
        self.angular_velocity = np.gradient(self.orientation, self.dt, axis=0)

    def add_noise(self, data):
        return data + np.random.normal(0, self.noise_level, data.shape)

    def generate_sensor_data(self):
        for i in range(self.num_steps):
            # Create rotation matrix
            r = R.from_euler('xyz', self.orientation[i], degrees=False)
            
            # Generate accelerometer readings
            acc_global = self.acceleration[i] + np.array([0, 0, 9.81])  # Add gravity
            self.acc_readings[i] = r.apply(acc_global, inverse=True)
            
            # Generate gyroscope readings
            self.gyro_readings[i] = self.angular_velocity[i]
            
            # Generate magnetometer readings
            mag_field = np.array([np.cos(self.mag_inclination), 0, -np.sin(self.mag_inclination)])
            mag_field = R.from_euler('z', self.mag_declination).apply(mag_field)
            self.mag_readings[i] = r.apply(mag_field, inverse=True)

            
            self.gps_readings[i] = self.add_noise(self.position[i, :2])
            self.alt_readings[i] = self.add_noise(self.position[i, 2])


        # Add noise to IMU sensors readings
        #self.acc_readings = self.add_noise(self.acc_readings)
        #self.gyro_readings = self.add_noise(self.gyro_readings)
        #self.mag_readings = self.add_noise(self.mag_readings)

        # Generate GPS and altimeter readings
        #self.gps_readings = self.add_noise(self.position[:, :2])
        #self.alt_readings = self.add_noise(self.position[:, 2])

    @error_handler
    def simulate(self, duration):
        #logger.info(f"Simulating...")
        self.initialize(duration)
        self.generate_path()
        self.generate_sensor_data()
        #logger.info(f"Simulation completed after {duration:.2f}s")

    @error_handler
    def get_data(self):
        data = pd.DataFrame({
            'time': np.round(self.time, 1),  # Round time to 1 decimal
            'acc_x': self.acc_readings[:, 0],
            'acc_y': self.acc_readings[:, 1],
            'acc_z': self.acc_readings[:, 2],
            'gyro_x': self.gyro_readings[:, 0],
            'gyro_y': self.gyro_readings[:, 1],
            'gyro_z': self.gyro_readings[:, 2],
            'mag_x': self.mag_readings[:, 0],
            'mag_y': self.mag_readings[:, 1],
            'mag_z': self.mag_readings[:, 2],
            'gps_lat': self.gps_readings[:, 0],
            'gps_lon': self.gps_readings[:, 1],
            'altimeter': self.alt_readings,
            'true_lat': self.position[:, 0],
            'true_lon': self.position[:, 1],
            'true_alt': self.position[:, 2],
            'true_roll': self.orientation[:, 0],
            'true_pitch': self.orientation[:, 1],
            'true_yaw': self.orientation[:, 2]
        })
        
        # Round all columns except 'time' to 5 decimals
        for column in data.columns:
            if column != 'time':
                data[column] = data[column].round(5)
        
        logger.debug("Data retrieval successful")
        return data

    @error_handler
    def plot_path(self):
        fig = plt.figure(figsize=(15, 5))

        # 2D path
        ax1 = fig.add_subplot(121)
        ax1.plot(self.position[:, 0], self.position[:, 1], label='True Path')
        ax1.plot(self.gps_readings[:, 0], self.gps_readings[:, 1], 'r.', label='GPS Readings')
        ax1.legend()
        ax1.set_title(f'Simulated 2D Path (Duration: {self.duration:.2f}s)')
        ax1.set_xlabel('Latitude')
        ax1.set_ylabel('Longitude')
        ax1.axis('equal')
        
        # 3D path
        ax2 = fig.add_subplot(122, projection='3d')
        ax2.plot(self.position[:, 0], self.position[:, 1], self.position[:, 2])
        ax2.set_title('Simulated 3D Path')
        ax2.set_xlabel('Latitude')
        ax2.set_ylabel('Longitude')
        ax2.set_zlabel('Altitude')
        
        plt.tight_layout()
        plt.show()

        logger.info("Path plot generated correctly")

@error_handler
def run_simulations():
    if config.RANDOM_SEED is not None:
        np.random.seed(config.RANDOM_SEED)

    simulator = Simulator()
    
    if not os.path.exists(config.OUTPUT_DIR) and not config.DISABLE_CSV_OUTPUT:
        os.makedirs(config.OUTPUT_DIR)
        logger.info(f"Created output directory: {config.OUTPUT_DIR}")

    for i in range(config.NUM_SIMULATIONS):
        duration = np.random.uniform(config.MIN_DURATION, config.MAX_DURATION)
        logger.info(f"Starting simulation {i+1} with duration {duration:.2f}s")

        try:
            simulator.simulate(duration)
            data = simulator.get_data()

            if not config.DISABLE_CSV_OUTPUT:            
                filepath = get_csv_filepath(duration)

                # Saving the data to a CSV file making sure to round the values to 5 decimals
                data.to_csv(filepath, index=False)#, float_format='%.5f')
                
                logger.info(f"Simulation {i+1} completed. Data saved to {filepath}\n")
            else:
                logger.info(f"Simulation {i+1} completed. CSV output disabled.\n")

            if config.GENERATE_PLOTS and i == 0: # Only in the first simulation
                simulator.plot_path()

        except SimulationError as e:
            logger.error(f"Error in simulation {i+1}: {str(e)}\n")
            continue  # Move to the next simulation
    
    logger.info(f"All simulations completed.\n")
