// calculates edit distance between words in two word lists
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <list>
#include <string>
#include <map>
#include <set>
#include "wchar.h"
using namespace std;

#define FREQ_POWER 1
#define MIN_FREQ 1
#define NULL_CHAR L"&"
#define BD '%'

typedef unsigned char enc_char;
typedef unsigned short enc_change;
typedef vector<enc_char> Word;
typedef map<wstring, int> TownWords;
typedef map<string, TownWords> TownList;
typedef multimap<wstring, wstring> WordPairs;
typedef map<enc_change, WordPairs> ChangeMatches;

map<wstring, enc_char> encoding;
map<enc_char, wstring> decoding;
unsigned char enc_ct = 0;

map<wstring, wstring> first_modifications;
map<wstring, wstring> second_modifications;

enum process {NUMBER, PATH, PAIR} ;

// outputs a string representing one change
// [-x] = delete x, [+x] = insert x, [x*y] = change x to y
wstring represent_change (enc_change change) {
  wstring representation;
  enc_char first_char = change / 256;
  enc_char second_char = change % 256;
  if (change == 0);
  else if (first_char == 0) {
    representation += L"[+";
    representation += decoding[second_char];
    representation += L"]";
  }
  else if (second_char == 0) {
    representation += L"[-";
    representation += decoding[first_char];
    representation += L"]";
  }
  else {
    representation += L"[";
    representation += decoding[first_char];
    representation += L"*";
    representation += decoding[second_char];
    representation += L"]";
  }
  return representation;
}

bool one_change (Word first, Word second, enc_change& change) {
  int diff = first.size() - second.size();
  change = 0;
  if (diff > 1 || diff < -1)
    return false;
  int i=0, j=0;
  if (diff == 1) {
    while (i < first.size()) {
      if (first[i] == second[j]) {
	i++;
	j++;
      }
      else {
	change = first[i++]*256;
	diff = 0;
	break;
      }
    }
    if (change == 0) {
      change = first[i-1]*256;
      return true;
    }
  }
  else if (diff == -1) {
    while (j < second.size()) {
      if (first[i] == second[j]) {
	i++;
	j++;
      }
      else {
	change = second[j++];
	diff = 0;
	break;
      }
    }
    if (change == 0) {
      change = second[j-1];
      return true;
    }
  }
  while (i < first.size()) {
    if (first[i] == second[j]) {
      i++;
      j++;
    }
    else {
      if (change == 0)
	change = first[i++]*256 + second[j++];
      else
	return false;
    }
  }
  return true;
}

Word split_word (wstring delimited_word) {
  Word split_result;
  wstring curr_char;
  for (int i=0; i != delimited_word.length(); i++) {
    if (delimited_word[i] == BD) {
      if (encoding.count(curr_char) == 0) {
	encoding[curr_char] = enc_ct;
	decoding[enc_ct++] = curr_char;
      }
      split_result.push_back (encoding[curr_char]);
      curr_char.clear();
    }
    else
      curr_char += delimited_word[i];
  }
  return split_result;
}

wstring combine_word (Word split_word) {
  wstring combined_result;
  for (int i=0; i != split_word.size(); i++) {
    combined_result += decoding[split_word[i]];
    combined_result += BD;
  }
  return combined_result;
}

// subroutine for adding a pair of words to our storage dictionary (see below)
bool calculate_pair (TownWords& first_dict, TownWords& second_dict, map<wstring, Word>& split_dict1, map<wstring, Word>& split_dict2, ChangeMatches& change_dict, wstring first_word, wstring second_word) {
  wstring curr_char;
  enc_change change;
  bool close = one_change(split_dict1[first_word], split_dict2[second_word], change);
  if (close) {
    if (change_dict.count(change) == 0) {
      WordPairs newmap;
      change_dict[change] = newmap;
    }
    change_dict[change].insert (pair<wstring, wstring> (first_word, second_word));
  }
  return close;
}

