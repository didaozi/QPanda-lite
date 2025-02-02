#include "noisy_simulator.h"
#include "rng.h"

namespace qpandalite {

	void NoiseSimulatorImpl::depolarizing(size_t qn, double p)
	{
		double r = qpandalite::rand();
		if (r > p)
			return;
		if (r < p / 3)
			x(qn);
		else if (r < p / 3 * 2)
			y(qn);
		else
			z(qn);
	}

	void NoiseSimulatorImpl::damping(size_t qn, double p)
	{
		// // Damping noise involves the kraus operator that is not unitary,
		// // so the state might need to be re-normalized in both cases of Kraus operators.
		// double r = qpandalite::rand();

		// // Only apply damping if the qubit is in the |1> state
		// if (is_qubit_one(qn))
		// {
		//     if (r < p)
		//     {
		//         // With probability p, we set the qubit qn to the ground state |0>
		//         reset(qn);
		//     }
		//     else
		//     {
		//         // With probability (1-p), we scale the amplitude of the |1> state
		//         // E0 operation: the qubit remains in |1> but its amplitude is scaled by sqrt(1 - p)
		//         scale_amplitude(qn, std::sqrt(1 - p));
		//     }
		// }
		// // No need to apply E1 if the qubit is already in |0>, as it has no effect.

		// Kraus operators for amplitude damping
		double E0 = std::sqrt(1 - p);
		double E1 = std::sqrt(p);

		// Calculate the probabilities for the no-decay and decay cases
		double p0 = 0.0;
		double p1 = 0.0;
		for (size_t i = 0; i < pow2(total_qubit); ++i) {
			if ((i >> qn) & 1) { // qubit qn is in the |1> state
				p1 += abs_sqr(state[i] * E1); // Probability of decay

				p0 += (abs_sqr(state[i] * E0) + abs_sqr(state[i - pow2(qn)])); // Probability of no decay
			}
		}


		// Check if p0 + p1 equals 1
		if (std::abs(p0 + p1 - 1.0) > 1e-10) { // Use a small epsilon for floating-point comparison
			std::cout << p0 + p1 << " ";
			ThrowRuntimeError("Error: Probabilities after applying Kraus operators do not sum up to 1.");
		}

		// Generate a random number between 0 and 1
		double r = qpandalite::rand();

		// Apply the appropriate Kraus operator based on the random number
		if (r < p1) {
			// Apply E1 - decay to the ground state |0>
			for (size_t i = 0; i < pow2(total_qubit); ++i) {
				if ((i >> qn) & 1) {
					size_t zero_state_index = i - pow2(qn);
					state[zero_state_index] = state[i]; // Transfer amplitude
					state[i] = 0; // Qubit has decayed
				}
			}
		}
		else {
			// Apply E0 - scale the amplitude of the |1> state
			for (size_t i = 0; i < pow2(total_qubit); ++i) {
				if ((i >> qn) & 1) {
					state[i] *= E0; // Scale amplitude
				}
			}
		}
		// Re-normalize the state if necessary
		normalize_state_vector();

	}

	void NoiseSimulatorImpl::bitflip(size_t qn, double p)
	{
		double r = qpandalite::rand();
		if (r > p)
			return;
		else
			x(qn);
	}

	void NoiseSimulatorImpl::phaseflip(size_t qn, double p)
	{
		double r = qpandalite::rand();
		if (r > p)
			return;
		else
			z(qn);
	}

	void NoiseSimulatorImpl::twoqubit_depolarizing(size_t qn1, size_t qn2, double p)
	{
		double r = qpandalite::rand();
		if (r > p)
			return;
		int depol_case = int(15 * r / p) + 1;

		// II, IX, IY, IZ,  0~3
		// XI, XX, XY, XZ,  4~7
		// YI, YX, YY, YZ,  8~11
		// ZI, ZX, ZY, ZZ,  12~15

		switch (depol_case % 4)
		{
		case 0:
			// do nothing
			break;
		case 1:
			x(qn1);
			break;
		case 2:
			y(qn1);
			break;
		case 3:
			z(qn1);
			break;
		default:
			ThrowRuntimeError("?????");
		}

		switch (depol_case / 4)
		{
		case 0:
			// do nothing
			break;
		case 1:
			x(qn2);
			break;
		case 2:
			y(qn2);
			break;
		case 3:
			z(qn2);
			break;
		default:
			ThrowRuntimeError("?????");
		}
	}

