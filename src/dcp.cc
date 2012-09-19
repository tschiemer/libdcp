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

/** @file  src/dcp.cc
 *  @brief A class to create a DCP.
 */

#include <sstream>
#include <fstream>
#include <iomanip>
#include <cassert>
#include <iostream>
#include <boost/filesystem.hpp>
#include <libxml++/libxml++.h>
#include "dcp.h"
#include "asset.h"
#include "sound_asset.h"
#include "picture_asset.h"
#include "subtitle_asset.h"
#include "util.h"
#include "metadata.h"
#include "exceptions.h"
#include "cpl_file.h"
#include "pkl_file.h"
#include "asset_map.h"
#include "reel.h"

using namespace std;
using namespace boost;
using namespace libdcp;

DCP::DCP (string directory)
	: _directory (directory)
{
	filesystem::create_directories (directory);
}

void
DCP::write_xml () const
{
	for (list<shared_ptr<const CPL> >::const_iterator i = _cpls.begin(); i != _cpls.end(); ++i) {
		(*i)->write_xml ();
	}

	string pkl_uuid = make_uuid ();
	string pkl_path = write_pkl (pkl_uuid);
	
	write_volindex ();
	write_assetmap (pkl_uuid, filesystem::file_size (pkl_path));
}

std::string
DCP::write_pkl (string pkl_uuid) const
{
	assert (!_cpls.empty ());
	
	filesystem::path p;
	p /= _directory;
	stringstream s;
	s << pkl_uuid << "_pkl.xml";
	p /= s.str();
	ofstream pkl (p.string().c_str());

	pkl << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	    << "<PackingList xmlns=\"http://www.smpte-ra.org/schemas/429-8/2007/PKL\">\n"
	    << "  <Id>urn:uuid:" << pkl_uuid << "</Id>\n"
		/* XXX: this is a bit of a hack */
	    << "  <AnnotationText>" << _cpls.front()->name() << "</AnnotationText>\n"
	    << "  <IssueDate>" << Metadata::instance()->issue_date << "</IssueDate>\n"
	    << "  <Issuer>" << Metadata::instance()->issuer << "</Issuer>\n"
	    << "  <Creator>" << Metadata::instance()->creator << "</Creator>\n"
	    << "  <AssetList>\n";

	list<shared_ptr<const Asset> > a = assets ();
	for (list<shared_ptr<const Asset> >::const_iterator i = a.begin(); i != a.end(); ++i) {
		(*i)->write_to_pkl (pkl);
	}

	for (list<shared_ptr<const CPL> >::const_iterator i = _cpls.begin(); i != _cpls.end(); ++i) {
		(*i)->write_to_pkl (pkl);
	}

	pkl << "  </AssetList>\n"
	    << "</PackingList>\n";

	return p.string ();
}

void
DCP::write_volindex () const
{
	filesystem::path p;
	p /= _directory;
	p /= "VOLINDEX.xml";
	ofstream vi (p.string().c_str());

	vi << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	   << "<VolumeIndex xmlns=\"http://www.smpte-ra.org/schemas/429-9/2007/AM\">\n"
	   << "  <Index>1</Index>\n"
	   << "</VolumeIndex>\n";
}

void
DCP::write_assetmap (string pkl_uuid, int pkl_length) const
{
	filesystem::path p;
	p /= _directory;
	p /= "ASSETMAP.xml";
	ofstream am (p.string().c_str());

	am << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	   << "<AssetMap xmlns=\"http://www.smpte-ra.org/schemas/429-9/2007/AM\">\n"
	   << "  <Id>urn:uuid:" << make_uuid() << "</Id>\n"
	   << "  <Creator>" << Metadata::instance()->creator << "</Creator>\n"
	   << "  <VolumeCount>1</VolumeCount>\n"
	   << "  <IssueDate>" << Metadata::instance()->issue_date << "</IssueDate>\n"
	   << "  <Issuer>" << Metadata::instance()->issuer << "</Issuer>\n"
	   << "  <AssetList>\n";

	am << "    <Asset>\n"
	   << "      <Id>urn:uuid:" << pkl_uuid << "</Id>\n"
	   << "      <PackingList>true</PackingList>\n"
	   << "      <ChunkList>\n"
	   << "        <Chunk>\n"
	   << "          <Path>" << pkl_uuid << "_pkl.xml</Path>\n"
	   << "          <VolumeIndex>1</VolumeIndex>\n"
	   << "          <Offset>0</Offset>\n"
	   << "          <Length>" << pkl_length << "</Length>\n"
	   << "        </Chunk>\n"
	   << "      </ChunkList>\n"
	   << "    </Asset>\n";
	
	for (list<shared_ptr<const CPL> >::const_iterator i = _cpls.begin(); i != _cpls.end(); ++i) {
		(*i)->write_to_assetmap (am);
	}

	list<shared_ptr<const Asset> > a = assets ();
	for (list<shared_ptr<const Asset> >::const_iterator i = a.begin(); i != a.end(); ++i) {
		(*i)->write_to_assetmap (am);
	}

	am << "  </AssetList>\n"
	   << "</AssetMap>\n";
}