// returns the number of pairs a distance of one away
int compare (TownWords& first_dict, TownWords& second_dict, TownWords& first_dict_other, TownWords& second_dict_other, ChangeMatches& change_dict) {
  int to_return = 0;
  int x = 0;
  TownWords::iterator it1;
  TownWords::iterator it2;
  map <wstring, Word> split_dict1;
  map <wstring, Word> split_dict2;
  for (it1=first_dict.begin(); it1 != first_dict.end(); it1++)
    split_dict1[it1->first] = split_word (it1->first);
  for (it2=second_dict.begin(); it2 != second_dict.end(); it2++)
    split_dict2[it2->first] = split_word (it2->first);
  for (it1=first_dict.begin(); it1 != first_dict.end(); it1++) {
    if (x++ % 10 == 0) {
      wcout << x-1 << endl;
    }
    for (it2=second_dict.begin(); it2 != second_dict.end(); it2++) {
      if (first_dict.count(it2->first) == 0 && first_dict_other.count(it2->first) == 0 && second_dict.count(it1->first) == 0 && second_dict_other.count(it1->first) == 0) {
	if (calculate_pair (first_dict, second_dict, split_dict1, split_dict2, change_dict, it1->first, it2->first))
	  to_return++;
      }
    }
  }
  return to_return;
}

bool modify_word (enc_char environ, bool before, Word& word, enc_char first_char, enc_char second_char) {
  //wcout << decoding[first_char] << ", " << decoding[second_char] << ", " << before << ", " << environ << endl;
  bool modified = false;
  Word::iterator it1;
  list<enc_char> listed_word;
  for (it1=word.begin(); it1 != word.end(); it1++)
    listed_word.push_back (*it1);
  list<enc_char>::iterator it2;
  /*if (environ == 0) {
    if (before) {
      it2 = listed_word.begin();
      if (*it2 == first_char) {
	if (second_char == 0)
	  listed_word.erase (it2);
	else
	  *it2 = second_char;
	modified = true;
      }
    }
    else {
      it2 = listed_word.end();
      it2--;
      if (*it2 == first_char) {
	if (second_char == 0)
	  listed_word.erase (it2);
	else
	  *it2 = second_char;
	modified = true;
      }
    }
    }*/
  if (environ != 0) {
    for (it2=listed_word.begin(); it2 != listed_word.end(); it2++) {
      if (*it2 == environ) {
	if (before) {
	  if (first_char == 0) {
	    it2 = listed_word.insert (++it2, second_char);
	    modified = true;
	  }
	  else {
	    if (++it2 != listed_word.end() && *it2 == first_char) {
	      if (second_char == 0)
		it2 = listed_word.erase (it2);
	      else
		*it2 = second_char;
	      modified = true;
	    }
	    --it2;
	  }
	}
	else {
	  if (first_char == 0) {
	    //wcout << *it << endl;
	    //wcout << combine_word(word) << endl;
	    listed_word.insert (it2, second_char);
	    modified = true;
	  }
	  else {
	    if (it2 != listed_word.begin()) {
	      if (*(--it2) == first_char) {
		if (second_char == 0) {
		  //wcout << combine_word(word) << endl;
		  it2 = listed_word.erase (it2);
		}
		else {
		  *(it2++) = second_char;
		}
		modified = true;
	      }
	      else
		++it2;
	    }
	  }
	}
      }
    }
  }
  Word new_word;
  for (it2=listed_word.begin(); it2 != listed_word.end(); it2++)
    new_word.push_back (*it2);
  word = new_word;
  //wcout << combine_word(word) << endl;
  return modified;
}

