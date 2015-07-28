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
 * @brief Header file for the different measurment classes.
 */

#pragma once

#include "basic.h"

namespace xerus {
	/** 
	* @brief Class used to represent a single point measurments.
	*/
    class SinglePointMeasurment {
		size_t position;
		std::vector<size_t> positions;
		value_t value;
		
		SinglePointMeasurment(const size_t _position, const std::vector<size_t> _positions, const value_t _value);
		
		bool operator< (const SinglePointMeasurment & _other);
	};
		
	/** 
	* @brief Class used to represent a set of single point measurments.
	*/
    class SinglePointMeasurmentSet {
		///@brief Dimensions of the tensor the measurments apply to.
		std::vector<size_t> dimensions;
		
		///@brief The measurments contained in the set.
		std::vector<SinglePointMeasurment> measurments;
		
		SinglePointMeasurmentSet(const std::vector<size_t>& _dimensions);
		
		void add(const size_t _position, const value_t _value);
		
		void add(const std::vector<size_t> _positions, const value_t _value);
	};
}
