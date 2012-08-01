// calculates edit distance between words in two word lists
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <list>
#include <string>
#include <map>
#include <set>
#include <cmath>
#include <limits>
#include "languagemodeling.h"
#include "czsk-parse.h"
using namespace std;

#define LM_POWER .2
#define GEOG_POWER 5
#define CHANGE_BASE log(.1)
#define TOLERANCE -11
#define FREQ_POWER 1
#define MIN_FREQ 1
#define NULL_CHAR L"&"
#define BD '%'

typedef short enc_char;
typedef int enc_change;
typedef vector<enc_char> Word;
typedef map<wstring, int> TownWords;
typedef map<wstring, map<wstring, int> > TownBigrams;
typedef pair<float, float> Town;
typedef map<Town, TownWords> TownList;
typedef multimap<wstring, wstring> WordPairs;
typedef map<enc_change, WordPairs> ChangeMatches;
typedef map<Town, ChangeMatches> TownChanges;

map<wstring, enc_char> encoding;
map<enc_char, wstring> decoding;
unsigned short enc_ct = 0;

map<wstring, wstring> first_modifications;
map<wstring, wstring> second_modifications;

map<wstring, float> unigram_probs;
map<wstring, map<wstring, float> > bigram_probs;
map<wstring, int> orig_uni_cts;
map<wstring, map<wstring, int> > orig_bi_cts;

enum process {NUMBER, PATH, PAIR} ;

