// Xerus - A General Purpose Tensor Library
// Copyright (C) 2014-2015 Benjamin Huber and Sebastian Wolf. 
// 
// Xerus is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License,
// or (at your option) any later version.
// 
// Xerus is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Affero General Public License for more details.
// 
// You should have received a copy of the GNU Affero General Public License
// along with Xerus. If not, see <http://www.gnu.org/licenses/>.
//
// For further information on Xerus visit https://libXerus.org 
// or contact us at contact@libXerus.org.

/**
 * @file
 * @brief Header file for the ADF algorithm and its variants.
 */

#pragma once

#include "../ttNetwork.h"
#include "../performanceData.h"
#include "../measurments.h"

namespace xerus {
	/**
	 * @brief Wrapper class for all ADF variants.
	 * @details By creating a new object of this class and modifying the member variables, the behaviour of the solver can be modified.
	 */
    class ADFVariant {
		protected:
			
		template<class MeasurmentSet> 
		class InternalSolver {
			/*
			 * General notation for the ADF:
			 * The vector of the measured values is denoted as b
			 * The measurment operator is denoted as A. It holds by definition A(x) = b (for noiseless measurments).
			 * The Operator constructed from the x by removing the component at corePosition is denoted as E.
			 */
			
		protected:
			///@brief Indices for all internal functions.
			const Index r1, r2, i1;
			
			///@brief Reference to the current solution (external ownership)
			TTTensor& x;
			
			///@brief Degree of the solution.
			const size_t degree;
			
			///@brief Maximally allowed ranks.
			const std::vector<size_t> maxRanks;
			
			///@brief Reference to the measurment set (external ownership)
			const MeasurmentSet& measurments;
			
			///@brief Number of measurments (i.e. measurments.size())
			const size_t numMeasurments;
			
			///@brief The two norm of the measured values
			const value_t normMeasuredValues;
			
			///@brief Maximal allowed number of iterations (one iteration = one sweep)
			const size_t maxIterations; 
			
			///@brief The target residual norm at which the algorithm shall stop.
			const double targetResidualNorm;
			
			///@brief Minimal relative decrease of the residual norm ( (oldRes-newRes)/oldRes ) until either the ranks are increased (if allowed) or the algorithm stops.
			const double minimalResidualNormDecrese; 
			
			///@brief The current iteration.
			size_t iteration;
			
			///@brief Current residual norm. Updated at the beginning of each iteration.
			double residualNorm;

			///@brief The residual norm of the last iteration.
			double lastResidualNorm;
			
			///@brief The current residual, saved as vector (instead of a order one tensor).
			std::vector<value_t> residual;
			
			///@brief The current projected Gradient component. That is E(A^T(Ax-b))
			FullTensor projectedGradientComponent;
			
			///@brief Ownership holder for a (degree+2)*numMeasurments array of FullTensor pointers. (Not used directly)
			std::unique_ptr<FullTensor*[]> forwardStackMem;
			
			/** @brief Array [numMeasurments][degree]. For positions smaller than the current corePosition and for each measurment, this array contains the pre-computed
			* contraction of the first _ component tensors and the first _ components of the measurment operator. These tensors are deduplicated in the sense that for each unqiue
			* part of the position only one tensor is actually stored, which is why the is an array of pointers. The Tensors at the current corePosition are used as 
			* scatch space. For convinience the underlying array (forwardStackMem) is larger, wherefore also the positions -1 and degree are allow, all poining to a {1} tensor
			* containing 1 as only entry. Note that position degree-1 must not be used.
			**/
			FullTensor* const * const forwardStack;
			
			/// @brief Ownership holder for the unqiue FullTensors referenced in forwardStack.
			std::unique_ptr<FullTensor[]> forwardStackSaveSlots;
			
			/// @brief Vector containing for each corePosition a vector of the smallest ids of each group of unique forwardStack entries.
			std::vector<std::vector<size_t>> forwardUpdates;
			
			
			///@brief Ownership holder for a (degree+2)*numMeasurments array of FullTensor pointers. (Not used directly)
			std::unique_ptr<FullTensor*[]> backwardStackMem;
			
