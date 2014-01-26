/*
    Copyright (C) 2014 Carl Hetherington <cth@carlh.net>

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

/** @file  src/asset.cc
 *  @brief Asset class.
 */

#include "asset.h"
#include "util.h"
#include <libxml++/libxml++.h>
#include <boost/lexical_cast.hpp>

using std::string;
using boost::lexical_cast;
using boost::function;
using namespace dcp;

/** Create an Asset with a randomly-generated ID */
Asset::Asset ()
{

}

Asset::Asset (boost::filesystem::path file)
	: _file (file)
{

}

/** Create an Asset with a specified ID.
 *  @param id ID to use.
 */
Asset::Asset (string id)
	: Object (id)
{

}

void
Asset::write_to_pkl (xmlpp::Node* node) const
{
	xmlpp::Node* asset = node->add_child ("Asset");
	asset->add_child("Id")->add_child_text ("urn:uuid:" + _id);
	asset->add_child("AnnotationText")->add_child_text (_id);
	asset->add_child("Hash")->add_child_text (hash ());
	asset->add_child("Size")->add_child_text (lexical_cast<string> (boost::filesystem::file_size (_file)));
	asset->add_child("Type")->add_child_text (pkl_type ());
}

void
Asset::write_to_assetmap (xmlpp::Node* node) const
{
	xmlpp::Node* asset = node->add_child ("Asset");
	asset->add_child("Id")->add_child_text ("urn:uuid:" + _id);
	xmlpp::Node* chunk_list = asset->add_child ("ChunkList");
	xmlpp::Node* chunk = chunk_list->add_child ("Chunk");
	chunk->add_child("Path")->add_child_text (_file.string ());
	chunk->add_child("VolumeIndex")->add_child_text ("1");
	chunk->add_child("Offset")->add_child_text ("0");
	chunk->add_child("Length")->add_child_text (lexical_cast<string> (boost::filesystem::file_size (_file)));
}

string
Asset::hash () const
{
	if (!_hash.empty ()) {
		_hash = make_digest (_file, 0);
	}

	return _hash;
}

bool
Asset::equals (boost::shared_ptr<const Asset> other, EqualityOptions, function<void (NoteType, string)> note) const
{
	if (_hash != other->_hash) {
		note (ERROR, "Asset hashes differ");
		return false;
	}

	return true;
}
