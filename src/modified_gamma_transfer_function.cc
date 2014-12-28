/*
    Copyright (C) 2012-2014 Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "modified_gamma_transfer_function.h"
#include <cmath>

using std::pow;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using namespace dcp;

ModifiedGammaTransferFunction::ModifiedGammaTransferFunction (float power, float threshold, float A, float B)
	: _power (power)
	, _threshold (threshold)
	, _A (A)
	, _B (B)
{

}

float *
ModifiedGammaTransferFunction::make_lut (int bit_depth) const
{
	int const bit_length = pow (2, bit_depth);
	float* lut = new float[int(std::pow(2.0f, bit_depth))];
	for (int i = 0; i < bit_length; ++i) {
		float const p = static_cast<float> (i) / (bit_length - 1);
		if (p > _threshold) {
			lut[i] = pow ((p + _A) / (1 + _A), _power);
		} else {
			lut[i] = p / _B;
		}
	}

	return lut;
}

bool
ModifiedGammaTransferFunction::about_equal (shared_ptr<const TransferFunction> other, float epsilon) const
{
	shared_ptr<const ModifiedGammaTransferFunction> o = dynamic_pointer_cast<const ModifiedGammaTransferFunction> (other);
	if (!o) {
		return false;
	}

	return (
		fabs (_power - o->_power) < epsilon &&
	        fabs (_threshold - o->_threshold) < epsilon &&
		fabs (_A - o->_A) < epsilon &&
		fabs (_B - o->_B) < epsilon
		);
}