			/** @brief Array [numMeasurments][degree]. For positions larger than the current corePosition and for each measurment, this array contains the pre-computed
			* contraction of the last _ component tensors and the last _ components of the measurment operator. These tensors are deduplicated in the sense that for each unqiue
			* part of the position only one tensor is actually stored, which is why the is an array of pointers. The Tensors at the current corePosition are used as 
			* scratch space. For convinience the underlying array (forwardStackMem) is larger, wherefore also the positions -1 and degree are allow, all poining to a {1} tensor
			* containing 1 as only entry. Note that position zero must not be used.
			**/
			FullTensor* const * const backwardStack;

			/// @brief Ownership holder for the unqiue FullTensors referenced in backwardStack.
			std::unique_ptr<FullTensor[]> backwardStackSaveSlots;
			
			/// @brief Vector containing for each corePosition a vector of the smallest ids of each group of unique backwardStack entries.
			std::vector<std::vector<size_t>> backwardUpdates;
			
			
			///@brief: Reference to the performanceData object (external ownership)
			PerformanceData& perfData;
			
			///@brief calculates the two-norm of the measured values.
			static double calculate_norm_of_measured_values(const MeasurmentSet& _measurments);
			
			///@brief Constructes either the forward or backward stack. That is, it determines the groups of partially equale measurments. Therby stetting (forward/backward)- Updates, StackMem and SaveSlot.
			void construct_stacks(std::unique_ptr< xerus::FullTensor[] >& _stackSaveSlot, std::vector< std::vector< size_t > >& _updates, const std::unique_ptr<FullTensor*[]>& _stackMem, const bool _forward);
			
			///@brief Resizes the unqiue stack tensors to correspond to the current ranks of x.
			void resize_stack_tensors();
			
			///@brief Returns a vector of tensors containing the slices of @a _component where the second dimension is fixed.
			std::vector<FullTensor> get_fixed_components(const Tensor& _component);
			
			///@brief For each measurment sets the forwardStack at the given _corePosition to the contraction between the forwardStack at the previous corePosition (i.e. -1)
			/// and the given component contracted with the component of the measurment operator. For _corePosition == corePosition and _currentComponent == x.components(corePosition)
			/// this really updates the stack, otherwise it uses the stack as scratch space.
			void update_forward_stack(const size_t _corePosition, const Tensor& _currentComponent);
			
			///@brief For each measurment sets the backwardStack at the given _corePosition to the contraction between the backwardStack at the previous corePosition (i.e. +1)
			/// and the given component contracted with the component of the measurment operator. For _corePosition == corePosition and _currentComponent == x.components(corePosition)
			/// this really updates the stack, otherwise it uses the stack as scratch space.
			void update_backward_stack(const size_t _corePosition, const Tensor& _currentComponent);
			
			///@brief (Re-)Calculates the current residual, i.e. Ax-b.
			void calculate_residual( const size_t _corePosition );
			
			///@brief Calculates one internal step of calculate_projected_gradient. In particular the dyadic product of the leftStack, the rightStack and the position vector.
			template<class PositionType>
			void perform_dyadic_product(const size_t _localLeftRank, const size_t _localRightRank, const value_t* const _leftPtr,  const value_t* const _rightPtr,  value_t* const _deltaPtr, const value_t _residual, const PositionType& _position, value_t* const _scratchSpace );
	
			
			///@brief: Calculates the component at _corePosition of the projected gradient from the residual, i.e. E(A^T(b-Ax)).
			void calculate_projected_gradient(const size_t _corePosition);
			
			/**
			* @brief: Calculates ||P_n (A(E(A^T(b-Ax)))))|| = ||P_n (A(E(A^T(residual)))))|| =  ||P_n (A(E(gradient)))|| for each n, 
			* where P_n sets all entries equals zero except where the index at _corePosition is equals n. In case of RankOneMeasurments,
			* the calculation is not slicewise (only n=0 is set).
			*/
			std::vector<value_t> calculate_slicewise_norm_A_projGrad( const size_t _corePosition);
			
			///@brief Updates the current solution x. For SinglePointMeasurments the is done for each slice speratly, for RankOneMeasurments there is only one combined update.
			void update_x(const std::vector<value_t>& _normAProjGrad, const size_t _corePosition);
			
			///@brief Basically the complete algorithm, trying to reconstruct x using its current ranks.
			void solve_with_current_ranks();
			
		public:
			///@brief Tries to solve the reconstruction problem with the current settings.
			double solve();
			
