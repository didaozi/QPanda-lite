import qpandalite.simulator as qsim

sim = qsim.Simulator()

sim.init_n_qubit(6)
print(len(sim.state))
print(sim.state)

sim.hadamard(0)
sim.cnot(0,1)
sim.cnot(1,3)
sim.cnot(0,2)
sim.cnot(1,4)
sim.cnot(3,5)
print(sim.state)