void
DCP::read (bool require_mxfs)
{
	Files files;

	shared_ptr<AssetMap> asset_map;
	try {
		filesystem::path p = _directory;
		p /= "ASSETMAP";
		if (filesystem::exists (p)) {
			asset_map.reset (new AssetMap (p.string ()));
		} else {
			p = _directory;
			p /= "ASSETMAP.xml";
			if (filesystem::exists (p)) {
				asset_map.reset (new AssetMap (p.string ()));
			} else {
				throw DCPReadError ("could not find AssetMap file");
			}
		}
		
	} catch (FileError& e) {
		throw FileError ("could not load AssetMap file", files.asset_map);
	}

	for (list<shared_ptr<AssetMapAsset> >::const_iterator i = asset_map->assets.begin(); i != asset_map->assets.end(); ++i) {
		if ((*i)->chunks.size() != 1) {
			throw XMLError ("unsupported asset chunk count");
		}

		filesystem::path t = _directory;
		t /= (*i)->chunks.front()->path;
		
		if (ends_with (t.string(), ".mxf") || ends_with (t.string(), ".ttf")) {
			continue;
		}

		xmlpp::DomParser* p = new xmlpp::DomParser;
		try {
			p->parse_file (t.string());
		} catch (std::exception& e) {
			delete p;
			continue;
		}

		string const root = p->get_document()->get_root_node()->get_name ();
		delete p;

		if (root == "CompositionPlaylist") {
			files.cpls.push_back (t.string());
		} else if (root == "PackingList") {
			if (files.pkl.empty ()) {
				files.pkl = t.string();
			} else {
				throw DCPReadError ("duplicate PKLs found");
			}
		} else if (root == "DCSubtitle") {
			files.subtitles.push_back (t.string());
		}
	}
	
	if (files.cpls.empty ()) {
		throw FileError ("no CPL files found", "");
	}

	if (files.pkl.empty ()) {
		throw FileError ("no PKL file found", "");
	}

	shared_ptr<PKLFile> pkl;
	try {
		pkl.reset (new PKLFile (files.pkl));
	} catch (FileError& e) {
		throw FileError ("could not load PKL file", files.pkl);
	}

	/* Cross-check */
	/* XXX */

	for (list<string>::iterator i = files.cpls.begin(); i != files.cpls.end(); ++i) {
		_cpls.push_back (shared_ptr<CPL> (new CPL (_directory, *i, asset_map, require_mxfs)));
	}

}

list<string>
DCP::equals (DCP const & other, EqualityOptions opt) const
{
	list<string> notes;

	if (_cpls.size() != other._cpls.size()) {
		notes.push_back ("CPL counts differ");
	}

	list<shared_ptr<const CPL> >::const_iterator a = _cpls.begin ();
	list<shared_ptr<const CPL> >::const_iterator b = other._cpls.begin ();

	while (a != _cpls.end ()) {
		list<string> n = (*a)->equals (*b->get(), opt);
		notes.merge (n);
		++a;
		++b;
	}

	return notes;
}


void
DCP::add_cpl (shared_ptr<CPL> cpl)
{
	_cpls.push_back (cpl);
}