	void NoiseSimulatorImpl::reset(size_t qn)
	{
		if (qn >= total_qubit)
		{
			auto errstr = fmt::format("Exceed total (total_qubit = {}, input = {})", total_qubit, qn);
			ThrowInvalidArgument(errstr);
		}

		for (size_t i = 0; i < pow2(total_qubit); ++i)
		{
			if ((i >> qn) & 1)  // If the qubit is in the |1> state
			{
				// We find the corresponding |0> state for this qubit
				size_t corresponding_zero_state = i & ~(pow2(qn));

				// Add the amplitude from the |1> state to the |0> state
				state[corresponding_zero_state] = std::norm(abs_sqr(state[i]));

				// Set the amplitude for the |1> state to zero
				state[i] = 0;

			}
		}
	}

	bool NoiseSimulatorImpl::is_qubit_one(size_t qn)
	{
		// You will need to check if the qubit qn is in the |1> state.
		// This requires knowledge about how the quantum state is represented in your simulator.
		// Assuming a state vector representation, you might do the following:

		bool qubit_is_one = false;
		for (size_t i = 0; i < pow2(total_qubit); ++i)
		{
			if (((i >> qn) & 1) && std::norm(state[i]) > 0)
			{
				qubit_is_one = true;
				break;
			}
		}
		return qubit_is_one;
	}

	void NoiseSimulatorImpl::scale_amplitude(size_t qn, double scale_factor)
	{
		// This function is to scale the amplitude of the |1> state of qn by scale_factor due to the damping noise.
		for (size_t i = 0; i < pow2(total_qubit); ++i)
		{
			if ((i >> qn) & 1)  // If the qubit is in the |1> state
			{
				state[i] *= scale_factor;
			}
		}
	}

	void NoiseSimulatorImpl::normalize_state_vector()
	{
		double norm = 0;

		// Sum the squares of the absolute values of the amplitudes
		for (size_t i = 0; i < pow2(total_qubit); ++i) {
			norm += abs_sqr(state[i]);
		}
		// Take the square root to get the normalization factor
		norm = std::sqrt(norm);
		// Divide each amplitude by the normalization factor
		for (size_t i = 0; i < pow2(total_qubit); ++i) {
			state[i] /= norm;
		}
	}

	NoisySimulator::NoisySimulator(size_t n_qubit,
		const std::map<std::string, double>& noise_description,
		const std::vector<std::array<double, 2>>& measurement_error)
		: nqubit(n_qubit),
		measurement_error_matrices(measurement_error)
	{
		// process the noise description and store the relevant noise types and their parameters.
		_load_noise(noise_description); // Load global noise
	}

	void NoisySimulator::_load_noise(std::map<std::string, double> noise_description)
	{
		auto it_depol = noise_description.find("depolarizing");
		if (it_depol != noise_description.end())
		{
			noise[NoiseType::Depolarizing] = it_depol->second;
		}
		auto it_damp = noise_description.find("damping");
		if (it_damp != noise_description.end())
		{
			noise[NoiseType::Damping] = it_damp->second;
		}

		auto it_bitflip = noise_description.find("bitflip");
		if (it_bitflip != noise_description.end())
		{
			noise[NoiseType::BitFlip] = it_bitflip->second;
		}

		auto it_phaseflip = noise_description.find("phaseflip");
		if (it_phaseflip != noise_description.end())
		{
			noise[NoiseType::PhaseFlip] = it_phaseflip->second;
		}

	}

