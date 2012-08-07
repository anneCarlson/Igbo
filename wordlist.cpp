// generates a word list from transcripts
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <typeinfo>
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
      split (splitline, line, is_any_of("\t "), token_compress_on);
      vector<string>::iterator it;
      it = splitline.begin();
      int x = 1;
      for (++it; it != splitline.end(); it++) {
	string word = splitline[x];
	if (word != "/" && word != "") {
	  if (word == "") {
	    cout << line << endl;}
	  if ((*town_dict).count(word) == 0) {
	    (*town_dict)[word] = 0;
	  }
	  (*town_dict)[word]++;
	}
	x++;
      }
    }
  }
  corpus.close();
}

void printlist (map<string, map<string, int> > town_dicts, const char* outfile) {
  ofstream output (outfile);
  map<string, map<string, int> >::iterator it1;
  for (it1=town_dicts.begin(); it1 != town_dicts.end(); it1++) {
    output << "Coordinates:\t" << it1->first << endl;
    map<string, int> town_dict = town_dicts[it1->first];
    multimap<int, string> inverse_town_dict;
    map<string, int>::iterator it2;
    for (it2=town_dict.begin(); it2 != town_dict.end(); it2++) {
      inverse_town_dict.insert(pair<int, string> (it2->second, it2->first));
    }
    multimap<int, string>::reverse_iterator it3;
    for (it3=inverse_town_dict.rbegin(); it3 != inverse_town_dict.rend(); it3++) {
      output << it3->first << '\t' << it3->second << endl;
    }
    output << "\n";
    }
}

int main () {
  map<string, map<string, int> > town_dicts;
  cout << "Corpus file: ";
  string ui;
  getline (cin, ui);
  const char* corpfile = ui.c_str();
  wordlist (town_dicts, corpfile);

  cout << "Output file: ";
  getline (cin, ui);
  const char* outfile = ui.c_str();
  printlist (town_dicts, outfile);
  return 0;
}