int test_environment (multimap<int, enc_char> env_ordered, multimap<enc_char, vector<wstring> >& mods, bool before, TownWords& first_copy, TownWords& second_dict, enc_char first_char, enc_char second_char) {
  multimap<int, enc_char>::reverse_iterator it3;
  int total_changed = 0;
  it3 = env_ordered.rbegin();
  int diff = 0;
  while (diff >= 0) {
    wcout << decoding[it3->second] << endl;
    TownWords first_modified;
    TownWords first_unmodified;
    TownWords::iterator it4;
    for (it4=first_copy.begin(); it4 != first_copy.end(); it4++) {
      Word curr_word = split_word(it4->first);
      //wcout << it4->first << endl;
      if (modify_word(it3->second, before, curr_word, first_char, second_char) && curr_word.size() > 0) {
	//wcout << combine_word(curr_word) << endl;
	first_unmodified[it4->first] = it4->second;
	first_modified[combine_word(curr_word)] = it4->second;
	first_copy.erase (it4->first);
      }
    }
    int modded_equal = 0;
    int unmodded_equal = 0;
    for (it4=first_modified.begin(); it4 != first_modified.end(); it4++) {
      if (second_dict.count(it4->first) == 1)
	modded_equal++;
    }
    for (it4=first_unmodified.begin(); it4 != first_unmodified.end(); it4++) {
      if (second_dict.count(it4->first) == 1)
	unmodded_equal++;
    }
    diff = modded_equal - unmodded_equal;
    wcout << diff << endl;
    TownWords* first_replace;
    if (diff >= 0) {
      vector<wstring> modded_words;
      for (it4=first_unmodified.begin(); it4 != first_unmodified.end(); it4++)
	modded_words.push_back (it4->first);
      mods.insert (pair<enc_char, vector<wstring> > (it3->second, modded_words));
      first_replace = &first_modified;
      total_changed += diff;
    }
    else
      first_replace = &first_unmodified;
    for (it4=first_replace->begin(); it4 != first_replace->end(); it4++) {
      first_copy[it4->first] = it4->second;
    }
    it3++;
    if (it3 == env_ordered.rend())
      break;
  }
  return total_changed;
}

void find_environments (enc_change change, WordPairs pair_dict, multimap<enc_char, vector<wstring> >& mods_made, TownWords& first_dict, TownWords& second_dict) {
  TownWords empty_dict;
  WordPairs::iterator it1;
  enc_char first_char = change / 256;
  enc_char second_char = change % 256;
  map<enc_char, int> before_counts;
  map<enc_char, int> after_counts;
  for (it1=pair_dict.begin(); it1 != pair_dict.end(); it1++) {
    wcout << it1->first << L", " << it1->second << endl;
    Word first = split_word (it1->first);
    Word second = split_word (it1->second);
    enc_char before = 0;
    enc_char after = 0;
    int i = 0;
    while (first[i] == second[i])
      i++;
    if (i > 0)
      before = first[i-1];
    int first_length = first.size();
    if (first_length >= second.size()) {
      if (i < first_length)
	after = first[i+1];
    }
    else {
      if (i < first_length)
	after = first[i];
    }
    if (before_counts.count(before) == 0)
      before_counts[before] = 0;
    if (after_counts.count(after) == 0)
      after_counts[after] = 0;
    wcout << decoding[before] << endl;
    wcout << decoding[after] << endl;
    before_counts[before]++;
    after_counts[after]++;
  }
  multimap<int, enc_char> before_ordered;
  multimap<int, enc_char> after_ordered;
  map<enc_char, int>::iterator it2;
  for (it2=before_counts.begin(); it2 != before_counts.end(); it2++) {
    if (it2->first != 0)
      before_ordered.insert (pair<enc_char, int> (it2->second, it2->first));
  }
  for (it2=after_counts.begin(); it2 != after_counts.end(); it2++) {
    if (it2->first != 0)
      after_ordered.insert (pair<enc_char, int> (it2->second, it2->first));
  }
  int max_changed = 0;
  bool max_is_first = true;
  bool max_is_before = true;
  bool winner = false;
  TownWords max_dict;
  multimap <enc_char, vector<wstring> > max_mods;
  TownWords first_copy = first_dict;
  multimap <enc_char, vector<wstring> > mods;
  int changed = test_environment(before_ordered, mods, true, first_copy, second_dict, first_char, second_char);
  wcout << changed << endl;
  if (changed > max_changed) {
    max_changed = changed;
    max_dict = first_copy;
    max_mods = mods;
    winner = true;
  }
  first_copy = first_dict;
  mods.clear();
  changed = test_environment (after_ordered, mods, false, first_copy, second_dict, first_char, second_char);
  wcout << changed << endl;
  if (changed > max_changed) {
    max_changed = changed;
    max_dict = first_copy;
    max_mods = mods;
    max_is_before = false;
    winner = true;
  }
  TownWords second_copy = second_dict;
  mods.clear();
  changed = test_environment (before_ordered, mods, true, second_copy, first_dict, second_char, first_char);
  wcout << changed << endl;
  if (changed > max_changed) {
    max_changed = changed;
    max_dict = second_copy;
    max_mods = mods;
    max_is_first = false;
    winner = true;
  }
  second_copy = second_dict;
  mods.clear();
  changed = test_environment (after_ordered, mods, false, second_copy, first_dict, second_char, first_char);
  wcout << changed << endl;
  if (changed > max_changed) {
    max_changed = changed;
    max_dict = second_copy;
    max_mods = mods;
    max_is_first = false;
    max_is_before = false;
    winner = true;
  }
  if (winner) {
    if (max_is_first)
      first_dict = max_dict;
    else
      second_dict = max_dict;
    mods_made = max_mods;
  }
}

