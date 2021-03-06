// Xerus - A General Purpose Tensor Library
// Copyright (C) 2014-2016 Benjamin Huber and Sebastian Wolf. 
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
* @brief Implementation of the blas and lapack wrapper functions.
*/


#include <complex.h>
// fix for non standard-conform complex implementation
#undef I

// workaround for broken Lapack
#define lapack_complex_float    float _Complex
#define lapack_complex_double   double _Complex
extern "C"
{
	#include <cblas.h> 
}

#ifdef __has_include
	#if __has_include(<lapacke.h>)
		#include <lapacke.h>
	#elif __has_include(<lapacke/lapacke.h>)
		#include <lapacke/lapacke.h>
	#else
		#pragma error no lapacke found
	#endif
#else
	#include <lapacke.h>
#endif


#include <memory>
#include <xerus/misc/standard.h>
#include <xerus/misc/performanceAnalysis.h>
#include <xerus/misc/check.h>

#include <xerus/misc/stringUtilities.h>
#include <xerus/basic.h>

#include <xerus/blasLapackWrapper.h>
#include <xerus/misc/basicArraySupport.h>
#include <xerus/misc/math.h>



namespace xerus {
	namespace blasWrapper {
		// the following routines use a work array as temporary storage
		// to avoid the overhead of repeated reallocation for many small calls, every thread pre-allocates a small scratch-space
// 		const size_t DEFAULT_WORKSPACE_SIZE = 1024*1024;
// 		thread_local value_t defaultWorkspace[DEFAULT_WORKSPACE_SIZE]; // NOTE recheck compatibility with eigen (dolfin) when reinserting this!
		
		
		//----------------------------------------------- LEVEL I BLAS ----------------------------------------------------------
		
