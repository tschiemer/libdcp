#include <iostream>
#include "subtitle_asset.h"

using namespace std;

int main (int argc, char* argv[])
{
	if (argc < 2) {
		cerr << "Syntax: " << argv[0] << " <subtitle file>\n";
		exit (EXIT_FAILURE);
	}
	
	dcp::SubtitleAsset s (argv[1]);
	cout << s.xml_as_string ();
	return 0;
}