	void NoisySimulator::load_opcode(const std::string& opstr,
		const std::vector<size_t>& qubits,
		const std::vector<double>& parameters,
		bool dagger,
		const std::vector<size_t>& global_controller)
	{
		SupportOperationType op = string_to_SupportOperationType(opstr);
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)op,
				{ qubits },
				{ parameters },
				dagger,
				global_controller)
		);
		insert_error({ qubits }, op);
	}

	void NoisySimulator::_insert_global_error(const std::vector<size_t>& qubits)
	{
		// Iterate through each noise type in the generic noise map
		for (const auto& noise_pair : noise) {
			NoiseType noise_type = noise_pair.first;
			double noise_probability = noise_pair.second;

			if (noise_type <= NoiseType::__NoiseTypeBegin ||
				noise_type >= NoiseType::__NoiseTypeEnd)
				ThrowRuntimeError("General noise type does not belong to the following: "
					"Depolarizing Damping BitFlip PhaseFlip.");

			opcodes.emplace_back(
				OpcodeType(
					static_cast<uint32_t>(noise_type),
					qubits,
					{ noise_probability },
					false,
					{})
			);
		}
		/*if (noise.empty()) return;

		auto it_end = noise.end();
		auto it_depol = noise.find(NoiseType::Depolarizing);
		auto it_damp = noise.find(NoiseType::Damping);
		auto it_bitflip = noise.find(NoiseType::BitFlip);
		auto it_phaseflip = noise.find(NoiseType::PhaseFlip);


		if (it_depol != it_end)
		{
			opcodes.emplace_back(
				OpcodeType(
				(uint32_t)NoiseType::Depolarizing,
				qubits,
				{ it_depol->second },
				false,
				{})
			);
		}

		if (it_damp != it_end)
		{
			opcodes.emplace_back(
				OpcodeType(
				(uint32_t)NoiseType::Damping,
				qubits,
				{ it_damp->second },
				false,
				{})
			);
		}

		if (it_bitflip != it_end)
		{
			opcodes.emplace_back(
				OpcodeType(
					(uint32_t)NoiseType::BitFlip,
				qubits,
				{ it_bitflip->second },
				false,
				{})
			);
		}

		if (it_phaseflip != it_end)
		{
			opcodes.emplace_back(
				OpcodeType(
				(uint32_t)NoiseType::PhaseFlip,
				qubits,
				{ it_phaseflip->second },
				false,
				{})
			);
		}*/
	}

	void NoisySimulator::_insert_generic_error(const std::vector<size_t>& qubits,
		const std::map<NoiseType, double>& generic_noise_map)
	{
		// Iterate through each noise type in the generic noise map
		for (const auto& noise_pair : generic_noise_map) {
			NoiseType noise_type = noise_pair.first;
			double noise_probability = noise_pair.second;

			/* The generic error will include more than NoiseType,
			   (noise_type can be unitary either)
			*/

			opcodes.emplace_back(
				OpcodeType(
					static_cast<uint32_t>(noise_type),
					qubits,
					{ noise_probability },
					false,
					{})
			);
		}
	}

	void NoisySimulator::insert_error(const std::vector<size_t>& qubits, SupportOperationType gate_type)
	{
		_insert_global_error(qubits);
	}
	
	//void NoisySimulator::id(size_t qn, bool is_dagger)
	//{
	//	id_cont(qn, {}, is_dagger);
	//}

	//void NoisySimulator::hadamard(size_t qn, bool is_dagger)
	//{
	//	hadamard_cont(qn, {}, is_dagger);
	//}

	//void NoisySimulator::u22(size_t qn, const u22_t& unitary, bool is_dagger)
	//{
	//	u22_cont(qn, unitary, {}, is_dagger);
	//}

	//void NoisySimulator::x(size_t qn, bool is_dagger)
	//{
	//	x_cont(qn, {}, is_dagger);
	//}

	//void NoisySimulator::y(size_t qn, bool is_dagger)
	//{
	//	y_cont(qn, {}, is_dagger);
	//}

	//void NoisySimulator::z(size_t qn, bool is_dagger)
	//{
	//	z_cont(qn, {}, is_dagger);
	//}

	//void NoisySimulator::sx(size_t qn, bool is_dagger)
	//{
	//	sx_cont(qn, {}, is_dagger);
	//}

	//void NoisySimulator::cz(size_t qn1, size_t qn2, bool is_dagger)
	//{
	//	cz_cont(qn1, qn2, {}, is_dagger);
	//}

	//void NoisySimulator::swap(size_t qn1, size_t qn2, bool is_dagger)
	//{
	//	swap_cont(qn1, qn2, {}, is_dagger);
	//}

	//void NoisySimulator::iswap(size_t qn1, size_t qn2, bool is_dagger)
	//{
	//	iswap_cont(qn1, qn2, {}, is_dagger);
	//}

	//void NoisySimulator::xy(size_t qn1, size_t qn2, double theta, bool is_dagger)
	//{
	//	xy_cont(qn1, qn2, theta, {}, is_dagger);
	//}

	//void NoisySimulator::cnot(size_t controller, size_t target, bool is_dagger)
	//{
	//	cnot_cont(controller, target, {}, is_dagger);
	//}

	//void NoisySimulator::rx(size_t qn, double angle, bool is_dagger)
	//{
	//	rx_cont(qn, angle, {}, is_dagger);
	//}

	//void NoisySimulator::ry(size_t qn, double angle, bool is_dagger)
	//{
	//	ry_cont(qn, angle, {}, is_dagger);
	//}

	//void NoisySimulator::rz(size_t qn, double angle, bool is_dagger)
	//{
	//	rz_cont(qn, angle, {}, is_dagger);
	//}

	//void NoisySimulator::rphi90(size_t qn, double phi, bool is_dagger)
	//{
	//	rphi90_cont(qn, phi, {}, is_dagger);
	//}

	//void NoisySimulator::rphi180(size_t qn, double phi, bool is_dagger)
	//{
	//	rphi180_cont(qn, phi, {}, is_dagger);
	//}

	//void NoisySimulator::rphi(size_t qn, double phi, double theta, bool is_dagger)
	//{
	//	rphi_cont(qn, phi, theta, {}, is_dagger);
 //   }
	//
	//void NoisySimulator::toffoli(size_t qn1, size_t qn2, size_t target, bool is_dagger)
	//{
	//	toffoli_cont(qn1, qn2, target, {}, is_dagger);
	//}

	//void NoisySimulator::cswap(size_t qn1, size_t qn2, size_t target, bool is_dagger)
	//{
	//	cswap_cont(qn1, qn2, target, {}, is_dagger);
	//}

	void NoisySimulator::id(size_t qn, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
			(uint32_t)SupportOperationType::IDENTITY,
			{ qn },
			{},
			is_dagger,
			global_controller)
		);
		insert_error({ qn }, SupportOperationType::IDENTITY);
	}

	void NoisySimulator::hadamard(size_t qn, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)SupportOperationType::HADAMARD,
				{ qn },
				{},
				is_dagger,
				global_controller)
		);
		insert_error({ qn }, SupportOperationType::HADAMARD);
	}

	void NoisySimulator::u22(size_t qn, const u22_t& unitary, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)SupportOperationType::U22,
				{ qn },
				{ unitary[0].real(), unitary[0].imag(),
				  unitary[1].real(), unitary[1].imag(),
				  unitary[2].real(), unitary[2].imag(),
				  unitary[3].real(), unitary[3].imag(), },
				is_dagger,
				global_controller)
		);
		insert_error({ qn }, SupportOperationType::U22);
	}

	void NoisySimulator::x(size_t qn, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)SupportOperationType::X,
				{ qn },
				{},
				is_dagger,
				global_controller)
		);

		insert_error({ qn }, SupportOperationType::X);
	}

	void NoisySimulator::y(size_t qn, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)SupportOperationType::Y,
				{ qn },
				{},
				is_dagger,
				global_controller)
		);
		insert_error({ qn }, SupportOperationType::Y);
	}

	void NoisySimulator::z(size_t qn, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)SupportOperationType::Z,
				{ qn },
				{},
				is_dagger,
				global_controller)
		);
		insert_error({ qn }, SupportOperationType::Z);
	}

	void NoisySimulator::sx(size_t qn, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)SupportOperationType::SX,
				{ qn },
				{},
				is_dagger,
				global_controller)
		);
		insert_error({ qn }, SupportOperationType::SX);
	}

	void NoisySimulator::cz(size_t qn1, size_t qn2, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)SupportOperationType::CZ,
				{ qn1, qn2 },
				{},
				is_dagger,
				global_controller)
		);
		insert_error({ qn1, qn2 }, SupportOperationType::CZ);
	}

	void NoisySimulator::swap(size_t qn1, size_t qn2, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)SupportOperationType::SWAP,
				{ qn1, qn2 },
				{},
				is_dagger,
				global_controller)
		);
		insert_error({ qn1, qn2 }, SupportOperationType::SWAP);
	}

	void NoisySimulator::xy(size_t qn1, size_t qn2, double theta, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)SupportOperationType::XY,
				{ qn1, qn2 },
				{ theta },
				is_dagger,
				global_controller)
		);
		insert_error({ qn1, qn2 }, SupportOperationType::XY);
	}

	void NoisySimulator::iswap(size_t qn1, size_t qn2, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)SupportOperationType::ISWAP,
				{ qn1, qn2 },
				{},
				is_dagger,
				global_controller)
		);
		insert_error({ qn1, qn2 }, SupportOperationType::ISWAP);
	}

	void NoisySimulator::cnot(size_t controller, size_t target, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)SupportOperationType::CNOT,
				{ controller, target },
				{},
				is_dagger,
				global_controller)
		);
		insert_error({ controller, target }, SupportOperationType::CNOT);
	}

	void NoisySimulator::rx(size_t qn, double theta, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)SupportOperationType::RX,
				{ qn },
				{ theta },
				is_dagger,
				global_controller)
		);
		insert_error({ qn }, SupportOperationType::RX);
	}

	void NoisySimulator::ry(size_t qn, double theta, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)SupportOperationType::RY,
				{ qn },
				{ theta },
				is_dagger,
				global_controller)
		);
		insert_error({ qn }, SupportOperationType::RY);
	}

	void NoisySimulator::rz(size_t qn, double theta, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)SupportOperationType::RZ,
				{ qn },
				{ theta },
				is_dagger,
				global_controller)
		);
		insert_error({ qn }, SupportOperationType::RZ);
	}

	void NoisySimulator::rphi90(size_t qn, double phi, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)SupportOperationType::RPHI90,
				{ qn },
				{ phi },
				is_dagger,
				global_controller)
		);
		insert_error({ qn }, SupportOperationType::RPHI90);
	}

	void NoisySimulator::rphi180(size_t qn, double phi, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)SupportOperationType::RPHI180,
				{ qn },
				{ phi },
				is_dagger,
				global_controller)
		);
		insert_error({ qn }, SupportOperationType::RPHI180);
	}

	void NoisySimulator::rphi(size_t qn, double phi, double theta, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)SupportOperationType::RPHI,
				{ qn },
				{ phi, theta },
				is_dagger,
				global_controller)
		);
		insert_error({ qn }, SupportOperationType::RPHI);
	}

	void NoisySimulator::toffoli(size_t qn1, size_t qn2, size_t target, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)SupportOperationType::TOFFOLI,
				{ qn1, qn2, target },
				{ },
				is_dagger,
				global_controller)
		);
		insert_error({ qn1, qn2, target }, SupportOperationType::TOFFOLI);
	}

	void NoisySimulator::cswap(size_t controller, size_t target1, size_t target2, const std::vector<size_t>& global_controller, bool is_dagger)
	{
		opcodes.emplace_back(
			OpcodeType(
				(uint32_t)SupportOperationType::CSWAP,
				{ controller, target1, target2 },
				{ },
				is_dagger,
				global_controller)
		);
		insert_error({ controller, target1, target2 }, SupportOperationType::CSWAP);
	}

	void NoisySimulator::measure(const std::vector<size_t> measure_qubits_)
	{
		measure_qubits = measure_qubits_;
		measure_map = preprocess_measure_list(measure_qubits, simulator.total_qubit);
	}

	void NoisySimulator::execute_once()
	{
		// It initializes the simulator object with a specified number of qubits, given by nqubit.
		simulator.init_n_qubit(nqubit);
		// std::cout << "once" << " ";
		// Currently, the iteration only supports Hadamard gates and depolarizing noise 2023/11/02
		for (const auto& opcode : opcodes)
		{
			// std::cout << opcode.op << std::endl;
			switch (opcode.op)
			{
			case (uint32_t)NoiseType::Depolarizing:
				for (const auto& q : opcode.qubits)
				{
					simulator.depolarizing(q, opcode.parameters[0]);
				}
				break;
			case (uint32_t)NoiseType::Damping:
				for (const auto& q : opcode.qubits)
				{
					simulator.damping(q, opcode.parameters[0]);
				}
				break;
			case (uint32_t)NoiseType::BitFlip:
				for (const auto& q : opcode.qubits)
				{
					simulator.bitflip(q, opcode.parameters[0]);
				}
				break;
			case (uint32_t)NoiseType::PhaseFlip:
				for (const auto& q : opcode.qubits)
				{
					simulator.phaseflip(q, opcode.parameters[0]);
				}
				break;
			case (uint32_t)NoiseType::TwoQubitDepolarizing:
				{
					if (opcode.qubits.size() != 2)
						ThrowInvalidArgument("The TwoQubitDepolarizing is not correctly applied.");
					simulator.twoqubit_depolarizing(opcode.qubits[0], opcode.qubits[1], opcode.parameters[0]);
				}
				break;
			case (uint32_t)SupportOperationType::HADAMARD:
				simulator.hadamard(opcode.qubits[0], opcode.global_controller, opcode.dagger);
				break;
			case (uint32_t)SupportOperationType::X:
				simulator.x(opcode.qubits[0], opcode.global_controller, opcode.dagger);
				break;
			case (uint32_t)SupportOperationType::SX:
				simulator.sx(opcode.qubits[0], opcode.global_controller, opcode.dagger);
				break;
			case (uint32_t)SupportOperationType::Y:
				simulator.y(opcode.qubits[0], opcode.global_controller, opcode.dagger);
				break;
			case (uint32_t)SupportOperationType::Z:
				simulator.z(opcode.qubits[0], opcode.global_controller, opcode.dagger);
				break;
			case (uint32_t)SupportOperationType::RX:
				simulator.rx(opcode.qubits[0], opcode.parameters[0], opcode.global_controller, opcode.dagger);
				break;
			case (uint32_t)SupportOperationType::RY:
				simulator.ry(opcode.qubits[0], opcode.parameters[0], opcode.global_controller, opcode.dagger);
				break;
			case (uint32_t)SupportOperationType::RZ:
				simulator.rz(opcode.qubits[0], opcode.parameters[0], opcode.global_controller, opcode.dagger);
				break;
			case (uint32_t)SupportOperationType::CZ:
				simulator.cz(opcode.qubits[0], opcode.qubits[1], opcode.global_controller, opcode.dagger);
				break;
			case (uint32_t)SupportOperationType::CNOT:
				simulator.cnot(opcode.qubits[0], opcode.qubits[1]);
				break;
			case (uint32_t)SupportOperationType::ISWAP:
				simulator.iswap(opcode.qubits[0], opcode.qubits[1], opcode.global_controller, opcode.dagger);
				break;
			case (uint32_t)SupportOperationType::XY:
				simulator.xy(opcode.qubits[0], opcode.qubits[1], opcode.parameters[0], opcode.global_controller, opcode.dagger);
				break;
			case (uint32_t)SupportOperationType::RPHI90:
				simulator.rphi90(opcode.qubits[0], opcode.parameters[0], opcode.global_controller, opcode.dagger);
				break;
			case (uint32_t)SupportOperationType::RPHI180:
				simulator.rphi180(opcode.qubits[0], opcode.parameters[0], opcode.global_controller, opcode.dagger);
				break;
			case (uint32_t)SupportOperationType::RPHI:
				simulator.rphi(opcode.qubits[0], opcode.parameters[0], opcode.parameters[1], opcode.global_controller, opcode.dagger);
				break;
			case (uint32_t)SupportOperationType::TOFFOLI:
				simulator.toffoli(opcode.qubits[0], opcode.qubits[1], opcode.qubits[2], opcode.global_controller, opcode.dagger);
				break;
			case (uint32_t)SupportOperationType::CSWAP:
				simulator.cswap(opcode.qubits[0], opcode.qubits[1], opcode.qubits[2], opcode.global_controller, opcode.dagger);
				break;
			default:
				ThrowRuntimeError(fmt::format("Failed to handle opcode = {}\nPlease check.", opcode.op));
			}
		}

		// This block could print the state after all operations(make sure the noiseless simulation is correct).
		// for (const auto &amp : simulator.state) {
		// 	    std::cout << amp.real() << " "<< amp.imag();
		// 	}
		// std::cout << std::endl;
	}

	std::pair<size_t, double> NoisySimulator::_get_state_prob(size_t i)
	{
		auto measure_map = preprocess_measure_list(measure_qubits, simulator.total_qubit);
		size_t meas_idx = get_state_with_qubit(i, measure_map);
		double prob = abs_sqr(simulator.state[i]);
		return { meas_idx, prob };
	}

	size_t NoisySimulator::get_measure_no_readout_error()
	{
		// Generate a random number between 0 and 1
		double r = qpandalite::rand();
		// std::cout << r << " ";
		for (size_t i = 0; i < pow2(simulator.total_qubit); ++i)
		{
			// std::cout << simulator.state[i] << " ";
			// Is this random number landing in this probability range?
			if (r < abs_sqr(simulator.state[i]))
			{
				// std::cout << r << " ";		
				return i;
			}
			// No, move to the next one, and subtract the current prob from the r
			else
			{
				r -= abs_sqr(simulator.state[i]);
				// std::cout << r << " ";
			}
		}
		ThrowRuntimeError("NoisySimulator::get_measure() internal fatal error!");
	}

	size_t NoisySimulator::get_measure()
	{
		size_t meas_result = get_measure_no_readout_error();
		if (measurement_error_matrices.size() == 0)
			return meas_result;

		if (measurement_error_matrices.size() != nqubit)
			ThrowRuntimeError("The size of the measurement_error_matrices does not match the qubit number!");

		for (size_t i = 0; i < nqubit; ++i)
		{
			double r = qpandalite::rand();
			if (meas_result & pow2(i))
			{
				// |1> case
				double meas_error = measurement_error_matrices[i][1];

				if (r < meas_error)
					meas_result -= pow2(i);
			}
			else
			{
				// |0> case
				double meas_error = measurement_error_matrices[i][0];

				if (r < meas_error)
					meas_result += pow2(i);
			}
		}
		return meas_result;
	}


	std::map<size_t, size_t> NoisySimulator::measure_shots(size_t shots)
	{
		/* The measure qubits are set empty - so every qubit is measured. */

		std::map<size_t, size_t> measured_result;
		for (size_t i = 0; i < shots; ++i)
		{
			// Execute the quantum circuit once and Measure the quantum state after executing the circuit.
			execute_once();

			size_t meas = get_measure();

			// std::cout << meas << " ";
			// Search the histogram to see if this state has been observed before.
			auto it = measured_result.find(meas);

			// If the state has been observed before, increment its count.
			if (it != measured_result.end())
			{
				it->second++;
			}
			// If this is the first time observing this state, add it to the histogram with a count of 1.
			else
			{
				measured_result.emplace(meas, 1);
			}
		}
		return measured_result;
	}

	std::map<size_t, size_t> NoisySimulator::measure_shots(const std::vector<size_t>& measure_list, size_t shots)
	{
		// Initialize an empty map to hold the frequency of each measured quantum state.

		measure_map = preprocess_measure_list(measure_list, nqubit);
		std::map<size_t, size_t> measured_result;

		for (size_t i = 0; i < shots; ++i)
		{
			// Execute the quantum circuit once and Measure the quantum state after executing the circuit.
			execute_once();

			size_t meas = get_measure();
			size_t meas_idx = get_state_with_qubit(meas, measure_map);

			// std::cout << meas << " ";
			// Search the histogram to see if this state has been observed before.
			auto it = measured_result.find(meas_idx);

			// If the state has been observed before, increment its count.
			if (it != measured_result.end())
			{
				it->second++;
			}
			// If this is the first time observing this state, add it to the histogram with a count of 1.
			else
			{
				measured_result.emplace(meas_idx, 1);
			}
		}
		return measured_result;
	}

	NoisySimulator_GateDependent::NoisySimulator_GateDependent(size_t n_qubit_,
		const std::map<std::string, double>& noise_description_,
		const std::map<std::string, std::map<std::string, double>>& gate_noise_description_,
		const std::vector<std::array<double, 2>>& measurement_error_
	) : NoisySimulator(n_qubit_, noise_description_, measurement_error_)
	{
		_load_gate_dependent_noise(gate_noise_description_);
	}

	void NoisySimulator_GateDependent::insert_error(const std::vector<size_t>& qubits, SupportOperationType gateType)
	{
		_insert_global_error(qubits);
		_insert_gate_dependent_error(qubits, gateType);
	}

	void NoisySimulator_GateDependent::_load_gate_dependent_noise(const std::map<std::string, std::map<std::string, double>>& gate_noise_description)
	{
		// Store the gate-dependent noise parameters
		// gate_dependent_noise = gate_noise_description;
		for (const auto& gate_pair : gate_noise_description)
		{
			SupportOperationType gateType = string_to_SupportOperationType(gate_pair.first);
			
			std::map<NoiseType, double> noiseProbabilities;
			for (const auto& noise_pair : gate_pair.second)
			{
				NoiseType noiseType = string_to_NoiseType(noise_pair.first);
				noiseProbabilities[noiseType] = noise_pair.second;
			}
			gate_dependent_noise[gateType] = noiseProbabilities;
		}
	}

	void NoisySimulator_GateDependent::_insert_gate_dependent_error(const std::vector<size_t>& qubits, SupportOperationType gateType)
	{
		if (gate_dependent_noise.empty()) return;

		// Find the noise configuration for the specific gate type
		auto it_gate_noise = gate_dependent_noise.find(gateType);
		if (it_gate_noise != gate_dependent_noise.end()) {
			// If gate-specific noise is defined, use insert_generic_error
			_insert_generic_error(qubits, it_gate_noise->second);
		}
	}

	NoisySimulator_GateSpecificError::NoisySimulator_GateSpecificError(size_t n_qubit,
		const std::map<std::string, double>& noise_description,
		const GateError1q_Description_t& gate_error1q_description,
		const GateError2q_Description_t& gate_error2q_description,
		const std::vector<std::array<double, 2>>& measurement_error)
		: NoisySimulator(n_qubit, noise_description, measurement_error)
	{
		_load_gate_error1q(gate_error1q_description);
		_load_gate_error2q(gate_error2q_description);
	}

	void NoisySimulator_GateSpecificError::_load_gate_error1q(const GateError1q_Description_t& gate_error_description)
	{
		for (auto& [gate_qubit, noise_type] : gate_error_description)
		{
			SupportOperationType gateType = string_to_SupportOperationType(gate_qubit.first);

			// DO NOT IMPLEMENT THIS!
			// Because we require this to emulate the local error in 2-qubit operation
			// 
			//if (gate_qubit_count(gateType) != 1)
			//	ThrowInvalidArgument("The specified gate is not 1q.");
			
			std::map<NoiseType, double> noiseProbabilities;
			for (const auto& noise_pair : noise_type)
			{
				NoiseType noiseType = string_to_NoiseType(noise_pair.first);
				noiseProbabilities[noiseType] = noise_pair.second;
			}
			gate_error1q[std::make_pair(gateType, gate_qubit.second)] = noiseProbabilities;
		}
	}

	void NoisySimulator_GateSpecificError::_load_gate_error2q(const GateError2q_Description_t& gate_error_description)
	{
		for (auto& [gate_qubit, noise_type] : gate_error_description)
		{
			SupportOperationType gateType = string_to_SupportOperationType(gate_qubit.first);

			// DO NOT IMPLEMENT THIS!
			// Because we require this to emulate the crosstalk
			// 
			//if (gate_qubit_count(gateType) != 2)
			//	ThrowInvalidArgument("The specified gate is not 2q.");
			
			if (gate_qubit_count(gateType) <= 2)
			{
				// to emulate the nonlocal error in 2q gate
				std::map<NoiseType, double> noiseProbabilities;
				for (const auto& noise_pair : noise_type)
				{
					NoiseType noiseType = string_to_NoiseType(noise_pair.first);
					noiseProbabilities[noiseType] = noise_pair.second;
				}
				gate_error2q[std::make_pair(gateType, gate_qubit.second)] = noiseProbabilities;
			}
			else
			{
				ThrowInvalidArgument("The specified gate is not 1q or 2q.");
			}

		}
	}

	void NoisySimulator_GateSpecificError::insert_error(const std::vector<size_t>& qubits, SupportOperationType gateType)
	{
		_insert_global_error(qubits);

		int gate_qcount = gate_qubit_count(gateType);
		switch (gate_qcount)
		{
		case 1:
			_insert_gate_error1q(gateType, qubits[0]);
			_insert_gate_error2q(gateType, qubits[0], -1);
			break;
		case 2:
			_insert_gate_error2q(gateType, qubits[0], qubits[1]);
			_insert_gate_error1q(gateType, qubits[0]);
			_insert_gate_error1q(gateType, qubits[1]);
			break;
		default:
			ThrowRuntimeError("[Fatal] Error type and gate qubit count "
				"is not correctly specified, which is not as expected.");
		}
	}

	void NoisySimulator_GateSpecificError::_insert_gate_error1q(SupportOperationType gateType, size_t qn)
	{
		if (gate_error1q.empty()) return;

		// Find the noise configuration for the specific gate type
		auto it_gate_noise = gate_error1q.find(std::make_pair(gateType, qn));
		if (it_gate_noise != gate_error1q.end()) {
			// If gate-specific noise is defined, use insert_generic_error
			_insert_generic_error({qn}, it_gate_noise->second);
		}
	}

	void NoisySimulator_GateSpecificError::_insert_gate_error2q(SupportOperationType gateType, size_t qn1, size_t qn2)
	{
		if (gate_error2q.empty()) return;

		if (qn2 == -1)
		{
			// 1q gate has 2q error
			for (const auto &gate_error : gate_error2q)
			{
				// gate_error = std::pair<...> (Gate Description, Error Description)
				// gate_error.first = std::pair<...> (GateType, Gate Specified Description)
				// gate_error.first.first= SupportOperationType
				// gate_error.first.second = std::pair<size_t, size_t> (q1, q2)
				// gate_error.second = Error Description
				if (gate_error.first.first != gateType)
					continue;

				if (gate_error.first.second.first == qn1)
				{
					size_t qn2_crosstalk_error = gate_error.first.second.second;
					_insert_generic_error({ qn1, qn2_crosstalk_error }, gate_error.second);
				}
			}
		}
		else {
			// 2q gate has 2q error
			// Find the noise configuration for the specific gate type
			auto it_gate_noise = gate_error2q.find(std::make_pair(gateType, std::make_pair(qn1, qn2)));
			if (it_gate_noise != gate_error2q.end()) {
				// If gate-specific noise is defined, use insert_generic_error
				_insert_generic_error({ qn1, qn2 }, it_gate_noise->second);
			}
		}
	}
}