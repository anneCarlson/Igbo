// prints a word list from transcripts
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include "czsk-parse.h"
using namespace std;

void printlist (map<string, map<string, int> >& town_dicts, const char* outfile) {
  ofstream output (outfile);
  map<string, map<string, int> >::iterator it1;
  for (it1=town_dicts.begin(); it1 != town_dicts.end(); it1++) {
    output << "Coordinates:\t" << it1->first << endl;
    map<string, int> town_dict = town_dicts[it1->first];
    multimap<int, string> inverse_town_dict;
    map<string, int>::iterator it2;
    for (it2=town_dict.begin(); it2 != town_dict.end(); it2++)
      inverse_town_dict.insert(pair<int, string> (it2->second, it2->first));
    multimap<int, string>::reverse_iterator it3;
    for (it3=inverse_town_dict.rbegin(); it3 != inverse_town_dict.rend(); it3++)
      output << it3->first << '\t' << it3->second << endl;
    output << "\n";
  }
}

int main (int argc, char* argv[]) {
  map<string, map<string, int> > town_dicts;
  wordlist (town_dicts, argv[1]);
  printlist (town_dicts, argv[3]);
  return 0;
}
