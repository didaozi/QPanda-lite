import numpy as np
import math

from qpandalite.simulator import StatevectorSimulator, NoisySimulator
from qpandalite.qasm import OpenQASM2_LineParser
from qpandalite.circuit_builder import Circuit
from qpandalite.simulator import seed

import time
current_time_seed = int(time.time())

# Use the current time as a seed
seed(current_time_seed)

# Define the noise descriptions
noise_description = {
    "depolarizing": 0.01
}

# Define the gate noise descriptions
gate_noise_description = {
    "X": {"depolarizing": 0.03},
    "HADAMARD": {"depolarizing": 0.02},
    "ISWAP": {"depolarizing": 0.02}
}

# Define the measurement errors
readout_error = [(0.01, 0.01), (0.02, 0.02)]

# Create an instance of the NoisySimulator
simulator = NoisySimulator(2,noise_description, gate_noise_description)

# noise_desc = simulator.noise
# gate_noise_desc = simulator.gate_dependent_noise

# print(noise_desc)
# print(gate_noise_desc)


# simulator.cnot_cont()
# Number of measurement shots
shots = 1024

# # Measure the state multiple times
measurement_results = simulator.measure_shots(measure_qubit=[(0,0)], shots=shots)

print(measurement_results)