// prints a representation of our dictionary
void printlist (ChangeMatches& change_dict, string first_town, string second_town, const char* outfile) {
  ofstream output (outfile);
  ChangeMatches::iterator it2;
  multimap<int, enc_change>::reverse_iterator it3;
  WordPairs::iterator it4;
  output << "Comparison: " << first_town << " and " << second_town << endl;
  // sort by frequency
  multimap<int, enc_change> change_counts;
  int total = 0;
  for (it2=change_dict.begin(); it2 != change_dict.end(); it2++) {
    pair<int, enc_change> change_count_pair (it2->second.size(), it2->first);
    change_counts.insert (change_count_pair);
  }
  for (it3=change_counts.rbegin(); it3 != change_counts.rend(); it3++) {
    total += it3->first;
    wstring change_rep = represent_change(it3->second);
    output << "\n\t\t" << WChar_to_UTF8(change_rep.c_str()) << "\t(" << it3->first << ")" << endl;
    for (it4=change_dict[it3->second].begin(); it4 != change_dict[it3->second].end(); it4++)
      output << WChar_to_UTF8(change_rep.c_str()) << "\t" << WChar_to_UTF8(it4->first.c_str()) << ", " << WChar_to_UTF8(it4->second.c_str()) << endl;
  }
  output << total << endl;
}

void printmods (vector<pair<enc_change, multimap<enc_char, vector<wstring> > > >& modified_words, const char* outfile) {
  ofstream output (outfile);
  vector<pair<enc_change, multimap<enc_char, vector<wstring> > > >::iterator it1;
  for (it1=modified_words.begin(); it1 != modified_words.end(); it1++) {
    output << "\t\t" << WChar_to_UTF8 (represent_change(it1->first).c_str()) << endl;
    multimap<enc_char, vector<wstring> >::iterator it2;
    for (it2=it1->second.begin(); it2 != it1->second.end(); it2++) {
      vector<wstring>::iterator it3;
      for (it3=it2->second.begin(); it3 != it2->second.end(); it3++)
	output << WChar_to_UTF8 (decoding[it2->first].c_str()) << "\t" << WChar_to_UTF8 (it3->c_str()) << endl;
    }
  }
}
  