class AssetComparator
{
public:
	bool operator() (shared_ptr<const Asset> a, shared_ptr<const Asset> b) {
		return a->uuid() < b->uuid();
	}
};

list<shared_ptr<const Asset> >
DCP::assets () const
{
	list<shared_ptr<const Asset> > a;
	for (list<shared_ptr<const CPL> >::const_iterator i = _cpls.begin(); i != _cpls.end(); ++i) {
		list<shared_ptr<const Asset> > t = (*i)->assets ();
		a.merge (t);
	}

	a.sort ();
	a.unique ();
	return a;
}

CPL::CPL (string directory, string name, ContentKind content_kind, int length, int frames_per_second)
	: _directory (directory)
	, _name (name)
	, _content_kind (content_kind)
	, _length (length)
	, _fps (frames_per_second)
{
	_uuid = make_uuid ();
}

CPL::CPL (string directory, string file, shared_ptr<const AssetMap> asset_map, bool require_mxfs)
	: _directory (directory)
	, _content_kind (FEATURE)
	, _length (0)
	, _fps (0)
{
	/* Read the XML */
	shared_ptr<CPLFile> cpl;
	try {
		cpl.reset (new CPLFile (file));
	} catch (FileError& e) {
		throw FileError ("could not load CPL file", file);
	}
	
	/* Now cherry-pick the required bits into our own data structure */
	
	_name = cpl->annotation_text;
	_content_kind = cpl->content_kind;

	for (list<shared_ptr<CPLReel> >::iterator i = cpl->reels.begin(); i != cpl->reels.end(); ++i) {

		shared_ptr<Picture> p;

		if ((*i)->asset_list->main_picture) {
			p = (*i)->asset_list->main_picture;
		} else {
			p = (*i)->asset_list->main_stereoscopic_picture;
		}
		
		_fps = p->edit_rate.numerator;
		_length += p->duration;

		shared_ptr<PictureAsset> picture;
		shared_ptr<SoundAsset> sound;
		shared_ptr<SubtitleAsset> subtitle;

		/* Some rather twisted logic to decide if we are 3D or not;
		   some DCPs give a MainStereoscopicPicture to indicate 3D, others
		   just have a FrameRate twice the EditRate and apparently
		   expect you to divine the fact that they are hence 3D.
		*/

		if (!(*i)->asset_list->main_stereoscopic_picture && p->edit_rate == p->frame_rate) {

			try {
				picture.reset (new MonoPictureAsset (
						       _directory,
						       asset_map->asset_from_id (p->id)->chunks.front()->path,
						       _fps,
						       (*i)->asset_list->main_picture->entry_point,
						       (*i)->asset_list->main_picture->duration
						       )
					);
			} catch (MXFFileError) {
				if (require_mxfs) {
					throw;
				}
			}
			
		} else {

			try {
				picture.reset (new StereoPictureAsset (
						       _directory,
						       asset_map->asset_from_id (p->id)->chunks.front()->path,
						       _fps,
						       p->entry_point,
						       p->duration
						       )
					);
			} catch (MXFFileError) {
				if (require_mxfs) {
					throw;
				}
			}
			
		}
		
		if ((*i)->asset_list->main_sound) {
			
			try {
				sound.reset (new SoundAsset (
						     _directory,
						     asset_map->asset_from_id ((*i)->asset_list->main_sound->id)->chunks.front()->path,
						     _fps,
						     (*i)->asset_list->main_sound->entry_point,
						     (*i)->asset_list->main_sound->duration
						     )
					);
			} catch (MXFFileError) {
				if (require_mxfs) {
					throw;
				}
			}
		}

		if ((*i)->asset_list->main_subtitle) {
			
			subtitle.reset (new SubtitleAsset (
						_directory,
						asset_map->asset_from_id ((*i)->asset_list->main_subtitle->id)->chunks.front()->path
						)
				);
		}
			
		_reels.push_back (shared_ptr<Reel> (new Reel (picture, sound, subtitle)));
	}
}

void
CPL::add_reel (shared_ptr<const Reel> reel)
{
	_reels.push_back (reel);
}

