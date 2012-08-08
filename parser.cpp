#include <string>
#include <map>
#include <set>
#include <cmath>
#include "wchar.h"
#include "czsk-parse.h"
using namespace std;

#define BD '%'

typedef map<string, int> TownStrings;
typedef map<wstring, int> TownWStrings;
typedef map<string, map<string, int> > TownList;

void printlist (TownList town_dicts, const char* outfile) {
  ofstream output (outfile);
  TownList::iterator it1;
  for (it1=town_dicts.begin(); it1 != town_dicts.end(); it1++) {
    output << "Coordinates:\t" << it1->first << endl;
    TownStrings::iterator it2;
    TownWStrings town_dict;
    for (it2=town_dicts[it1->first].begin(); it2 != town_dicts[it1->first].end(); it2++) {
      wchar_t* to_parse = UTF8_to_WChar (it2->first.c_str());
      town_dict[czsk_parse(to_parse, it1->first == "CZ")] = it2->second;
    }
    multimap<int, wstring> inverse_town_dict;
    TownWStrings::iterator it3;
    for (it3=town_dict.begin(); it3 != town_dict.end(); it3++)
      inverse_town_dict.insert(pair<int, wstring> (it3->second, it3->first));
    multimap<int, wstring>::reverse_iterator it4;
    for (it4=inverse_town_dict.rbegin(); it4 != inverse_town_dict.rend(); it4++)
      output << it4->first << '\t' << WChar_to_UTF8(it4->second.c_str()) << endl;
    output << '\n';
  }
}

int main (int argc, char* argv[]) {
  TownList town_dicts;
  czsk_wordlist(town_dicts["CZ"], true, argv[1]);
  czsk_wordlist(town_dicts["SK"], false, argv[2]);
  printlist (town_dicts, argv[2]);
  return 0;
}