void find_correspondences (TownList& town_dicts, ChangeMatches& change_dict, string first_town, string second_town, const char* outfile, const char* outfile2, const char* outfile3) {
  TownWords first_dict = town_dicts[first_town];
  TownWords second_dict = town_dicts[second_town];
  TownWords empty_dict;
  vector<pair<enc_change, multimap<enc_char, vector<wstring> > > > modified_words;
  //compare (first_dict, second_dict, empty_dict, empty_dict, change_dict);
  set<enc_change> changes_made;
  for (int i=0; i < 30; i++) {
    ChangeMatches::iterator it1;
    multimap<int, enc_change>::reverse_iterator it2;
    multimap<int, enc_change> change_counts;
    for (it1=change_dict.begin(); it1 != change_dict.end(); it1++) {
      change_counts.insert (pair<int, enc_change> (it1->second.size(), it1->first));
    }
    it2 = change_counts.rbegin();
    bool change_found = false;
    enc_change change_to_make;
    while (!change_found) {
      if (changes_made.count(it2->second) == 0) {
	change_found = true;
	change_to_make = it2->second;
	changes_made.insert (it2->second);
      }
      it2++;
    }
    wcout << represent_change (change_to_make) << endl;
    multimap<enc_char, vector<wstring> > mods_made;
    find_environments (change_to_make, change_dict[change_to_make], mods_made,  first_dict, second_dict);
    modified_words.push_back (pair<enc_change, multimap<enc_char, vector<wstring> > > (change_to_make, mods_made));
    change_dict.clear();
    compare (first_dict, second_dict, empty_dict, empty_dict, change_dict);
    printlist (change_dict, first_town, second_town, outfile);
    printmods (modified_words, outfile2);
    ofstream output (outfile3);
    TownWords::iterator it3;
    for (it3=first_dict.begin(); it3 != first_dict.end(); it3++) {
      if (second_dict.count(it3->first) != 0)
	output << it3->second << '\t' << WChar_to_UTF8(it3->first.c_str()) << endl;
    }
  }
}

void gather_lists (TownList& town_dicts, const char* listfile) {
  ifstream lists (listfile);
  bool header = true;
  char coordinates [50];
  while (lists.good()) {
    if (header) {
      lists.ignore (256,'\t');
      lists.getline (coordinates, 256);
      header = false;
      TownWords newmap;
      town_dicts[coordinates] = newmap;
    }
    else {
      if (lists.peek() == '\n') {
	header = true;
	lists.ignore (15,'\n');
      }
      else {
	//cout << coordinates;
	char count_string [6];
	int count;
	lists.getline (count_string, 256, '\t');
	stringstream convert (count_string);
	convert >> count;
	convert.flush();
	char word [500];
	lists.getline (word, 500);
	wstring wide_word = UTF8_to_WChar (word);
	town_dicts[coordinates][wide_word] = count;
      }
    }
  }
}

void gather_pairs (ChangeMatches& change_dict, const char* pairfile) {
  ifstream pairs (pairfile);
  bool header = true;
  enc_change change;
  pairs.ignore (256, '\n');
  pairs.ignore (256, '\n');
  while (pairs.good()) {
    if (header) {
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
	  change = encoding[str]*256;
	  change_dict[change] = newmap;
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
	change_dict[change] = newmap;
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
	change = encoding[str]*256;
	pairs.getline (change_second, 10, ']');
	str = UTF8_to_WChar (change_second);
	if (encoding.count(str) == 0) {
	  encoding[str] = enc_ct;
	  decoding[enc_ct++] = str;
	}
	change += encoding[str];
	change_dict[change] = newmap;
      }
      pairs.ignore (256, '\n');
      header = false;
    }
    else if (pairs.peek() == '\n') {
      pairs.ignore (256, '\n');
      header = true;
    }
    else {
      char first_word [128];
      char second_word [128];
      pairs.ignore (256, '\t');
      pairs.getline (first_word, 128, ' ');
      wstring first = UTF8_to_WChar (first_word);
      first = first.substr (0, first.length() -1);
      pairs.getline (second_word, 128);
      wstring second = UTF8_to_WChar (second_word);
      split_word (first);
      split_word (second);
      change_dict[change].insert (pair<wstring, wstring>(first, second));
    }
  }
}

int main (int argc, char* argv[]) {
  encoding[NULL_CHAR] = enc_ct;
  decoding[enc_ct++] = NULL_CHAR;
  TownList town_dicts;
  ChangeMatches change_dict;
  TownWords empty_dict;
  encoding[L""] = 0;
  gather_lists (town_dicts, argv[1]);
  gather_pairs (change_dict, argv[2]);
  //compare(town_dicts[argv[3]], town_dicts[argv[4]], empty_dict, empty_dict, change_dict);
  find_correspondences (town_dicts, change_dict, argv[3], argv[4], argv[5], argv[6], argv[7]);
  //printlist (change_dict, argv[3], argv[4], argv[5]);
  return 0;
}
