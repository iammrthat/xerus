#=================================================================================================
# Compiler Options
#=================================================================================================
# Xerus can be compiled either with G++ or the clang++ frontend of the LLVM.
# Uncomment the next line to use Clang++ instead of G++.
# USECLANG = TRUE

#=================================================================================================
# Optimization
#=================================================================================================
# We suggest the use of one of the following optimization levels. The first uses basically no 
# optimization and is primarly intended for debugging purposes. The second (recommended) level 
# activates more or less all optimization options that conform with the ISO C++ Standard. 
# The last level activates all optimazations available, including non-ISO C++ conform optimization 
# and optimazations that may result in a loss of numerical precicsion, use at your own risk.
#Optimization options
# LOW_OPTIMIZATION = TRUE		# Activates -O0
# HIGH_OPTIMIZATION = TRUE		# Activates -O3 -march=native and some others
# DANGEROUS_OPTIMIZATION = TRUE		# Activates everything of HIGH_OPTIMIZATION plus basically everything that is said to improve performance including several potentially unsafe optimizations

# Additionally Link Time Optimization support can be build into the library by uncommenting the following line.
# LTO = TRUE

#=================================================================================================
# Debug and Logging   
#=================================================================================================
# The Xerus library performs a number of runtime checks to ensure a valid input to all routines.
# While not recommended these runtime checks can be completly disabled by commenting the following
# line. This slighlty improves the performance.
DEBUG += -D CHECK_

# Disable the use of exceptions by the xerus library
# NO_XERUS_EXCEPTIONS = TRUE


# You can add all kind of debuging options. In the following are some examples
# DEBUG = -D_GLIBCXX_DEBUG		# Enables bounds checking for stl containers
 DEBUG += -g				# Adds debug symbols


# Sanitization 
# DEBUG += -fsanitize=undefined		    # gcc 4.9 only
# DEBUG += -fsanitize=memory		    # clang only
# DEBUG += -fsanitize=address		    # find out of bounds access


# Set the logging level. Xerus has a logging system to provide runtime information. Here you can adjust the logging level used by the library
LOGGING += -D DEBUG_			
# LOGGING += -D INFO_
# LOGGING += -D WARNING_
# LOGGING += -D CRITICAL_

# Per default the logs are printed to cerr. Uncomment the following line to print the log messages to the file error.log instead.
# LOGGING += -D LOGFILE_

# Uncomment the following line to save the last LOgs in a circular buffer (without printing them) to allow detailed reports in case of errors.
# Note that this can significatly slow down the library.
#  LOGGING += -D LOG_BUFFER_

# Add time measurments for all blas and Lapack calls
# LOGGING += -D BLAS_ANALYSIS		# Enable BLAS/LAPACK analysis



#=================================================================================================
# External libraries
#=================================================================================================
# Xerus depends on several external libraries, namely blas, cblas, lapack, lapacke and suiteSparse,
# all of which are available through common GNU/Linux packaging systems. If you want to run the
# unit tests of Xerus you have to provide the corresponding libraries here (otherwise only when
# linking a program using Xerus). 

# Uncomment or add the suiteable blas and lapack libraries 
 BLAS_LIBRARIES = -lopenblas -lgfortran			# Openblas, serial
# BLAS_LIBRARIES = -lopenblasp -lgfortran	   		# Openblas, parallel
# BLAS_LIBRARIES = /usr/lib64/atlas/libsatlas.so -lgfortran	# Atlas
# BLAS_LIBRARIES = /usr/lib64/atlas/libf77blas.a /usr/lib64/atlas/libcblas.a /usr/lib64/atlas/libatlas.a -lgfortran	# Custom


 LAPACK_LIBRARIES = -llapacke -llapack		    # Standard Lapack + Lapacke libraries
# LAPACK_LIBRARIES = ../lib/lapack/liblapacke.a ../lib/lapack/liblapack.a       # Custom

# Change the following line if you want to use a self compiled verion of the SuiteSparse library
SUITESPARSE = -lcxsparse

# If needed set additional include paths
# ADDITIONAL_INCLUDE = -I ../include/