		double two_norm(const double* const _x, const size_t _n) {
			REQUIRE(_n <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
			
			PA_START;
			
			const double result = cblas_dnrm2(static_cast<int>(_n), _x, 1);
			
			PA_END("Dense BLAS", "Two Norm", misc::to_string(_n));
			
			return result;
		}
		
		double dot_product(const double* const _x, const size_t _n, const double* const _y) {
			REQUIRE(_n <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
			
			PA_START;
			
			const double result = cblas_ddot(static_cast<int>(_n), _x, 1, _y, 1);
			
			PA_END("Dense BLAS", "Dot Product", misc::to_string(_n)+"*"+misc::to_string(_n));
			
			return result;
		}
		
		
		//----------------------------------------------- LEVEL II BLAS ---------------------------------------------------------
		
		void matrix_vector_product(double* const _x, const size_t _m, const double _alpha, const double* const _A, const size_t _n, const bool _transposed, const double* const _y) {
			// Delegate call if appropriate
			if(_m == 1) { *_x = _alpha*dot_product(_A, _n, _y); return;}
			
			REQUIRE(_m <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
			REQUIRE(_n <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
			
			PA_START;
			if(!_transposed) {
				cblas_dgemv(CblasRowMajor, CblasNoTrans, static_cast<int>(_m), static_cast<int>(_n), _alpha, _A, static_cast<int>(_n), _y, 1, 0.0, _x, 1);
			} else {
				cblas_dgemv(CblasRowMajor, CblasTrans, static_cast<int>(_n), static_cast<int>(_m), _alpha, _A, static_cast<int>(_m) , _y, 1, 0.0, _x, 1);
			}
			
			PA_END("Dense BLAS", "Matrix Vector Product", misc::to_string(_m)+"x"+misc::to_string(_n)+" * "+misc::to_string(_n));
		}
		
		void dyadic_vector_product(double* _A, const size_t _m, const size_t _n, const double _alpha, const double*const  _x, const double* const _y) {
			REQUIRE(_m <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
			REQUIRE(_n <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
			
			PA_START;
			
			//Blas wants to add the product to A, but we don't.
			misc::set_zero(_A, _m*_n);
			
			cblas_dger(CblasRowMajor, static_cast<int>(_m), static_cast<int>(_n), _alpha, _x, 1, _y, 1, _A, static_cast<int>(_n));
			
			PA_END("Dense BLAS", "Dyadic Vector Product", misc::to_string(_m)+" o "+misc::to_string(_n));
		}
		
		
		//----------------------------------------------- LEVEL III BLAS --------------------------------------------------------
		/// Performs the Matrix-Matrix product c = a * b
		void matrix_matrix_product( double* const _C,
									const size_t _leftDim,
									const size_t _rightDim,
									const double _alpha,
									const double* const _A,
									const size_t _lda,
									const bool _transposeA,
									const size_t _middleDim,
									const double* const _B,
									const size_t _ldb,
									const bool _transposeB) {
			//Delegate call if appropriate
			if(_leftDim == 1) {
				matrix_vector_product(_C, _rightDim, _alpha, _B, _middleDim, !_transposeB, _A);
			} else if(_rightDim == 1) {
				matrix_vector_product(_C, _leftDim, _alpha, _A, _middleDim, _transposeA, _B);
			} else if(_middleDim == 1) { 
				dyadic_vector_product(_C, _leftDim, _rightDim, _alpha, _A, _B);
			} else {
			
				REQUIRE(_leftDim <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
				REQUIRE(_middleDim <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
				REQUIRE(_rightDim <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
				REQUIRE(_lda <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
				REQUIRE(_ldb <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
				
				PA_START;
				
				cblas_dgemm( CblasRowMajor,                             // Array storage format
						_transposeA ? CblasTrans : CblasNoTrans,        // LHS transposed?
						_transposeB ? CblasTrans : CblasNoTrans,        // RHS transposed?
						static_cast<int>(_leftDim),                     // Left dimension
						static_cast<int>(_rightDim),                    // Right dimension
						static_cast<int>(_middleDim),                   // Middle dimension
						_alpha,                                         // Factor to the Product
						_A,                                             // Pointer to LHS
						static_cast<int>(_lda),                         // LDA
						_B,                                             // Pointer to RHS
						static_cast<int>(_ldb),                         // LDB
						0.0,                                            // Factor of C (Zero if only the product is required)
						_C,                                             // Pointer to result
						static_cast<int>(_rightDim)                     // LDC
				);
				
				PA_END("Dense BLAS", "Matrix-Matrix-Multiplication", misc::to_string(_leftDim)+"x"+misc::to_string(_middleDim)+" * "+misc::to_string(_middleDim)+"x"+misc::to_string(_rightDim));
			}
		}
		
		
		
		//----------------------------------------------- LAPACK ----------------------------------------------------------------
		
		void svd( double* const _U, double* const _S, double* const _Vt, const double* const _A, const size_t _m, const size_t _n) {
			//Create copy of A
			const std::unique_ptr<double[]> tmpA(new double[_m*_n]);
			misc::copy(tmpA.get(), _A, _m*_n);
			
			svd_destructive(_U, _S, _Vt, tmpA.get(), _m, _n);
		}
		
		
		void svd_destructive( double* const _U, double* const _S, double* const _Vt, double* const _A, const size_t _m, const size_t _n) {
			REQUIRE(_m <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
			REQUIRE(_n <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
			
			PA_START;
			std::unique_ptr<double[]> tmpA(new double[_m*_n]);
			misc::copy(tmpA.get(), _A, _m*_n);
			
			int lapackAnswer = LAPACKE_dgesdd(LAPACK_ROW_MAJOR, 'S', static_cast<int>(_m), static_cast<int>(_n), _A, static_cast<int>(_n), _S, _U, static_cast<int>(std::min(_m, _n)), _Vt, static_cast<int>(_n));
			CHECK(lapackAnswer == 0, error, "Lapack failed to compute SVD. Answer is: " << lapackAnswer);
			CHECK(lapackAnswer == 0, error, "Call was: LAPACKE_dgesdd(LAPACK_ROW_MAJOR, 'S', " << static_cast<int>(_m) << ", " << static_cast<int>(_n) << ", " << _A << ", " << static_cast<int>(_n) <<", " 
			<< _S <<", " << _U << ", " << static_cast<int>(std::min(_m, _n)) << ", " << _Vt << ", " << static_cast<int>(_n) << ");");
			if(lapackAnswer != 0) {
				LOG(error, "A was: ");
				for(size_t i=0; i < _m; ++i) {
					for(size_t j=0; j < _n; ++j) {
						LOG(error, tmpA[i*_n+j]);
					}
				}
			}
			
			PA_END("Dense LAPACK", "Singular Value Decomposition", misc::to_string(_m)+"x"+misc::to_string(_n));
		}
		
		
		std::tuple<std::unique_ptr<double[]>, std::unique_ptr<double[]>, size_t> qc(const double* const _A, const size_t _m, const size_t _n) {
			const std::unique_ptr<double[]> tmpA(new double[_m*_n]);
			misc::copy(tmpA.get(), _A, _m*_n);
			
			return qc_destructive(tmpA.get(), _m, _n);
		}
		
		
		std::tuple<std::unique_ptr<double[]>, std::unique_ptr<double[]>, size_t> qc_destructive(double* const _A, const size_t _m, const size_t _n) {
			REQUIRE(_m <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
			REQUIRE(_n <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
			
			REQUIRE(_n > 0, "Dimension n must be larger than zero");
			REQUIRE(_m > 0, "Dimension m must be larger than zero");
			
			PA_START;
			
			// Maximal rank is used by Lapacke
			const size_t maxRank = std::min(_m, _n);
			
			// Tmp Array for Lapacke
			const std::unique_ptr<double[]> tau(new double[maxRank]);
			
			const std::unique_ptr<int[]> permutation(new int[_n]());
			misc::set_zero(permutation.get(), _n); // Lapack requires the entries to be zero.
			
			// Calculate QR factorisations with column pivoting
			IF_CHECK(int lapackAnswer = ) LAPACKE_dgeqp3(LAPACK_ROW_MAJOR, static_cast<int>(_m), static_cast<int>(_n), _A, static_cast<int>(_n), permutation.get(), tau.get());
			REQUIRE(lapackAnswer == 0, "Unable to perform QC factorisaton (dgeqp3). Lapacke says: " << lapackAnswer );
			
			
			// Determine the actual rank
			size_t rank;
			for (rank = 1; rank <= maxRank; ++rank) {
				if (rank == maxRank || std::abs(_A[rank+rank*_n]) < 16*std::numeric_limits<double>::epsilon()*_A[0]) {
					break;
				}
			}
			
			
			// Create the matrix C
			std::unique_ptr<double[]> C(new double[rank*_n]);
			misc::set_zero(C.get(), rank*_n); 
			
			// Copy the upper triangular Matrix C (rank x _n) into position
			for (size_t col = 0; col < _n; ++col) {
				const size_t targetCol = static_cast<size_t>(permutation[col]-1); // For Lapack numbers start at 1 (instead of 0).
				for(size_t row = 0; row < rank && row < col+1; ++row) {
					C[row*_n + targetCol] = _A[row*_n + col];
				}
			}
			
			
			// Create orthogonal matrix Q
			IF_CHECK(lapackAnswer = ) LAPACKE_dorgqr(LAPACK_ROW_MAJOR, static_cast<int>(_m), static_cast<int>(maxRank), static_cast<int>(maxRank), _A, static_cast<int>(_n), tau.get());
			CHECK(lapackAnswer == 0, error, "Unable to reconstruct Q from the QC factorisation. Lapacke says: " << lapackAnswer);
			
			// Copy the newly created Q into position
			std::unique_ptr<double[]> Q(new double[_m*rank]);
			if(rank == _n) {
				misc::copy(Q.get(), _A, _m*rank);
			} else {
				for(size_t row = 0; row < _m; ++row) {
					misc::copy(Q.get()+row*rank, _A+row*_n, rank);
				}
			}
			
			PA_END("Dense LAPACK", "QRP Factorisation", misc::to_string(_m)+"x"+misc::to_string(rank)+" * "+misc::to_string(rank)+"x"+misc::to_string(_n));
			
			return std::make_tuple(std::move(Q), std::move(C), rank);
		}
		
		
		std::tuple<std::unique_ptr<double[]>, std::unique_ptr<double[]>, size_t> cq(const double* const _A, const size_t _m, const size_t _n) {
			const std::unique_ptr<double[]> tmpA(new double[_m*_n]);
			misc::copy(tmpA.get(), _A, _m*_n);
			
			return cq_destructive(tmpA.get(), _m, _n);
		}
		
		
		// We use that in col-major we get At = Qt * Ct => A = C * Q, i.e. doing the calculation in col-major and switching Q and C give the desired result.
		std::tuple<std::unique_ptr<double[]>, std::unique_ptr<double[]>, size_t> cq_destructive(double* const _A, const size_t _m, const size_t _n) {
			REQUIRE(_n <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
			REQUIRE(_m <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
			
			REQUIRE(_m > 0, "Dimension n must be larger than zero");
			REQUIRE(_n > 0, "Dimension m must be larger than zero");
			
			PA_START;
			
			// Maximal rank is used by Lapacke
			const size_t maxRank = std::min(_n, _m);
			
			// Tmp Array for Lapacke
			const std::unique_ptr<double[]> tau(new double[maxRank]);
			
			const std::unique_ptr<int[]> permutation(new int[_m]());
			misc::set_zero(permutation.get(), _m); // Lapack requires the entries to be zero.
			
			// Calculate QR factorisations with column pivoting
			IF_CHECK(int lapackAnswer = ) LAPACKE_dgeqp3(LAPACK_COL_MAJOR, static_cast<int>(_n), static_cast<int>(_m), _A, static_cast<int>(_n), permutation.get(), tau.get());
			REQUIRE(lapackAnswer == 0, "Unable to perform QC factorisaton (dgeqp3). Lapacke says: " << lapackAnswer );
			
			
			// Determine the actual rank
			size_t rank;
			for (rank = 1; rank <= maxRank; ++rank) {
				if (rank == maxRank || std::abs(_A[rank+rank*_n]) < 16*std::numeric_limits<double>::epsilon()*_A[0]) {
					break;
				}
			}
			
			
			// Create the matrix C
			std::unique_ptr<double[]> C(new double[rank*_m]);
			misc::set_zero(C.get(), rank*_m); 
			
			// Copy the upper triangular Matrix C (rank x _m) into position
			for (size_t col = 0; col < _m; ++col) {
				const size_t targetCol = static_cast<size_t>(permutation[col]-1); // For Lapack numbers start at 1 (instead of 0).
				misc::copy(C.get()+targetCol*rank, _A+col*_n, std::min(rank, col+1));
			}
			
			
			// Create orthogonal matrix Q
			IF_CHECK(lapackAnswer = ) LAPACKE_dorgqr(LAPACK_COL_MAJOR, static_cast<int>(_n), static_cast<int>(maxRank), static_cast<int>(maxRank), _A, static_cast<int>(_n), tau.get());
			CHECK(lapackAnswer == 0, error, "Unable to reconstruct Q from the QC factorisation. Lapacke says: " << lapackAnswer);
			
			// Copy the newly created Q into position
			std::unique_ptr<double[]> Q(new double[_n*rank]);
			misc::copy(Q.get(), _A, _n*rank);
			
			PA_END("Dense LAPACK", "QRP Factorisation", misc::to_string(_n)+"x"+misc::to_string(rank)+" * "+misc::to_string(rank)+"x"+misc::to_string(_m));
			
			return std::make_tuple(std::move(C), std::move(Q), rank);
		}
		
		
		void qr(double* const _Q, double* const _R, const double* const _A, const size_t _m, const size_t _n) {
			// Create tmp copy of A since Lapack wants to destroy it
			const std::unique_ptr<double[]> tmpA(new double[_m*_n]);
			misc::copy(tmpA.get(), _A, _m*_n);
			
			qr_destructive(_Q, _R, tmpA.get(), _m, _n);
		}
		
		
		void inplace_qr(double* const _AtoQ, double* const _R, const size_t _m, const size_t _n) {
			qr_destructive(_AtoQ, _R, _AtoQ, _m, _n);
		}
		
		
		void qr_destructive( double* const _Q, double* const _R, double* const _A, const size_t _m, const size_t _n) {
			REQUIRE(_m <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
			REQUIRE(_n <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
			
			REQUIRE(_n > 0, "Dimension n must be larger than zero");
			REQUIRE(_m > 0, "Dimension m must be larger than zero");
			
			REQUIRE(_Q && _R && _A, "QR decomposition must not be called with null pointers: Q:" << _Q << " R: " << _R << " A: " << _A);
			REQUIRE(_A != _R, "_A and _R must be different, otherwise qr call will fail.");
			
			PA_START;
			
			// Maximal rank is used by Lapacke
			const size_t rank = std::min(_m, _n); 
			
			// Tmp Array for Lapacke
			const std::unique_ptr<double[]> tau(new double[rank]);
			
			// Calculate QR factorisations
//             LOG(Lapacke, "Call to dorgqr with parameters: " << LAPACK_ROW_MAJOR << ", " << static_cast<int>(_m)  << ", " << static_cast<int>(_n)  << ", " << _A << ", " << static_cast<int>(_n)  << ", " << tau.get());
			IF_CHECK( int lapackAnswer = ) LAPACKE_dgeqrf(LAPACK_ROW_MAJOR, static_cast<int>(_m), static_cast<int>(_n), _A, static_cast<int>(_n), tau.get());
			CHECK(lapackAnswer == 0, error, "Unable to perform QR factorisaton. Lapacke says: " << lapackAnswer );
			
			// Copy the upper triangular Matrix R (rank x _n) into position
			for(size_t row =0; row < rank; ++row) {
				misc::set_zero(_R+row*_n, row); // Set starting zeros
				misc::copy(_R+row*_n+row, _A+row*_n+row, _n-row); // Copy upper triangular part from lapack result.
			}
			
			// Create orthogonal matrix Q (in tmpA)
	//         LOG(Lapacke, "Call to dorgqr with parameters: " << LAPACK_ROW_MAJOR << ", " << static_cast<int>(_m)  << ", " << static_cast<int>(rank)  << ", " << static_cast<int>(rank) << ", " << _A << ", " << static_cast<int>(_n)  << ", " << tau.get());
			IF_CHECK( lapackAnswer = ) LAPACKE_dorgqr(LAPACK_ROW_MAJOR, static_cast<int>(_m), static_cast<int>(rank), static_cast<int>(rank), _A, static_cast<int>(_n), tau.get());
			CHECK(lapackAnswer == 0, error, "Unable to reconstruct Q from the QR factorisation. Lapacke says: " << lapackAnswer);
			
			// Copy Q (_m x rank) into position
			if(_A != _Q) {
				if(_n == rank) {
					misc::copy(_Q, _A, _m*_n);
				} else {
					for(size_t row = 0; row < _m; ++row) {
						misc::copy(_Q+row*rank, _A+row*_n, rank);
					}
				}
			} else if(_n != rank) { // Note extra treatmeant to avoid memcpy overlap
				for(size_t row = 1; row < _m; ++row) {
					misc::copy_inplace(_Q+row*rank, _A+row*_n, rank);
				}
			}
			
			PA_END("Dense LAPACK", "QR Factorisation", misc::to_string(_m)+"x"+misc::to_string(_n));
		}
		
		
		void rq( double* const _R, double* const _Q, const double* const _A, const size_t _m, const size_t _n) {
			// Create tmp copy of A since Lapack wants to destroy it
			const std::unique_ptr<double[]> tmpA(new double[_m*_n]);
			misc::copy(tmpA.get(), _A, _m*_n);
			
			rq_destructive(_R, _Q, tmpA.get(), _m, _n);
		}
		
		
		void inplace_rq( double* const _R, double* const _AtoQ, const size_t _m, const size_t _n) {
			rq_destructive(_R, _AtoQ, _AtoQ, _m, _n);
		}
		
		
		void rq_destructive( double* const _R, double* const _Q, double* const _A, const size_t _m, const size_t _n) {
			REQUIRE(_m <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
			REQUIRE(_n <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
			
			REQUIRE(_n > 0, "Dimension n must be larger than zero");
			REQUIRE(_m > 0, "Dimension m must be larger than zero");
			
			REQUIRE(_Q && _R && _A, "QR decomposition must not be called with null pointers: R " << _R << " Q: " << _Q << " A: " << _A);
			REQUIRE(_A != _R, "_A and _R must be different, otherwise qr call will fail.");
			
			PA_START;
			
			// Maximal rank is used by Lapacke
			const size_t rank = std::min(_m, _n); 
			
			// Tmp Array for Lapacke
			const std::unique_ptr<double[]> tau(new double[rank]);
			
			IF_CHECK( int lapackAnswer = ) LAPACKE_dgerqf(LAPACK_ROW_MAJOR, static_cast<int>(_m), static_cast<int>(_n), _A, static_cast<int>(_n), tau.get());
			CHECK(lapackAnswer == 0, error, "Unable to perform QR factorisaton. Lapacke says: " << lapackAnswer );
			
			
			// Copy the upper triangular Matrix R (_m x rank) into position.
			size_t row = 0;
			for( ; row < _m - rank; ++row) {
				misc::copy(_R+row*rank, _A+row*_n+_n-rank, rank);
			}
			for(size_t skip = 0; row < _m; ++row, ++skip) {
				misc::set_zero(_R+row*rank, skip); // Set starting zeros
				misc::copy(_R+row*rank+skip, _A+row*_n+_n-rank+skip, rank-skip); // Copy upper triangular part from lapack result.
			}
			
			// Create orthogonal matrix Q (in _A). Lapacke expects to get the last rank rows of A...
			IF_CHECK( lapackAnswer = ) LAPACKE_dorgrq(LAPACK_ROW_MAJOR, static_cast<int>(rank), static_cast<int>(_n), static_cast<int>(rank), _A+(_m-rank)*_n, static_cast<int>(_n), tau.get()); 
			CHECK(lapackAnswer == 0, error, "Unable to reconstruct Q from the RQ factorisation. Lapacke says: " << lapackAnswer);

			
			//Copy Q (rank x _n) into position
			if(_A != _Q) {
				misc::copy(_Q, _A+(_m-rank)*_n, rank*_n);
			}
			
			PA_END("Dense LAPACK", "RQ Factorisation", misc::to_string(_m)+"x"+misc::to_string(_n));
		}
		
		
	/*  TODO we need test cases for these  
		/// Solves Ax = b for x
		void solve( double* const _x, const double* const _A, const size_t _n, const double* const _b) {
			const std::unique_ptr<double[]> tmpA(new double[_n*_n]);
			array_copy(tmpA.get(), _A, _n*_n);
			array_copy(_x, _b, _n);
			
			solve_destructive(_x, tmpA.get(), _n);
		}
		
		/// Solves Ax = b for x
		void solve_destructive( double* const _bToX, double* const _A, const size_t _n) {
			REQUIRE(_n <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
			
			START_TIME;
			
			std::unique_ptr<int[]> pivot(new int[_n]);
			
			int lapackAnswer = LAPACKE_dgesv(
				LAPACK_ROW_MAJOR,
				static_cast<int>(_n),       // Dimensions of A (nxn)
				1,              // Number of b's, here always one
				_A,             // The input matrix A, will be destroyed
				static_cast<int>(_n),       // LDA
				pivot.get(),    // Output of the pivot ordering
				_bToX,          // Input the vector b, output the vector x
				1 );
			CHECK(lapackAnswer == 0, error, "Unable to solves Ax = b. Lapacke says: " << lapackAnswer);
			
			ADD_CALL("Solve ", to_string(_n)+"x"+to_string(_n));
		}*/
		
	
		void solve_least_squares( double* const _x, const double* const _A, const size_t _m, const size_t _n, const double* const _b){
			const std::unique_ptr<double[]> tmpA(new double[_m*_n]);
			misc::copy(tmpA.get(), _A, _m*_n);
			
			const std::unique_ptr<double[]> tmpB(new double[_n]);
			misc::copy(tmpB.get(), _b, _n);
			
			solve_least_squares_destructive(_x, tmpA.get(), _m, _n, tmpB.get());
		}
		
		
		void solve_least_squares_destructive( double* const _x, double* const _A, const size_t _m, const size_t _n, double* const _b){
			REQUIRE(_m <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
			REQUIRE(_n <= static_cast<size_t>(std::numeric_limits<int>::max()), "Dimension to large for BLAS/Lapack");
			
			PA_START;
			
			std::unique_ptr<int[]> pivot(new int[_n]);
			misc::set_zero(pivot.get(), _n);
			int rank;
			
			double* bOrX;
			if(_m >= _n) {
				bOrX = _x;
				misc::copy(bOrX, _b, _n);
			} else {
				bOrX = _b;
			}
			
			IF_CHECK( int lapackAnswer = ) LAPACKE_dgelsy(
				LAPACK_ROW_MAJOR, 
				static_cast<int>(_m),   // Left dimension of A
				static_cast<int>(_n),   // Right dimension of A
				1,          // Number of b's, here always one
				_A,         // Matrix A
				static_cast<int>(_n),   // LDA
				bOrX,       // On input b, on output x
				1,          // LDB, here always one
				pivot.get(),// Pivot, entries must be zero to allow pivoting
				xerus::EPSILON,      // Used to determine the accuracy of the Lapacke call. Basically all singular values smaller than RCOND*s[0] are ignored. (s[0] is the largest signular value)
				&rank);     // Outputs the rank of A
			CHECK(lapackAnswer == 0, error, "Unable to solves min ||Ax - b||_2 for x. Lapacke says: " << lapackAnswer);
			
			if(_m < _n) {
				misc::copy(_x, bOrX, _m);
			}
			
			PA_END("Dense LAPACK", "Solve Least Squares", misc::to_string(_m)+"x"+misc::to_string(_n));
		}
		
	}

}
