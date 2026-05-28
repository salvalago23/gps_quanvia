import logging
import sys
import os
from functools import wraps
import traceback
from datetime import datetime

def setup_logger(name):
    # Create logs directory in the project root if it doesn't exist
    logs_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'logs')
    if not os.path.exists(logs_dir):
        os.makedirs(logs_dir)

    # Create a directory for today's date if it doesn't exist
    today = datetime.now().strftime('%d-%m-%Y')
    today_dir = os.path.join(logs_dir, today)
    if not os.path.exists(today_dir):
        os.makedirs(today_dir)

    # Create a log file with timestamp
    timestamp = datetime.now().strftime('%d%m%Y_%H%M%S')
    log_file = os.path.join(today_dir, f'sim_{timestamp}.log')

    # Set up logging
    formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s')
    
    file_handler = logging.FileHandler(log_file)        
    file_handler.setFormatter(formatter)

    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setFormatter(formatter)

    logger = logging.getLogger(name)
    logger.setLevel(logging.INFO)
    logger.addHandler(file_handler)
    logger.addHandler(console_handler)

    return logger

# Create a logger instance
logger = setup_logger('simulation_logger')

class SimulationError(Exception):
    """Custom exception for simulation errors"""
    pass

def error_handler(func):
    @wraps(func)
    def wrapper(*args, **kwargs):
        try:
            return func(*args, **kwargs)
        except SimulationError as e:
            logger.error(f"Simulation Error in {func.__name__}: {str(e)}")
            raise
        except Exception as e:
            logger.error(f"Unexpected error in {func.__name__}: {str(e)}")
            logger.error(traceback.format_exc())
            raise SimulationError(f"An unexpected error occurred in {func.__name__}. Check logs for details.")
    return wrapper