// outputs a string representing one change
// [-x] = delete x, [+x] = insert x, [x*y] = change x to y
wstring represent_change (enc_change change) {
  wstring representation;
  enc_char first_char = change / 512;
  enc_char second_char = change % 512;
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

bool one_change (Word& first, Word& second, enc_change& change) {
  int first_size = first.size();
  int second_size = second.size();
  int diff = first_size - second_size;
  change = 0;
  if (diff > 1 || diff < -1)
    return false;
  int i=0, j=0;
  if (diff == 1) {
    while (i < first_size) {
      if (first[i] == second[j]) {
	i++;
	j++;
      }
      else {
	change = first[i++]*512;
	diff = 0;
	break;
      }
    }
    if (change == 0) {
      change = first[i-1]*512;
      return true;
    }
  }
  else if (diff == -1) {
    while (j < second_size) {
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
  while (i < first_size) {
    if (first[i] == second[j]) {
      i++;
      j++;
    }
    else {
      if (change == 0)
	change = first[i++]*512 + second[j++];
      else {
	change = 0;
	return false;
      }
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
    /*if (change_dict.count(change) == 0) {
      WordPairs newmap;
      change_dict[change] = newmap;
      }*/
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

bool modify_word (pair<enc_char, enc_char> environ, Word& word, enc_char first_char, enc_char second_char) {
  //wcout << decoding[first_char] << ", " << decoding[second_char] << ", " << before << ", " << environ << endl;
  bool modified = false;
  Word::iterator it1;
  list<enc_char> listed_word;
  for (it1=word.begin(); it1 != word.end(); it1++)
    listed_word.push_back (*it1);
  list<enc_char>::iterator it2;
  enc_char before = environ.first;
  enc_char after = environ.second;
  if (before == 0 && after == 0) {
    if (listed_word.size() == 1 && *(listed_word.begin()) == first_char) {
      if (second_char != 0) {
	*(listed_word.begin()) = second_char;
	modified = true;
      }
    }
  }
  else if (before == 0) {
    it2 = listed_word.begin();
    if (first_char == 0) {
      if (*it2 == after) {
	listed_word.insert (it2, second_char);
	modified = true;
      }
    }
    else if (*it2 == first_char) {
      if (++it2 != listed_word.end()) {
	if (*(it2--) == after) {
	  if (second_char == 0)
	    listed_word.erase (it2);
	  else
	    *it2 = second_char;
	  modified = true;
	}
      }
    }
  }
  else if (after == 0) {
    it2 = listed_word.end();
    if (first_char == 0) {
      if (*(--it2) == before) {
	listed_word.insert (++it2, second_char);
	modified = true;
      }
    }
    else if (*(--it2) == first_char) {
      if (it2 != listed_word.begin()) {
	if (*(--it2) == before) {
	  if (second_char == 0)
	    listed_word.erase (++it2);
	  else
	    *(++it2) = second_char;
	  modified = true;
	}
      }
    }
  }
  else {
    it2 = listed_word.begin();
    while (it2 != listed_word.end()) {
      if (*(it2++) == before) {
	if (it2 != listed_word.end()) {
	  if (first_char == 0) {
	    if (*it2 == after) {
	      it2++ = listed_word.insert (it2, second_char);
	      modified = true;
	    }
	  }
	  else if (*it2 == first_char) {
	    if (++it2 != listed_word.end()) {
	      if (*(it2--) == after) {
		if (second_char == 0)
		  it2 = listed_word.erase (it2);
		else
		  *it2 = second_char;
		modified = true;
	      }
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

float dist (Town town1, Town town2) {
  return sqrt (pow(town2.first-town1.first, 2) + pow(town2.second-town1.second, 2));
}

float change_prob (enc_change change, TownChanges& change_dicts, Town first_town, Town second_town) {
  set<Town> has_change;
  set<Town> lacks_change;
  TownChanges::iterator it1;
  for (it1=change_dicts.begin(); it1 != change_dicts.end(); it1++) {
    multimap<int, enc_change> ordered_changes;
    ChangeMatches::iterator it2;
    for (it2=it1->second.begin(); it2 != it1->second.end(); it2++) {
      ordered_changes.insert (pair<int, enc_change> (it2->second.size(), it2->first));
    }
    multimap<int, enc_change>::reverse_iterator it3 = ordered_changes.rbegin();
    bool change_found = false;
    for (int i=0; i < 20; i++)
      ++it3;
    int min_freq = it3->first;
    it3 = ordered_changes.rbegin();
    while (it3->first >= min_freq) {
      if ((it3++)->second == change) {
	has_change.insert (it1->first);
	change_found = true;
	break;
      }
    }
    if (!change_found)
      lacks_change.insert (it1->first);
  }
  float closest_with = numeric_limits<float>::infinity();
  float second_closest;
  float furthest_without = 0;
  float second_furthest;
  int total = 0;
  int wrong = 0;
  set<Town>::iterator it4;
  for (it4=has_change.begin(); it4 != has_change.end(); it4++) {
    ++total;
    float dist_ratio = dist(first_town, *it4)/dist(second_town, *it4);
    //wcout << dist_ratio << "\t" << it4->first << "\t" << it4->second << endl;
    if (dist_ratio <= closest_with) {
      second_closest = closest_with;
      closest_with = dist_ratio;
    }
    else if (dist_ratio < second_closest)
      second_closest = dist_ratio;
  }
  //wcout << "---" << endl;
  for (it4=lacks_change.begin(); it4 != lacks_change.end(); it4++) {
    ++total;
    float dist_ratio = dist(first_town, *it4)/dist(second_town, *it4);
    //wcout << dist_ratio << "\t" << it4->first << "\t" << it4->second << endl;
    if (dist_ratio >= furthest_without) {
      second_furthest = furthest_without;
      furthest_without = dist_ratio;
    }
    else if (dist_ratio > second_furthest)
      second_furthest = dist_ratio;
  }
  wcout << second_closest << endl;
  wcout << second_furthest << endl;
  for (it4=has_change.begin(); it4 != has_change.end(); it4++) {
    if (dist(first_town, *it4)/dist(second_town, *it4) < second_furthest)
      ++wrong;
  }
  for (it4=lacks_change.begin(); it4 != lacks_change.end(); it4++) {
    if (dist(first_town, *it4)/dist(second_town, *it4) > second_closest)
      ++wrong;
  }
  wcout << wrong << " " << total << endl;
  return 1-((float) wrong/total);
}

int test_environment (map<pair<enc_char, enc_char>, int> env_counts, multimap<pair<enc_char, enc_char>, vector<wstring> >& mods, TownWords& first_unigrams, TownBigrams& first_bigrams, TownWords second_unigrams, TownBigrams second_bigrams, enc_char first_char, enc_char second_char, float geog_prob) {
  map<pair<enc_char, enc_char>, int>::iterator it3;
  int total_changed = 0;
  float diff = 0;
  TownWords unigram_counts;
  TownBigrams bigram_counts;
  merge_lists (first_unigrams, second_unigrams, unigram_counts);
  merge_lists (first_bigrams, second_bigrams, bigram_counts);
  float prev_prob = LM_POWER*find_prob (bigram_counts, orig_uni_cts, orig_bi_cts, unigram_probs, bigram_probs);
  for (it3=env_counts.begin(); it3 != env_counts.end(); it3++) {
    TownWords unigram_copy = first_unigrams;
    TownBigrams bigram_copy = first_bigrams;
    wcout << decoding[it3->first.first] << "~" << decoding[it3->first.second] << endl;
    TownWords::iterator it4;
    TownBigrams::iterator it5;
    TownWords::iterator it6;
    vector<wstring> curr_mods;
    for (it4=first_unigrams.begin(); it4 != first_unigrams.end(); it4++) {
      Word curr_word = split_word(it4->first);
      //wcout << it4->first << endl;
      if (/*second_unigrams.count(it4->first) == 0 &&*/ modify_word(it3->first, curr_word, first_char, second_char) && curr_word.size() > 0) {
	wstring combined = combine_word (curr_word);
	curr_mods.push_back (it4->first);
	//wcout << combined << endl;
	unigram_copy[combined] += it4->second;
	unigram_copy.erase (it4->first);
	for (it5=bigram_copy.begin(); it5 != bigram_copy.end(); it5++) {
	  if (it5->second.count(it4->first) == 1) {
	    it5->second[combined] += it5->second[it4->first];
	    it5->second.erase (it4->first);
	  }
	}
	for (it6=bigram_copy[it4->first].begin(); it6 != bigram_copy[it4->first].end(); it6++)
	  bigram_copy[combined][it6->first] += it6->second;
	bigram_copy.erase (it4->first);
      }
    }
    unigram_counts.clear();
    bigram_counts.clear();
    int mod_size = curr_mods.size();
    merge_lists (unigram_copy, second_unigrams, unigram_counts);
    merge_lists (bigram_copy, second_bigrams, bigram_counts);
    float curr_prob = LM_POWER*find_prob (bigram_counts, orig_uni_cts, orig_bi_cts, unigram_probs, bigram_probs) + GEOG_POWER*mod_size*geog_prob;
    /*int c = 0;
    TownWords::iterator it;
    TownBigrams::iterator it2;
    for (it2=first_bigrams.begin(); it2 != first_bigrams.end(); it2++) {
      for (it=it2->second.begin(); it != it2->second.end(); it++)
	c += it->second;
    }
    wcout << c << endl;
    c = 0;
    for (it2=bigram_copy.begin(); it2 != bigram_copy.end(); it2++) {
      for (it=it2->second.begin(); it != it2->second.end(); it++)
	c += it->second;
    }
    wcout << c << endl;*/
    wcout << prev_prob << endl;
    wcout << curr_prob << endl;
    diff = curr_prob - prev_prob;
    wcout << diff << endl;
    TownWords* first_replace;
    wcout << mod_size << endl;
    if (diff >= TOLERANCE*mod_size) {
      first_unigrams = unigram_copy;
      first_bigrams = bigram_copy;
      mods.insert (pair<pair<enc_char, enc_char>, vector<wstring> > (it3->first, curr_mods));
      total_changed += curr_mods.size();
    }
    wcout << total_changed << endl;
  }
  return total_changed;
}

void find_environments (enc_change change, float geog_prob, WordPairs pair_dict, multimap<pair<enc_char, enc_char>, vector<wstring> >& mods_made, TownWords& first_unigrams, TownBigrams& first_bigrams, TownWords& second_unigrams, TownBigrams& second_bigrams) {
  TownWords empty_dict;
  WordPairs::iterator it1;
  enc_char first_char = change / 512;
  enc_char second_char = change % 512;
  map<pair<enc_char, enc_char>, int> env_counts;
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
    wcout << decoding[before] << endl;
    wcout << decoding[after] << endl;
    env_counts[pair<enc_char, enc_char> (before, after)]++;
  }
  TownWords unigram_counts;
  TownBigrams bigram_counts;
  merge_lists (first_unigrams, second_unigrams, unigram_counts);
  merge_lists (first_bigrams, second_bigrams, bigram_counts);
  int max_changed = 0;
  bool max_is_first = true;
  bool winner = false;
  TownWords max_unigrams;
  TownBigrams max_bigrams;
  multimap <pair<enc_char, enc_char>, vector<wstring> > max_mods;
  TownWords unigram_copy = first_unigrams;
  TownBigrams bigram_copy = first_bigrams;
  multimap <pair<enc_char, enc_char>, vector<wstring> > mods;
  wcout << geog_prob << endl;
  int changed = test_environment (env_counts, mods, unigram_copy, bigram_copy, second_unigrams, second_bigrams, first_char, second_char, geog_prob);
  wcout << changed << endl;
  if (changed > max_changed) {
    max_changed = changed;
    max_unigrams = unigram_copy;
    max_bigrams = bigram_copy;
    max_mods = mods;
    winner = true;
  }
  unigram_copy = second_unigrams;
  bigram_copy = second_bigrams;
  mods.clear();
  changed = test_environment (env_counts, mods, unigram_copy, bigram_copy, first_unigrams, first_bigrams, second_char, first_char, geog_prob);
  wcout << changed << endl;
  if (changed > max_changed) {
    max_changed = changed;
    max_unigrams = unigram_copy;
    max_bigrams = bigram_copy;
    max_mods = mods;
    max_is_first = false;
    winner = true;
  }
  if (winner) {
    if (max_is_first) {
      first_unigrams = max_unigrams;
      first_bigrams = max_bigrams;
    }
    else {
      second_unigrams = max_unigrams;
      second_bigrams = max_bigrams;
    }
    mods_made = max_mods;
  }
}

// prints a representation of our dictionary
void printlist (ChangeMatches& change_dict, Town first_town, Town second_town, const char* outfile) {
  ofstream output (outfile);
  ChangeMatches::iterator it2;
  multimap<int, enc_change>::reverse_iterator it3;
  WordPairs::iterator it4;
  output << "Comparison: " << first_town.first << "," << first_town.second << " and " << second_town.first << "," << second_town.second << endl;
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
  output << "\n";
}

void printmods (vector<pair<enc_change, multimap<pair<enc_char, enc_char>, vector<wstring> > > >& modified_words, const char* outfile) {
  ofstream output (outfile);
  vector<pair<enc_change, multimap<pair<enc_char, enc_char>, vector<wstring> > > >::iterator it1;
  for (it1=modified_words.begin(); it1 != modified_words.end(); it1++) {
    output << "\t\t" << WChar_to_UTF8 (represent_change(it1->first).c_str()) << endl;
    multimap<pair<enc_char, enc_char>, vector<wstring> >::iterator it2;
    for (it2=it1->second.begin(); it2 != it1->second.end(); it2++) {
      vector<wstring>::iterator it3;
      for (it3=it2->second.begin(); it3 != it2->second.end(); it3++)
	output << WChar_to_UTF8 (decoding[it2->first.first].c_str()) << "~" << WChar_to_UTF8 (decoding[it2->first.second].c_str()) << "\t" << WChar_to_UTF8 (it3->c_str()) << endl;
    }
  }
}
  
void find_correspondences (TownChanges& change_dicts, Town first_town, Town second_town, const char* corpfile, const char* outfile, const char* outfile2, const char* outfile3) {
  ChangeMatches change_dict = change_dicts[second_town];
  TownWords first_unigrams;
  TownWords second_unigrams;
  TownBigrams first_bigrams;
  TownBigrams second_bigrams;
  counts (first_unigrams, first_bigrams, first_town, corpfile);
  counts (second_unigrams, second_bigrams, second_town, corpfile);
  //czsk_counts (first_unigrams, first_bigrams, true, "Czech");
  //czsk_counts (second_unigrams, second_bigrams, false, "Slovak");
  merge_lists (first_unigrams, second_unigrams, orig_uni_cts);
  merge_lists (first_bigrams, second_bigrams, orig_bi_cts);
  calculate_probs (orig_uni_cts, unigram_probs, orig_bi_cts, bigram_probs);
  TownWords empty_dict;
  vector<pair<enc_change, multimap<pair<enc_char, enc_char>, vector<wstring> > > > modified_words;
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
    multimap<pair<enc_char, enc_char>, vector<wstring> > mods_made;
    float geog_prob = log (change_prob(change_to_make, change_dicts, first_town, second_town));
    find_environments (change_to_make, geog_prob, change_dict[change_to_make], mods_made,  first_unigrams, first_bigrams, second_unigrams, second_bigrams);
    modified_words.push_back (pair<enc_change, multimap<pair<enc_char, enc_char>, vector<wstring> > > (change_to_make, mods_made));
    change_dict.clear();
    compare (first_unigrams, second_unigrams, empty_dict, empty_dict, change_dict);
    printlist (change_dict, first_town, second_town, outfile);
    printmods (modified_words, outfile2);
    ofstream output (outfile3);
    TownWords::iterator it3;
    for (it3=first_unigrams.begin(); it3 != first_unigrams.end(); it3++) {
      if (second_unigrams.count(it3->first) != 0)
	output << it3->second << '\t' << WChar_to_UTF8(it3->first.c_str()) << endl;
    }
  }
}

void gather_lists (TownList& town_dicts, const char* listfile) {
  ifstream lists (listfile);
  bool header = true;
  pair<float, float> coordinates;
  while (lists.good()) {
    if (header) {
      lists.ignore (256,'\t');
      char coord_string [50];
      lists.getline (coord_string, 50, ',');
      stringstream first_coord (coord_string);
      first_coord >> coordinates.first;
      lists.getline (coord_string, 50);
      stringstream second_coord (coord_string);
      second_coord >> coordinates.second;
      first_coord.flush();
      second_coord.flush();
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
	  change = encoding[str]*512;
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
	change = encoding[str]*512;
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

void char_matches (TownWords& first_town, TownWords& second_town, ChangeMatches change_dict, const char* outfile) {
  ofstream output (outfile);
  set<enc_char> first_chars;
  set<enc_char> second_chars;
  TownWords::iterator it1;
  for (it1=first_town.begin(); it1 != first_town.end(); it1++) {
    Word curr_word = split_word (it1->first);
    Word::iterator it2;
    for (it2=curr_word.begin(); it2 != curr_word.end(); it2++) {
      if (first_chars.count(*it2) == 0)
	first_chars.insert (*it2);
    }
  }
  for (it1=second_town.begin(); it1 != second_town.end(); it1++) {
    Word curr_word = split_word (it1->first);
    Word::iterator it2;
    for (it2=curr_word.begin(); it2 != curr_word.end(); it2++) {
      if (second_chars.count(*it2) == 0)
	second_chars.insert (*it2);
    }
  }
  set<enc_char>::iterator it3;
  ChangeMatches::iterator it4;
  for (it3=first_chars.begin(); it3 != first_chars.end(); it3++) {
    if (second_chars.count(*it3) == 0) {
      output << "\t\t" << WChar_to_UTF8 (decoding[*it3].c_str()) << endl;
      for (it4=change_dict.begin(); it4 != change_dict.end(); it4++) {
	enc_char first_char = it4->first / 512;
	if (first_char == *it3)
	  output << it4->second.size() << "\t" << WChar_to_UTF8 (represent_change(it4->first).c_str()) << endl;
      }
      output << "\n";
    }
  }
  for (it3=second_chars.begin(); it3 != second_chars.end(); it3++) {
    if (first_chars.count(*it3) == 0) {
      output << "\t\t" << WChar_to_UTF8 (decoding[*it3].c_str()) << endl;
      for (it4=change_dict.begin(); it4 != change_dict.end(); it4++) {
	enc_char second_char = it4->first % 512;
	if (second_char == *it3)
	  output << it4->second.size() << "\t" << WChar_to_UTF8 (represent_change(it4->first).c_str()) << endl;
      }
      output << "\n";
    }
  }
}