void
CPL::write_xml () const
{
	filesystem::path p;
	p /= _directory;
	stringstream s;
	s << _uuid << "_cpl.xml";
	p /= s.str();
	ofstream os (p.string().c_str());
	
	os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	   << "<CompositionPlaylist xmlns=\"http://www.smpte-ra.org/schemas/429-7/2006/CPL\">\n"
	   << "  <Id>urn:uuid:" << _uuid << "</Id>\n"
	   << "  <AnnotationText>" << _name << "</AnnotationText>\n"
	   << "  <IssueDate>" << Metadata::instance()->issue_date << "</IssueDate>\n"
	   << "  <Creator>" << Metadata::instance()->creator << "</Creator>\n"
	   << "  <ContentTitleText>" << _name << "</ContentTitleText>\n"
	   << "  <ContentKind>" << content_kind_to_string (_content_kind) << "</ContentKind>\n"
	   << "  <ContentVersion>\n"
	   << "    <Id>urn:uri:" << _uuid << "_" << Metadata::instance()->issue_date << "</Id>\n"
	   << "    <LabelText>" << _uuid << "_" << Metadata::instance()->issue_date << "</LabelText>\n"
	   << "  </ContentVersion>\n"
	   << "  <RatingList/>\n"
	   << "  <ReelList>\n";
	
	for (list<shared_ptr<const Reel> >::const_iterator i = _reels.begin(); i != _reels.end(); ++i) {
		(*i)->write_to_cpl (os);
	}

	os << "      </AssetList>\n"
	   << "    </Reel>\n"
	   << "  </ReelList>\n"
	   << "</CompositionPlaylist>\n";

	os.close ();

	_digest = make_digest (p.string (), 0);
	_length = filesystem::file_size (p.string ());
}

void
CPL::write_to_pkl (ostream& s) const
{
	s << "    <Asset>\n"
	  << "      <Id>urn:uuid:" << _uuid << "</Id>\n"
	  << "      <Hash>" << _digest << "</Hash>\n"
	  << "      <Size>" << _length << "</Size>\n"
	  << "      <Type>text/xml</Type>\n"
	  << "    </Asset>\n";
}

list<shared_ptr<const Asset> >
CPL::assets () const
{
	list<shared_ptr<const Asset> > a;
	for (list<shared_ptr<const Reel> >::const_iterator i = _reels.begin(); i != _reels.end(); ++i) {
		if ((*i)->main_picture ()) {
			a.push_back ((*i)->main_picture ());
		}
		if ((*i)->main_sound ()) {
			a.push_back ((*i)->main_sound ());
		}
		if ((*i)->main_subtitle ()) {
			a.push_back ((*i)->main_subtitle ());
		}
	}

	return a;
}

void
CPL::write_to_assetmap (ostream& s) const
{
	s << "    <Asset>\n"
	  << "      <Id>urn:uuid:" << _uuid << "</Id>\n"
	  << "      <ChunkList>\n"
	  << "        <Chunk>\n"
	  << "          <Path>" << _uuid << "_cpl.xml</Path>\n"
	  << "          <VolumeIndex>1</VolumeIndex>\n"
	  << "          <Offset>0</Offset>\n"
	  << "          <Length>" << _length << "</Length>\n"
	  << "        </Chunk>\n"
	  << "      </ChunkList>\n"
	  << "    </Asset>\n";
}
	
	
	
list<string>
CPL::equals (CPL const & other, EqualityOptions opt) const
{
	list<string> notes;
	
	if (opt.flags & LIBDCP_METADATA) {
		if (_name != other._name) {
			notes.push_back ("names differ");
		}
		if (_content_kind != other._content_kind) {
			notes.push_back ("content kinds differ");
		}
		if (_fps != other._fps) {
			notes.push_back ("frames per second differ");
		}
		if (_length != other._length) {
			notes.push_back ("lengths differ");
		}
	}

	if (_reels.size() != other._reels.size()) {
		notes.push_back ("reel counts differ");
	}
	
	list<shared_ptr<const Reel> >::const_iterator a = _reels.begin ();
	list<shared_ptr<const Reel> >::const_iterator b = other._reels.begin ();
	
	while (a != _reels.end ()) {
		list<string> n = (*a)->equals (*b, opt);
		notes.merge (n);
		++a;
		++b;
	}

	return notes;
}
