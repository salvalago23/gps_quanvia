import sys
import os
import argparse

sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from src.simulator import run_simulations
from src.config import config
from src.utils import error_handler, SimulationError, logger

@error_handler
def validate_num_simulations(num_simulations):
    if num_simulations <= 0:
        error_message = f"num_simulations ({num_simulations}) must be greater than 0"
        raise SimulationError(error_message)

@error_handler
def validate_durations(min_duration, max_duration):
    if max_duration < 1 or min_duration < 0:
        error_message = f"min_duration ({min_duration}) must be greater or equal to 0 and max_duration ({max_duration}) must be at least 1"
        raise SimulationError(error_message)
    elif max_duration < min_duration:
        error_message = f"max_duration ({max_duration}) must be greater than (or equal to) min_duration ({min_duration})"
        raise SimulationError(error_message)
    
@error_handler
def main(args=None):
    parser = argparse.ArgumentParser(description='Run simulations')
    parser.add_argument('--num_simulations', type=int, default=config.NUM_SIMULATIONS, help='Number of simulations to run')
    parser.add_argument('--min_duration', type=float, default=config.MIN_DURATION, help='Minimum duration of each simulation in seconds')
    parser.add_argument('--max_duration', type=float, default=config.MAX_DURATION, help='Maximum duration of each simulation in seconds')
    parser.add_argument('--output_dir', type=str, default=config.OUTPUT_DIR, help='Directory to save simulation data')
    parser.add_argument('--random_seed', type=int, default=config.RANDOM_SEED, help='Random seed for reproducibility (set to None for random behavior)')
    parser.add_argument('--randomize_start', action='store_true', help='Randomize the starting GPS position and altitude')
    parser.add_argument('--disable_csv', action='store_true', help='Disable CSV output')
    parser.add_argument('--generate_plots', action='store_true', help='Generate plots for the simulations')

    # Parse command-line arguments
    args = parser.parse_args(args)

    # Update config with command-line arguments
    config.NUM_SIMULATIONS = args.num_simulations
    config.MIN_DURATION = args.min_duration
    config.MAX_DURATION = args.max_duration
    config.OUTPUT_DIR = args.output_dir
    config.RANDOM_SEED = args.random_seed
    config.GENERATE_PLOTS = args.generate_plots
    config.RANDOMIZE_START = args.randomize_start
    config.DISABLE_CSV_OUTPUT = args.disable_csv

    try :
        validate_num_simulations(config.NUM_SIMULATIONS)
        validate_durations(config.MIN_DURATION, config.MAX_DURATION)
    except SimulationError:
        sys.exit(1)  # Exit the program with an error code

    logger.info("Running simulations with the following settings:")
    logger.info(f"Number of simulations: {config.NUM_SIMULATIONS}")
    logger.info(f"Duration range: {config.MIN_DURATION} - {config.MAX_DURATION} seconds")
    logger.info(f"Output directory: {config.OUTPUT_DIR}")
    if config.RANDOM_SEED is not None:
        logger.info(f"Random seed: {config.RANDOM_SEED}")
    else:
        logger.info("Random seed: None (random behavior will be used)")
    logger.info(f"Randomize starting position: {config.RANDOMIZE_START}")
    logger.info(f"CSV output: {not (config.DISABLE_CSV_OUTPUT)}")
    logger.info(f"Generate plots: {config.GENERATE_PLOTS}\n")

    run_simulations()

if __name__ == "__main__":
    main()