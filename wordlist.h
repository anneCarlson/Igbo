// generates a word list from transcripts
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
using namespace std;
using namespace boost;

void wordlist (map<string, map<string, int> >& town_dicts, const char* corpfile) {
  ifstream corpus (corpfile);
  bool header = true;
  map<string, int>* town_dict;
  string line;
  while (corpus.good()) {
    getline (corpus,line);
    vector<string> splitline;
    split (splitline, line, is_any_of("\t"));
    if (line == "") {
	header = !header;
      }
    else if (header) {
      if (splitline[0] == "Coordinates:") {
	string curr_coords = splitline[1];
	if (town_dicts.count(curr_coords) == 0) {
	  map<string, int> newmap;
	  town_dicts[curr_coords] = newmap;
	}
	  town_dict = &town_dicts[curr_coords];
      }
    }
    else if (!header) {
      string::iterator it;
      for (it=line.begin(); it != line.end(); it++) {
	if (*(it-1) == 'n' && *it == '\'')
	  it = line.insert (++it, ' ');
	if (*(it+1) == ' ' && (*it == ',' || *it == '.' || *it == ':' || *it == ';' || *it == '!' || *it == '?')) {
	  it = line.insert (it, ' ');
	  it++;
	}
      }
      split (splitline, line, is_any_of("\t "), token_compress_on);
      vector<string>::iterator it1;
      it1 = splitline.begin();
      int x = 1;
      for (++it1; it1 != splitline.end(); it1++) {
	string word = splitline[x];
	if (word != "/" && word != "") {
	  if ((*town_dict).count(word) == 0)
	    (*town_dict)[word] = 0;
	  (*town_dict)[word]++;
	}
	x++;
      }
    }
  }
  corpus.close();
}
