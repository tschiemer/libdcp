/*
    Copyright (C) 2014-2015 Carl Hetherington <cth@carlh.net>

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

#include "image.h"
#include "rgb_xyz.h"
#include "xyz_image.h"
#include "colour_conversion.h"
#include <boost/test/unit_test.hpp>
#include <boost/bind.hpp>

using std::max;
using std::list;
using std::string;
using boost::shared_ptr;
using boost::optional;

class SimpleImage : public dcp::Image
{
public:
	SimpleImage (dcp::Size size)
		: Image (size)
	{
		/* 48bpp */
		_stride[0] = _size.width * 6;
		_data[0] = new uint8_t[size.height * stride()[0]];
	}

	~SimpleImage ()
	{
		delete[] _data[0];
	}

	uint8_t * const * data () const {
		return _data;
	}

	int const * stride () const {
		return _stride;
	}

private:
	uint8_t* _data[1];
	int _stride[1];
};

/** Convert a test image from sRGB to XYZ and check that the transforms are right */
BOOST_AUTO_TEST_CASE (rgb_xyz_test)
{
	unsigned int seed = 0;
	dcp::Size const size (640, 480);

	shared_ptr<const dcp::Image> rgb (new SimpleImage (size));
	for (int y = 0; y < size.height; ++y) {
		uint16_t* p = reinterpret_cast<uint16_t*> (rgb->data()[0] + y * rgb->stride()[0]);
		for (int x = 0; x < size.width; ++x) {
			/* Write a 12-bit random number for each component */
			for (int c = 0; c < 3; ++c) {
				*p = (rand_r (&seed) & 0xfff) << 4;
				++p;
			}
		}
	}

	shared_ptr<dcp::XYZImage> xyz = dcp::rgb_to_xyz (rgb, dcp::ColourConversion::srgb_to_xyz ());

	for (int y = 0; y < size.height; ++y) {
		uint16_t* p = reinterpret_cast<uint16_t*> (rgb->data()[0] + y * rgb->stride()[0]);
		for (int x = 0; x < size.width; ++x) {

			double cr = *p++ / 65535.0;
			double cg = *p++ / 65535.0;
			double cb = *p++ / 65535.0;

			/* Input gamma */

			if (cr < 0.04045) {
				cr /= 12.92;
			} else {
				cr = pow ((cr + 0.055) / 1.055, 2.4);
			}

			if (cg < 0.04045) {
				cg /= 12.92;
			} else {
				cg = pow ((cg + 0.055) / 1.055, 2.4);
			}

			if (cb < 0.04045) {
				cb /= 12.92;
			} else {
				cb = pow ((cb + 0.055) / 1.055, 2.4);
			}

			/* Matrix */

			double cx = cr * 0.4124564 + cg * 0.3575761 + cb * 0.1804375;
			double cy = cr * 0.2126729 + cg * 0.7151522 + cb * 0.0721750;
			double cz = cr * 0.0193339 + cg * 0.1191920 + cb * 0.9503041;

			/* Compand */

			cx *= 48 / 52.37;
			cy *= 48 / 52.37;
			cz *= 48 / 52.37;

			/* Output gamma */

			cx = pow (cx, 1 / 2.6);
			cy = pow (cy, 1 / 2.6);
			cz = pow (cz, 1 / 2.6);

			BOOST_REQUIRE_CLOSE (cx * 4095, xyz->data(0)[y * size.width + x], 1);
			BOOST_REQUIRE_CLOSE (cy * 4095, xyz->data(1)[y * size.width + x], 1);
			BOOST_REQUIRE_CLOSE (cz * 4095, xyz->data(2)[y * size.width + x], 1);
		}
	}
}

static list<string> notes;

static void
note_handler (dcp::NoteType n, string s)
{
	BOOST_REQUIRE_EQUAL (n, dcp::DCP_NOTE);
	notes.push_back (s);
}

/** Check that xyz_to_rgb clamps XYZ values correctly */
BOOST_AUTO_TEST_CASE (xyz_rgb_range_test)
{
	shared_ptr<dcp::XYZImage> xyz (new dcp::XYZImage (dcp::Size (2, 2)));
	
	xyz->data(0)[0] = -4;
	xyz->data(0)[1] = 6901;
	xyz->data(0)[2] = 0;
	xyz->data(0)[3] = 4095;
	xyz->data(1)[0] = -4;
	xyz->data(1)[1] = 6901;
	xyz->data(1)[2] = 0;
	xyz->data(1)[3] = 4095;
	xyz->data(2)[0] = -4;
	xyz->data(2)[1] = 6901;
	xyz->data(2)[2] = 0;
	xyz->data(2)[3] = 4095;

	shared_ptr<SimpleImage> image (new SimpleImage (dcp::Size (2, 2)));

	notes.clear ();
	dcp::xyz_to_rgb (xyz, dcp::ColourConversion::xyz_to_srgb (), image, boost::optional<dcp::NoteHandler> (boost::bind (&note_handler, _1, _2)));

	/* The 6 out-of-range samples should have been noted */
	BOOST_REQUIRE_EQUAL (notes.size(), 6);
	list<string>::const_iterator i = notes.begin ();
	BOOST_REQUIRE_EQUAL (*i++, "XYZ value -4 out of range");
	BOOST_REQUIRE_EQUAL (*i++, "XYZ value -4 out of range");
	BOOST_REQUIRE_EQUAL (*i++, "XYZ value -4 out of range");
	BOOST_REQUIRE_EQUAL (*i++, "XYZ value 6901 out of range");
	BOOST_REQUIRE_EQUAL (*i++, "XYZ value 6901 out of range");
	BOOST_REQUIRE_EQUAL (*i++, "XYZ value 6901 out of range");

	/* And those samples should have been clamped, so check that they give the same result
	   as inputs at the extremes (0 and 4095).
	*/

	uint16_t* buffer = reinterpret_cast<uint16_t*> (image->data()[0]);
	BOOST_REQUIRE_EQUAL (buffer[0 * 3 + 0], buffer[2 * 3 + 1]);
	BOOST_REQUIRE_EQUAL (buffer[0 * 3 + 1], buffer[2 * 3 + 1]);
	BOOST_REQUIRE_EQUAL (buffer[0 * 3 + 2], buffer[2 * 3 + 2]);

	BOOST_REQUIRE_EQUAL (buffer[1 * 3 + 0], buffer[3 * 3 + 0]);
	BOOST_REQUIRE_EQUAL (buffer[1 * 3 + 1], buffer[3 * 3 + 1]);
	BOOST_REQUIRE_EQUAL (buffer[1 * 3 + 2], buffer[3 * 3 + 2]);
}
