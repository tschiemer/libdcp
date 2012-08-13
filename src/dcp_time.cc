/*
    Copyright (C) 2012 Carl Hetherington <cth@carlh.net>

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

#include <iostream>
#include <cmath>
#include "dcp_time.h"

using namespace std;
using namespace libdcp;

Time::Time (int frame, int frames_per_second)
{
	float sec_float = float (frame) / frames_per_second;
	ms = int (sec_float * 1000) % 1000;
	s = floor (sec_float);

	if (s > 60) {
		m = s / 60;
		s -= m * 60;
	}

	if (m > 60) {
		h = m / 60;
		m -= h * 60;
	}
}

bool
libdcp::operator== (Time const & a, Time const & b)
{
	return (a.h == b.h && a.m == b.m && a.s == b.s && a.ms == b.ms);
}

bool
libdcp::operator<= (Time const & a, Time const & b)
{
	if (a.h != b.h) {
		return a.h <= b.h;
	}

	if (a.m != b.m) {
		return a.m <= b.m;
	}

	if (a.s != b.s) {
		return a.s <= b.s;
	}

	if (a.ms != b.ms) {
		return a.ms <= b.ms;
	}

	return true;
}

ostream &
libdcp::operator<< (ostream& s, Time const & t)
{
	s << t.h << ":" << t.m << ":" << t.s << "." << t.ms;
	return s;
}