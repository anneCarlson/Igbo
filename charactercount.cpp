#include <map>
#include <iostream>
#include <fstream>
#include "wchar.h"
using namespace std;

void character_count (map<wchar_t, int>& charmap, const char* infile) {
  ifstream input (infile);
  bool header = true;
  string line;
  while (input.good()) {
    getline (input, line);
    if (line == "")
      header = !header;
    else if (!header) {
      wchar_t* line_wide = UTF8_to_WChar (line.c_str());
      while (*line_wide != L'\0') {
	if (charmap.count(*line_wide) == 0)
	  charmap[*line_wide] = 0;
	charmap[*line_wide++]++;
      }
    }
  }
}

void print_counts (map<wchar_t, int>& charmap, const char* outfile) {
  ofstream output (outfile);
  multimap<int, wchar_t> reverse_counts;
  map<wchar_t, int>::iterator it1;
  for (it1=charmap.begin(); it1 != charmap.end(); it1++) {
    pair <int, wchar_t> char_count_pair (it1->second, it1->first);
    reverse_counts.insert(char_count_pair);
  }
  multimap<int, wchar_t>::iterator it2;
  for (it2=reverse_counts.begin(); it2 != reverse_counts.end(); it2++) {
    wchar_t to_convert[2] = {it2->second, L'\0'};
    output << WChar_to_UTF8 (to_convert) << "\t" << it2->second << endl;
  }
}

int main (int arch, char* argv[]) {
  wchar_t test = 7885;
  wcout << test << endl;
  map<wchar_t, int> charmap;
  character_count (charmap, argv[1]);
  print_counts (charmap, argv[2]);
  return 0;
}
