/*
    Copyright (C) 2012-2013 Carl Hetherington <cth@carlh.net>

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

#include "AS_DCP.h"
#include "stereo_picture_mxf.h"
#include "stereo_picture_frame.h"
#include "exceptions.h"
#include "stereo_picture_mxf_writer.h"

using std::string;
using std::pair;
using std::make_pair;
using boost::shared_ptr;
using boost::dynamic_pointer_cast;
using namespace dcp;

bool
StereoPictureMXF::equals (shared_ptr<const Content> other, EqualityOptions opt, boost::function<void (NoteType, string)> note) const
{
	if (!MXF::equals (other, opt, note)) {
		return false;
	}

	ASDCP::JP2K::MXFSReader reader_A;
	Kumu::Result_t r = reader_A.OpenRead (file().string().c_str());
	if (ASDCP_FAILURE (r)) {
		boost::throw_exception (MXFFileError ("could not open MXF file for reading", file().string(), r));
	}
	
	ASDCP::JP2K::MXFSReader reader_B;
	r = reader_B.OpenRead (other->file().string().c_str());
	if (ASDCP_FAILURE (r)) {
		boost::throw_exception (MXFFileError ("could not open MXF file for reading", file().string(), r));
	}
	
	ASDCP::JP2K::PictureDescriptor desc_A;
	if (ASDCP_FAILURE (reader_A.FillPictureDescriptor (desc_A))) {
		boost::throw_exception (DCPReadError ("could not read video MXF information"));
	}
	ASDCP::JP2K::PictureDescriptor desc_B;
	if (ASDCP_FAILURE (reader_B.FillPictureDescriptor (desc_B))) {
		boost::throw_exception (DCPReadError ("could not read video MXF information"));
	}
	
	if (!descriptor_equals (desc_A, desc_B, note)) {
		return false;
	}
	
	shared_ptr<const StereoPictureMXF> other_picture = dynamic_pointer_cast<const StereoPictureMXF> (other);
	assert (other_picture);

	for (int i = 0; i < _intrinsic_duration; ++i) {
		shared_ptr<const StereoPictureFrame> frame_A = get_frame (i);
		shared_ptr<const StereoPictureFrame> frame_B = other_picture->get_frame (i);
		
		if (!frame_buffer_equals (
			    i, opt, note,
			    frame_A->left_j2k_data(), frame_A->left_j2k_size(),
			    frame_B->left_j2k_data(), frame_B->left_j2k_size()
			    )) {
			return false;
		}
		
		if (!frame_buffer_equals (
			    i, opt, note,
			    frame_A->right_j2k_data(), frame_A->right_j2k_size(),
			    frame_B->right_j2k_data(), frame_B->right_j2k_size()
			    )) {
			return false;
		}
	}

	return true;
}

StereoPictureMXF::StereoPictureMXF (boost::filesystem::path file)
	: PictureMXF (file)
{
	ASDCP::JP2K::MXFSReader reader;
	Kumu::Result_t r = reader.OpenRead (file.string().c_str());
	if (ASDCP_FAILURE (r)) {
		boost::throw_exception (MXFFileError ("could not open MXF file for reading", file.string(), r));
	}
	
	ASDCP::JP2K::PictureDescriptor desc;
	if (ASDCP_FAILURE (reader.FillPictureDescriptor (desc))) {
		boost::throw_exception (DCPReadError ("could not read video MXF information"));
	}

	_size.width = desc.StoredWidth;
	_size.height = desc.StoredHeight;
}

shared_ptr<const StereoPictureFrame>
StereoPictureMXF::get_frame (int n) const
{
	return shared_ptr<const StereoPictureFrame> (new StereoPictureFrame (file().string(), n));
}

shared_ptr<PictureMXFWriter>
StereoPictureMXF::start_write (boost::filesystem::path file, bool overwrite)
{
	return shared_ptr<StereoPictureMXFWriter> (new StereoPictureMXFWriter (this, file, overwrite));
}

string
StereoPictureMXF::cpl_node_name () const
{
	return "msp-cpl:MainStereoscopicPicture";
}

pair<string, string>
StereoPictureMXF::cpl_node_attribute () const
{
	if (_interop) {
		return make_pair ("xmlns:msp-cpl", "http://www.digicine.com/schemas/437-Y/2007/Main-Stereo-Picture-CPL");
	} else {
		return make_pair ("xmlns:msp-cpl", "http://www.smpte-ra.org/schemas/429-10/2008/Main-Stereo-Picture-CPL");
	}

	return make_pair ("", "");
}

int
StereoPictureMXF::edit_rate_factor () const
{
	return 2;
}
