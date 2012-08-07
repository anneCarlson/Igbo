#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <cctype>
#include "compare-with-lm2.h"
using namespace std;

// finds all the towns "between" two given towns in town_dicts
vector<Town> towns_between (TownList& town_dicts, Town first_town, Town second_town) {
  vector<Town> to_return;
  float x = second_town.first - first_town.first;
  float y = second_town.second - first_town.second;
  TownList::iterator it;
  for (it=town_dicts.begin(); it != town_dicts.end(); it++) {
    float along_vector = (x*(it->first.first-first_town.first) + y*(it->first.second-first_town.second))/(x*x + y*y);
    if (along_vector >=0 && along_vector <= 1) {
      to_return.push_back (it->first);
    }
  }
  return to_return;
}

void compare_groups (TownList& town_dicts, TownChanges& change_dicts, Town first_town, Town second_town, const char* outfile) {
  vector<Town> towns = towns_between (town_dicts, first_town, second_town);
  vector<Town>::iterator it1;
  for (it1=towns.begin(); it1 != towns.end(); it1++) {
    ChangeMatches change_dict;
    TownWords empty;
    compare (town_dicts[first_town], town_dicts[*it1], empty, empty, change_dict);
    change_dicts[second_town] = change_dict;
    printlist (change_dict, first_town, *it1, outfile);
  }
}

void gather_pairlists (TownChanges& change_dicts, const char* pairfile) {
  ifstream pairs (pairfile);
  bool top_header = true;
  bool header = false;
  enc_change change;
  Town second_town;
  while (pairs.good()) {
    if (top_header) {
      pairs.ignore (256, 'd');
      pairs.ignore (1);
      char coord_string [50];
      pairs.getline (coord_string, 50, ',');
      stringstream first_coord (coord_string);
      first_coord >> second_town.first;
      pairs.getline (coord_string, 50);
      stringstream second_coord (coord_string);
      second_coord >> second_town.second;
      first_coord.flush();
      second_coord.flush();
      top_header = false;
    }
    else if (header) {
      bool done = false;
      pairs.ignore (5, '[');
      WordPairs newmap;
      if (pairs.peek() == '-') {
	char c = pairs.get();
	if (pairs.peek() != '*') {
	  char change_string [10];
	  pairs.getline (change_string, 10, ']');
	  wstring str = UTF8_to_WChar (change_string);
	  if (encoding.count(str) == 0) {
	    encoding[str] = enc_ct;
	    decoding[enc_ct++] = str;
	  }
	  change = encoding[str]*512;
	  done = true;
	}
	else
	  pairs.putback (c);
      }
      else if (pairs.peek() == '+') {
	char change_string [10];
	pairs.ignore (1);
	pairs.getline (change_string, 10, ']');
	wstring str = UTF8_to_WChar (change_string);
	if (encoding.count(str) == 0) {
	  encoding[str] = enc_ct;
	  decoding[enc_ct++] = str;
	}
	change = encoding[str];
	done = true;
      }
      if (done == false) {
	char change_first [10];
	char change_second [10];
	pairs.getline (change_first, 10, '*');
	wstring str = UTF8_to_WChar (change_first);
	if (encoding.count(str) == 0) {
	  encoding[str] = enc_ct;
	  decoding[enc_ct++] = str;
	}
	change = encoding[str]*512;
	pairs.getline (change_second, 10, ']');
	str = UTF8_to_WChar (change_second);
	if (encoding.count(str) == 0) {
	  encoding[str] = enc_ct;
	  decoding[enc_ct++] = str;
	}
	change += encoding[str];
      }
      pairs.ignore (256, '\n');
      header = false;
    }
    else if (pairs.peek() == '\n') {
      pairs.ignore (256, '\n');
      if (pairs.peek() == '\t')
	header = true;
      else
	top_header = true;
    }
    else if (isdigit(pairs.peek()))
      pairs.ignore (256, '\n');
    else {
      char first_word [128];
      char second_word [128];
      pairs.ignore (256, '\t');
      pairs.getline (first_word, 128, ' ');
      wstring first = UTF8_to_WChar (first_word);
      first = first.substr (0, first.length() -1);
      pairs.getline (second_word, 128);
      wstring second = UTF8_to_WChar (second_word);
      change_dicts[second_town][change].insert (pair<wstring, wstring>(first, second));
    }
  }
}

int main (int argc, char* argv[]) {
  TownList town_dicts;
  TownChanges change_dicts;
  encoding[NULL_CHAR] = enc_ct;
  decoding[enc_ct++] = NULL_CHAR;
  encoding[L""] = 0;
  encoding[L"!"] = enc_ct;
  decoding[enc_ct++] = L"!";
  float first_x;
  float second_x;
  float first_y;
  float second_y;
  stringstream convert1 (argv[3]);
  convert1 >> first_x;
  convert1.flush();
  stringstream convert2 (argv[4]);
  convert2 >> first_y;
  convert2.flush();
  stringstream convert3 (argv[5]);
  convert3 >> second_x;
  convert3.flush();
  stringstream convert4 (argv[6]);
  convert4 >> second_y;
  convert4.flush();
  Town first_town = pair<float, float> (first_x, first_y);
  Town second_town = pair<float, float> (second_x, second_y);
  //gather_lists (town_dicts, argv[1]);
  //towns_between (town_dicts, first_town, second_town);
  //compare_groups (town_dicts, change_dicts, first_town, second_town, argv[7]);
  gather_pairlists (change_dicts, argv[2]);
  find_correspondences (change_dicts, first_town, second_town, argv[1], argv[7], argv[8], argv[9]);
  //printlist (change_dict, argv[3], argv[4], argv[5]);
  return 0;
}