			InternalSolver(TTTensor& _x, 
							const std::vector<size_t>& _maxRanks, 
							const MeasurmentSet& _measurments, 
							const size_t _maxIteration, 
							const double _targetResidualNorm, 
							const double _minimalResidualNormDecrese, 
							PerformanceData& _perfData ) : 
				x(_x),
				degree(_x.degree()),
				maxRanks(TTTensor::reduce_to_maximal_ranks(_maxRanks, _x.dimensions)),
				
				measurments(_measurments),
				numMeasurments(_measurments.size()),
				normMeasuredValues(calculate_norm_of_measured_values(_measurments)),
				
				maxIterations(_maxIteration),
				targetResidualNorm(_targetResidualNorm),
				minimalResidualNormDecrese(_minimalResidualNormDecrese),
				
				iteration(0),
				residualNorm(std::numeric_limits<double>::max()), 
				lastResidualNorm(std::numeric_limits<double>::max()),
				
				residual(numMeasurments),
				
				forwardStackMem(new FullTensor*[numMeasurments*(degree+2)]),
				forwardStack(forwardStackMem.get()+numMeasurments),
				forwardUpdates(degree),
					
				backwardStackMem(new FullTensor*[numMeasurments*(degree+2)]),
				backwardStack(backwardStackMem.get()+numMeasurments),
				backwardUpdates(degree),
				
				perfData(_perfData) 
				{
					REQUIRE(_x.is_valid_tt(), "_x must be a valid TT-Tensor.");
					REQUIRE(numMeasurments > 0, "Need at very least one measurment.");
					REQUIRE(measurments.degree() == degree, "Measurment degree must coincide with x degree.");
				}
				
		};
		
	public:
        size_t maxIterations; ///< Maximum number of sweeps to perform. Set to 0 for infinite.
        double targetResidualNorm; ///< Target residual. The algorithm will stop upon reaching a residual smaller than this value.
        double minimalResidualNormDecrese; // The minimal relative decrese of the residual per step  ( i.e. (lastResidual-residual)/lastResidual ). If the avg. of the last three steps is smaller than this value, the algorithm stops.
        bool printProgress; ///< informs the user about the current progress via std::cout (one continuously overwritten line)
        
		/// fully defining constructor. alternatively ALSVariants can be created by copying a predefined variant and modifying it
        ADFVariant(const size_t _maxIteration, const double _targetResidual, const double _minimalResidualDecrese, const bool _printProgress) 
                : maxIterations(_maxIteration), targetResidualNorm(_targetResidual), minimalResidualNormDecrese(_minimalResidualDecrese), printProgress(_printProgress) { }
        
        /**
		* @brief Tries to reconstruct the (low rank) tensor _x from the given measurments. 
		* @param[in,out] _x On input: an initial guess of the solution, also defining the ranks. On output: The reconstruction found by the algorithm.
		* @param _measurments the available measurments, can be either a SinglePointMeasurmentSet or RankOneMeasurmentSet.
		* @param _perfData optinal performanceData object to be used.
		* @returns the residual @f$|P_\Omega(x-b)|_2@f$ of the final @a _x.
		*/
		template<class MeasurmentSet>
		double operator()(TTTensor& _x, const MeasurmentSet& _measurments, PerformanceData& _perfData) const {
			InternalSolver<MeasurmentSet> solver(_x, _x.ranks(), _measurments, maxIterations, targetResidualNorm, minimalResidualNormDecrese, _perfData);
			return solver.solve();
		}
		
		/**
		* @brief Tries to reconstruct the (low rank) tensor _x from the given measurments. 
		* @param[in,out] _x On input: an initial guess of the solution, may be of smaller rank. On output: The reconstruction found by the algorithm.
		* @param _measurments the available measurments, can be either a SinglePointMeasurmentSet or RankOneMeasurmentSet.
		* @param _maxRanks the maximal ranks the algorithm may use to decrease the resdiual.
		* @param _perfData optinal performanceData object to be used.
		* @returns the residual @f$|P_\Omega(x-b)|_2@f$ of the final @a _x.
		*/
		template<class MeasurmentSet>
		double operator()(TTTensor& _x, const MeasurmentSet& _measurments, const std::vector<size_t>& _maxRanks, PerformanceData& _perfData) const {
			InternalSolver<MeasurmentSet> solver(_x, _maxRanks, _measurments, maxIterations, targetResidualNorm, minimalResidualNormDecrese, _perfData);
			return solver.solve();
		}
	};
	
	/// @brief Default variant of the ADF algorithm
    extern const ADFVariant ADF;
}

