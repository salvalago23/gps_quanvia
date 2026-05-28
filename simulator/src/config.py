import os

class SimulationConfig:
    # Simulation parameters
    NUM_SIMULATIONS = 1
    MIN_DURATION = 100  # Minimum duration in seconds
    MAX_DURATION = 500  # Maximum duration in seconds
    DT = 0.1  # Time step in seconds

    # Sensor parameters
    NOISE_LEVEL = 0.1
    MAG_INCLINATION = 60  # in degrees
    MAG_DECLINATION = 0   # in degrees

    # Initial position parameters
    INITIAL_LATITUDE = 42.6029  # degrees
    INITIAL_LONGITUDE = -8.9387  # degrees
    INITIAL_ALTITUDE = 10  # meters above sea level

    # Randomization flag
    RANDOMIZE_START = False

    # Random position ranges (used when RANDOMIZE_START is True)
    MIN_LATITUDE = -90
    MAX_LATITUDE = 90
    MIN_LONGITUDE = -180
    MAX_LONGITUDE = 180
    MIN_ALTITUDE = -1000  # meters
    MAX_ALTITUDE = 4000  # meters

    # Position variation
    POSITION_VARIATION = 0.1  # degrees for lat/lon, km for altitude

    # Path generation parameters
    LAT_AMPLITUDE = 10
    LON_AMPLITUDE = 10
    ALT_AMPLITUDE = 2
    LAT_FREQUENCY = 0.1
    LON_FREQUENCY = 0.1
    ALT_FREQUENCY = 0.05

    # Random seed for reproducibility (None for random behaviour)
    RANDOM_SEED = None

    # Output parameters for CSV
    OUTPUT_DIR = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'sim_data')

    # Output enabling (default option is to output files)
    DISABLE_CSV_OUTPUT = False

    # Plotting enabling
    GENERATE_PLOTS = False

config = SimulationConfig()