#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <list>
#include <queue>
#include "compare-with-lm2.h"
#include <time.h>
#include <chrono>
#include <sys/time.h>
using namespace std;

#define GAMMA 0.001
#define ITER 1000
#define DIST_POWER 1/4
#define MIN_COUNT 10

typedef unsigned char enc_town;
typedef int enc_word;
typedef int cog_class;

//time variables for keeping track of time
int totals[10];
//typedef std::chrono::high_resolution_clock Clock;
//typedef std::chrono::milliseconds milliseconds;
int times[10];

// maps numbers to the towns they represent
map<enc_town, Town> town_decoding;
// a counter for encoding towns
enc_town town_enc_ct = 0;
// each town's list of neighbors
vector<enc_town>* neighbors;
// each town's list of words and the cognate classes to which they belong
map<enc_word, cog_class>* cognates;
// tallies of sound correspondences between each pair of sounds in each pair of towns
short*** corr_counts;
// tallies of sounds in each pair of towns (i.e., in the set of words from those two towns that are in the same cognate class)
short*** corr_totals;

// maps numbers to the words they represent
vector<wstring> word_decoding;
// a counter for encoding words
enc_word word_enc_ct = 0;
// each town's list of words
vector<enc_word>* word_lists;
// the number of word types in each town
enc_word* word_counts;
// the (average reduced) frequency across all of Igbo for each word
vector<float> total_word_counts;
// stores the one change (if it is so) between pairs of words
map<enc_word, map<enc_word, enc_change> > word_pairs;
// maps each word to its representation as a vector of characters
vector<Word> split_dict;

// maps numbers to the cognate classes they represent; that is, arrays storing each town's member in the class
map<cog_class, enc_word*> cognate_classes;
// a counter for encoding cognate classes
cog_class cog_class_ct = 1;
// the (average reduced) frequency of each cognate class (that is, the sum of the frequencies of its members)
map<cog_class, float> class_counts;
// a queue of empty classes
queue<cog_class> empty_classes;

// the number of character types in each town
int* char_counts;
// the total (average reduced) frequency for each town
float* arf_totals;
// the total (average reduced) frequency across all of Igbo
float arf_total = 0;

enum state {NONE, DIFF, SAME};

int diff_ms(timeval t1, timeval t2)
{
    return (((t1.tv_sec - t2.tv_sec) * 1000000) + 
            (t1.tv_usec - t2.tv_usec));
}

// takes a town's corpus and turns it into a vector of words
vector<wstring> vectorize (Town town, const char* corpfile) {
  ifstream corpus (corpfile);
  vector<wstring> word_vector;
  Town coords;
  bool header = true;
  bool found_coords = false;
  while (corpus.good()) {
    if (corpus.peek() == '\n') {
      corpus.ignore (256, '\n');
      header = !header;
      if (header)
	found_coords = false;
    }
    else if (header) {
      if (!found_coords) {
	char field [50];
	corpus.getline (field, 20, '\t');
	string field_str = field;
	if (field_str == "Coordinates:") {
	  char coord_string [50];
	  corpus.getline (coord_string, 50, ',');
	  stringstream first_coord (coord_string);
	  first_coord >> coords.first;
	  corpus.getline (coord_string, 50);
	  stringstream second_coord (coord_string);
	  second_coord >> coords.second;
	  first_coord.flush();
	  second_coord.flush();
	  found_coords = true;
	}
      }
      corpus.ignore (100, '\n');
    }
    else if (!header) {
      if (coords == town) {
	corpus.ignore (20, '\t');
	char linechar [500];
	corpus.getline (linechar, 500);
	string line = linechar;
	string::iterator it;
	for (it=line.begin(); it != line.end(); it++) {
	  if (*(it-1) == 'n' && *it == '\'')
	    it = line.insert (++it, ' ');
	  if (*(it-2) == 'n' && *(it-1) == 'a' && *it == '-')
	    it = line.insert (++it, ' ');
	  if (/*(*(it+1) == ' ' || *(it+1) == '\n') &&*/ (*it == ',' || *it == '.' || *it == ':' || *it == ';' || *it == '!' || *it == '?')) {
	    it = line.insert (it, ' ');
	    it++;
	  }
	}
	vector<string> splitline;
	split (splitline, line, is_any_of("\t "), token_compress_on);
	vector<string>::iterator it1;
	for (it1=splitline.begin(); it1 != splitline.end(); it1++) {
	  if (*it1 != "/" && *it1 != "") {
	    wstring curr_string = parse (UTF8_to_WChar(it1->c_str()));
	    word_vector.push_back (curr_string);
	  }
	}
      }
      else
	corpus.ignore (500, '\n');
    }
  }
  return word_vector;
}

// turns every town's corpus into a vector of words
void vectorize_all (map<Town, vector<wstring> >& town_vectors, const char* corpfile) {
  ifstream corpus (corpfile);
  vector<wstring> word_vector;
  Town coords;
  bool header = true;
  bool found_coords = false;
  while (corpus.good()) {
    if (corpus.peek() == '\n') {
      corpus.ignore (256, '\n');
      header = !header;
      if (header)
	found_coords = false;
    }
    else if (header) {
      if (!found_coords) {
	char field [50];
	corpus.getline (field, 20, '\t');
	string field_str = field;
	if (field_str == "Coordinates:") {
	  char coord_string [50];
	  corpus.getline (coord_string, 50, ',');
	  stringstream first_coord (coord_string);
	  first_coord >> coords.first;
	  corpus.getline (coord_string, 50, '\n');
	  stringstream second_coord (coord_string);
	  second_coord >> coords.second;
	  first_coord.flush();
	  second_coord.flush();
	  found_coords = true;
	}
      }
      corpus.ignore (100, '\n');
    }
    else if (!header) {
      corpus.ignore (20, '\t');
      char linechar [500];
      corpus.getline (linechar, 500);
      string line = linechar;
      string::iterator it;
      for (it=line.begin(); it != line.end(); it++) {
	if (*(it-1) == 'n' && *it == '\'')
	  it = line.insert (++it, ' ');
	if (*(it-2) == 'n' && *(it-1) == 'a' && *it == '-')
	    it = line.insert (++it, ' ');
	if (/*(*(it+1) == ' ' || *(it+1) == '\n') &&*/ (*it == ',' || *it == '.' || *it == ':' || *it == ';' || *it == '!' || *it == '?')) {
	  it = line.insert (it, ' ');
	  it++;
	  if (*(it+1) != ' ' && it+1 != line.end()) {
	    it = line.insert (it+1, ' ');
	    it++;
	  }
	}
      }
      vector<string> splitline;
      split (splitline, line, is_any_of("\t "), token_compress_on);
      vector<string>::iterator it1;
      for (it1=splitline.begin(); it1 != splitline.end(); it1++) {
	if (*it1 != "/" && *it1 != "") {
	  wstring curr_string = parse (UTF8_to_WChar(it1->c_str()));
	  town_vectors[coords].push_back (curr_string);
	}
      }
    }
  }
}

// calculates the average reduced frequency of a token (normalizing for burstiness)
float avg_red_freq (vector<wstring>& corpus, wstring word) {
  vector<wstring>::iterator it1;
  int size = 0;
  int freq = 0;
  vector<int> locations;
  for (it1=corpus.begin(); it1 != corpus.end(); it1++) {
    ++size;
    if (*it1 == word) {
      ++freq;
      locations.push_back (size);
    }
  }
  vector<int>::iterator it2;
  float partitions = float(size)/freq;
  float sum = 0;
  int prev_location = size - *(locations.end()-1);
  for (it2=locations.begin(); it2 != locations.end(); it2++) {
    float min_dist = partitions;
    if (*it2 - prev_location < min_dist)
      min_dist = *it2;
    sum += min_dist;
  }
  return (1/partitions)*sum;
}

// calculates the ARF of every word in a given corpus
void all_freqs (vector<wstring>& corpus, map<wstring, float>& town_arfs) {
  vector<wstring>::iterator it1;
  int size = 0;
  map<wstring, int> freq_map;
  map<wstring, vector<int> > location_map;
  for (it1=corpus.begin(); it1 != corpus.end(); it1++) {
    ++size;
    ++freq_map[*it1];
    location_map[*it1].push_back (size);
  }
  map<wstring, vector<int> >::iterator it2;
  for (it2=location_map.begin(); it2 != location_map.end(); it2++) {
    vector<int>::iterator it3;
    float partitions = float(size)/freq_map[it2->first];
    float sum = 0;
    int prev_location = *(it2->second.end()-1) - size;
    for (it3=it2->second.begin(); it3 != it2->second.end(); it3++) {
      float min_dist = partitions;
      if (*it3 - prev_location < min_dist)
	min_dist = *it3 - prev_location;
      sum += min_dist;
      prev_location = *it3;
    }
    town_arfs[it2->first] = sum/partitions;
  }
}

// don't call these procedures
/*bool calculate_pair2 (map<wstring, Word>& first_split, map<wstring, Word>& second_split, enc_change& change, wstring first_word, wstring second_word) {
  wstring curr_char;
  bool close = one_change(first_split[first_word], second_split[second_word], change);
  return close;
}

int find_changes (map<wstring, float>& first_dict, map<wstring, float>& second_dict, map<enc_change, map<wstring, wstring> >& change_dict) {
  multimap<float, wstring> first_ordered;
  map<wstring, Word> first_split;
  map<wstring, Word> second_split;
  map<wstring, float>::iterator it1;
  int to_return = 0;
  for (it1=first_dict.begin(); it1 != first_dict.end(); it1++)
    first_split[it1->first] = split_word (it1->first);
  for (it1=second_dict.begin(); it1 != second_dict.end(); it1++)
    second_split[it1->first] = split_word (it1->first);
  for (it1=first_dict.begin(); it1 != first_dict.end(); it1++)
    first_ordered.insert (pair<float, wstring> (it1->second, it1->first));
  multimap<float, wstring>::reverse_iterator it2;
  int x = 0;
  for (it2=first_ordered.rbegin(); it2 != first_ordered.rend(); it2++) {
    if (x++ % 10 == 0)
      wcout << x-1 << endl;
    wstring best_match;
    float min_arf_diff = 100000;
    enc_change best_change;
    enc_change curr_change;
    for (it1=second_dict.begin(); it1 != second_dict.end(); it1++) {
      if (it2->second == it1->first) {
	//wcout << it2->second << ", " << it1->first << endl;
	best_match = it1->first;
	best_change = 0;
	min_arf_diff = 0;
	break;
      }
      else if (calculate_pair2 (first_split, second_split, curr_change, it2->second, it1->first)) {
	if (abs(second_dict[it1->first]-it2->first) < min_arf_diff) {
	  min_arf_diff = abs(second_dict[it1->first]-it2->first);
	  best_match = it1->first;
	  best_change = curr_change;
	}
      }
    }
    if (min_arf_diff < 100000)
      change_dict[best_change][it2->second] = best_match;
  }
  return to_return;
  }*/

// brings the list of ARFs into the proper data structure
void gather_lists (map<enc_town, map<wstring, float> >& town_dicts, const char* listfile) {
  ifstream lists (listfile);
  bool header = true;
  enc_town curr_town;
  vector<set<enc_char> > chars;
  vector<float> arfs;
  while (lists.good()) {
    if (header) {
      Town coordinates;
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
      if (coordinates.first > 0) {
	arfs.push_back (0);
	set<enc_char> newset;
	chars.push_back (newset);
	town_decoding[town_enc_ct] = coordinates;
	curr_town = town_enc_ct++;
      }
      header = false;
    }
    else {
      if (lists.peek() == '\n') {
	header = true;
	lists.ignore (15,'\n');
      }
      else {
	char arf_string [6];
	float arf;
	lists.getline (arf_string, 256, '\t');
	stringstream convert (arf_string);
	convert >> arf;
	convert.flush();
	arfs[curr_town] += arf;
	char word [500];
	lists.getline (word, 500);
	wstring wide_word = UTF8_to_WChar (word);
	Word word_chars = split_word (wide_word);
	Word::iterator it;
	for (it=word_chars.begin(); it != word_chars.end(); it++)
	  chars[curr_town].insert (*it);
	town_dicts[curr_town][wide_word] = arf;
      }
    }
  }
  char_counts = new int [town_enc_ct];
  for (int i=0; i < town_enc_ct; i++)
    char_counts[i] = chars[i].size();
  arf_totals = new float [town_enc_ct];
  for (int i=0; i < town_enc_ct; i++)
    arf_totals[i] = arfs[i];
}

enc_change invert_change (enc_change change) {
  enc_char first = change / 512;
  enc_char second = change % 512;
  return second*512 + first;
}

void print_arfs (map<Town, map<wstring, float> >& town_dicts, const char* outfile) {
  ofstream output (outfile);
  map<Town, map<wstring, float> >::iterator it1;
  for (it1=town_dicts.begin(); it1 != town_dicts.end(); it1++) {
    output << "Coordinates:\t" << it1->first.first << "," << it1->first.second << endl;
    map<wstring, float>::iterator it2;
    multimap<float, wstring> inverse_town_dict;
    for (it2=it1->second.begin(); it2 != it1->second.end(); it2++)
      inverse_town_dict.insert(pair<float, wstring> (it2->second, it2->first));
    multimap<float, wstring>::reverse_iterator it3;
    for (it3=inverse_town_dict.rbegin(); it3 != inverse_town_dict.rend(); it3++)
      output << it3->first << '\t' << WChar_to_UTF8(it3->second.c_str()) << endl;
    output << '\n';
  }
}

void print_changes (map<Town, map<wstring, float> >& town_dicts, map<enc_change, map<wstring, wstring> >& change_dict, Town first_town, Town second_town, const char* outfile) {
  
  ofstream output (outfile);
  output << "Comparison: " << first_town.first << "," << first_town.second << " and " << second_town.first << "," << second_town.second << endl;
  map<wstring, float> first_dict = town_dicts[first_town];
  map<wstring, float> second_dict = town_dicts[second_town];
  // sort by frequency
  multimap<float, enc_change> ordered_changes;
  map<enc_change, map<wstring, wstring> >::iterator it1;
  map<wstring, wstring>::iterator it2;
  for (it1=change_dict.begin(); it1 != change_dict.end(); it1++) {
    //float total_diff = 0;
    float total_sum = 0;
    for (it2=it1->second.begin(); it2 != it1->second.end(); it2++) {
      //total_diff += abs(first_dict[it2->first]-second_dict[it2->second]);
      total_sum += first_dict[it2->first];
    }
    ordered_changes.insert (pair<float, enc_change> (total_sum, it1->first));
  }
  multimap<float, enc_change>::reverse_iterator it3;
  multimap<float, pair<wstring, wstring> >::reverse_iterator it4;
  for (it3=ordered_changes.rbegin(); it3 != ordered_changes.rend(); it3++) {
    output << "\n\t\t" << WChar_to_UTF8 (represent_change(it3->second).c_str()) << "\t(" << it3->first << ")" << endl;
    multimap<float, pair<wstring, wstring> > ordered_pairs;
    for (it2=change_dict[it3->second].begin(); it2 != change_dict[it3->second].end(); it2++) {
      ordered_pairs.insert (pair<float, pair<wstring, wstring> > (first_dict[it2->first], *it2));
    }
    for (it4=ordered_pairs.rbegin(); it4 != ordered_pairs.rend(); it4++) {
      output << it4->first << "\t" << WChar_to_UTF8 (it4->second.first.c_str()) << ", " << WChar_to_UTF8 (it4->second.second.c_str()) << endl;
    }
  }
 
}

// finds the neighbors of a given town
vector<enc_town> find_neighbors (map<enc_town, map<wstring, float> >& town_dicts, enc_town base_enc) {
  //for postion 0
  timeval time_start,time_end;
  gettimeofday (&time_start, NULL);
  
  Town base = town_decoding[base_enc];
  list<enc_town> possible_neighbors;
  map<enc_town, map<wstring, float> >::iterator it1;
  list<enc_town>::iterator it2;
  for (it1=town_dicts.begin(); it1 != town_dicts.end(); it1++) {
    Town test = town_decoding[it1->first];
    if (test != base) {
      bool to_add = true;
      it2=possible_neighbors.begin();
      while (it2 != possible_neighbors.end()) {
	Town curr = town_decoding[*it2];
	if (pow(dist(base, curr), 2) + pow(dist(curr, test), 2) < pow(dist(base, test), 2))
	  to_add = false;
	if (pow(dist(base, test), 2) + pow(dist(test, curr), 2) < pow(dist(base, curr), 2))
	  it2 = possible_neighbors.erase (it2);
	else
	  ++it2;
      }
      if (to_add)
	possible_neighbors.push_back (it1->first);
    }
  }
  vector<enc_town> to_return;
  for (it2=possible_neighbors.begin(); it2 != possible_neighbors.end(); it2++)
    to_return.push_back (*it2);
  
  //for position 0
    gettimeofday (&time_end, NULL);
    times[0] += diff_ms (time_end,time_start);
    totals[0]++;
  
  return to_return;
}

// finds all the words in a given town that are a distance of 1 away from a given word in a different town
/*vector<pair<wstring, enc_change> > find_potential_cognates (wstring word, map<wstring, float> second_town_dict) {
  vector<pair<wstring, enc_change> > to_return;
  map<wstring, float>::iterator it;
  for (it=second_town_dict.begin(); it != second_town_dict.end(); it++) {
    enc_change change;
    if (one_change(split_dict[word], split_dict[it->first], change)) {
      to_return.push_back (pair<wstring, enc_change> (it->first, change));
    }
  }
  return to_return;
  }*/

double binomial_prob (float second_arf, float second_arf_total, float first_prob) {
  //for postion 1
  timeval time_start,time_end;
  gettimeofday(&time_start,NULL);
  
  int c = (int) second_arf;
  int n = (int) second_arf_total;
  int i = 0;
  double to_return = 1;
  while (++i <= c)
    to_return *= first_prob*(n-i)/i;
  while (++i <= n)
    to_return *= 1-first_prob;
  return to_return;
  
  //for position 1
    gettimeofday(&time_end,NULL);
    times[1] += diff_ms (time_end,time_start);
    totals[1]++;
}

void add_class (cog_class to_use) {
  //for postion 2
  timeval time_start,time_end;
  gettimeofday(&time_start,NULL);
  
  cognate_classes[to_use] = new enc_word [town_enc_ct];
  for (int i=0; i < town_enc_ct; i++)
    cognate_classes[to_use][i] = -1;
  
   //for position 2
    gettimeofday(&time_end,NULL);
    times[2] += diff_ms (time_end,time_start);
    totals[2]++;
}

void remove_class (cog_class to_remove) {
  //for postion 3  
  timeval time_start,time_end;
  gettimeofday(&time_start,NULL);
  
  cognate_classes.erase (to_remove);
  class_counts.erase (to_remove);
  empty_classes.push (to_remove);
  
  //for position 3
    gettimeofday(&time_end,NULL);
    times[3] += diff_ms (time_end,time_start);
    totals[3]++;
}

// returns the correspondence between two words if they're a distance of 1 away and 0 otherwise
enc_change find_change (enc_word first_word, enc_word second_word) {
  //for postion 4
  timeval time_start,time_end;
  gettimeofday(&time_start,NULL);
  
  //if (first_word == second_word)
  //wcout << "identical words: " << word_decoding[first_word] << ", " << word_decoding[second_word] << endl;
  enc_change change = 0;
  if (word_pairs[first_word].count(second_word) == 0) {
    if (one_change(split_dict[first_word], split_dict[second_word], change)) {
      word_pairs[first_word][second_word] = change;
      word_pairs[second_word][first_word] = (change%512)*512 + (change/512);
    }
    else {
      word_pairs[first_word][second_word] = 0;
      word_pairs[second_word][first_word] = 0;
    }
  }
  else
    change = word_pairs[first_word][second_word];
  
  //for position 4
    gettimeofday(&time_end,NULL);
    times[4] += diff_ms (time_end,time_start);
    totals[4]++;
  
  return change;
}

// returns a list of letter correspondences between two given words
map<enc_change, int> count_adjust (enc_town first_town, enc_town second_town, enc_word first_word, enc_word second_word, enc_change change) {
  //for postion 5
  timeval time_start,time_end;
  gettimeofday(&time_start,NULL);
  
  Word::iterator it;
  // assume that every letter correpsonds with itself, tally up accordingly
  map<enc_change, int> first_counts;
  Word first_split = split_dict[first_word];
  for (it=first_split.begin(); it != first_split.end(); it++) {
    enc_change letter = *it*512 + *it;
    if (first_counts.count(letter) == 0)
      first_counts[letter] = 0;
    ++first_counts[letter];
  }
  // if the words are one apart, fix this assumption by working the one differing correspondence into account
  if (change > 0) {
    if (change/512 > 0)
      --first_counts[(change/512)*512+(change/512)];
    ++first_counts[change];
    
    //for position 5
    gettimeofday(&time_end,NULL);
    times[5] += diff_ms (time_end,time_start);
    totals[5]++;
    
    return first_counts;
  }
  // if the words are identical, we're good
  else if (first_word == second_word)
  {
    //for position 5
    gettimeofday(&time_end,NULL);
    times[5] += diff_ms (time_end,time_start);
    totals[5]++;
    
    return first_counts;
  }
  // otherwise it takes more work
  else {
    map<enc_change, int> to_return;
    // go through the second word as well, tally up and remove the words that correspond to themselves
    map<enc_change, int> second_counts;
    Word second_split = split_dict[second_word];
    for (it=second_split.begin(); it != second_split.end(); it++) {
      enc_change letter = *it*512 + *it;
      if (first_counts.count(letter) == 1) {
	--first_counts[letter];
	if (to_return.count(letter) == 0)
	  to_return[letter] = 0;
	++to_return[letter];
	if (first_counts[letter] == 0)
	  first_counts.erase (letter);
      }
      else {
	if (second_counts.count(letter) == 0)
	  second_counts[letter] = 0;
	++second_counts[letter];
      }
    }
    // put all the remaining letters into an ordered list
    list<enc_char> first_list;
    list<enc_char> second_list;
    int first_size = 0;
    int second_size = 0;
    for (it=first_split.begin(); it != first_split.end(); it++) {
      enc_change letter = *it*512 + *it;
      if (first_counts.count(letter) == 1) {
	first_list.push_back (*it);
	++first_size;
	--first_counts[letter];
	if (first_counts[letter] == 0)
	  first_counts.erase (letter);
      }
    }
    for (it=second_split.begin(); it != second_split.end(); it++) {
      enc_change letter = *it*512 + *it;
      if (second_counts.count(letter) == 1) {
	second_list.push_back (*it);
	++second_size;
	--second_counts[letter];
	if (second_counts[letter] == 0)
	  second_counts.erase (letter);
      }
    }
    // if the words aren't the same size, find the letters in the longer word most likely to correspond to NULL in the shorter word
    if (first_counts.size() + second_counts.size() > 0)
      wcout << "oops!: " << first_counts.size() << "\t" << second_counts.size() << endl;
    list<enc_char>::iterator it1 = first_list.begin();
    list<enc_char>::iterator it2 = second_list.begin();
    if (first_size != second_size) {
      int diff = first_size - second_size;
      // first word is longer
      if (diff > 0) {
	priority_queue<pair<float, enc_char> > null_matches;
	for (it1=first_list.begin(); it1 != first_list.end(); it1++) {
	  null_matches.push (pair<float, enc_char> ((corr_counts[second_town][first_town][*it1]+GAMMA)/(corr_totals[second_town][first_town][0]+100*GAMMA), *it1));
	}
	set<enc_char> to_delete;
	for (int i=0; i < diff; i++) {
	  to_delete.insert (null_matches.top().second);
	  null_matches.pop();
	}
	// tally those and remove them accordingly
	it1 = first_list.begin();
	while (diff > 0) {
	  if (to_delete.count(*it1) == 1) {
	    if (to_return.count (*it1*512) == 0)
	      to_return[*it1*512] = 0;
	    ++to_return[*it1*512];
	    it1 = first_list.erase (it1);
	    --diff;
	  }
	  else
	    ++it1;
	}
      }
      // second word is longer
      else {
	priority_queue<pair<float, enc_char> > null_matches;
	for (it2=second_list.begin(); it2 != second_list.end(); it2++)
	  null_matches.push (pair<float, enc_char> ((corr_counts[first_town][second_town][*it2]+GAMMA)/(corr_totals[first_town][second_town][0]+100*GAMMA), *it2));
	set<enc_char> to_delete;
	for (int i=0; i > diff; i--) {
	  to_delete.insert (null_matches.top().second);
	  null_matches.pop();
	}
	it2 = second_list.begin();
	while (diff < 0) {
	  if (to_delete.count(*it2) == 1) {
	    if (to_return.count (*it2) == 0)
	      to_return[*it2] = 0;
	    ++to_return[*it2];
	    it2 = second_list.erase (it2);
	    ++diff;
	  }
	  else
	    ++it2;
	}
      }
    }
    // now that there's an equal number of letters remaining in each town, pair those off as they align
    it1 = first_list.begin();
    it2 = second_list.begin();
    for (it1; it1 != first_list.end(); it1++) {
      enc_change pair = *it1*512 + *(it2++);
      if (to_return.count(pair) == 0)
	to_return[pair] = 0;
      ++to_return[pair];
    }
    
    //for position 5
    gettimeofday(&time_end,NULL);
    times[5] += diff_ms (time_end,time_start);
    totals[5]++;
    
    return to_return;
  }
}

void add_counts (enc_town first_town, enc_town second_town, enc_word first_word, enc_word second_word, enc_change change) {
  //for postion 6
  timeval time_start,time_end;
  gettimeofday(&time_start,NULL);
  
  map<enc_change, int> to_add = count_adjust (first_town, second_town, first_word, second_word, change);
  map<enc_change, int>::iterator it;
  for (it=to_add.begin(); it != to_add.end(); it++) {
    corr_counts[first_town][second_town][it->first] += it->second;
    corr_counts[second_town][first_town][(it->first%512)*512 + (it->first/512)] += it->second;
    corr_totals[first_town][second_town][it->first/512] += it->second;
    corr_totals[second_town][first_town][it->first%512] += it->second;
  }
  
   //for position 6
    gettimeofday(&time_end,NULL);
    times[6] += diff_ms (time_end,time_start);
    totals[6]++;
}

void remove_counts (enc_town first_town, enc_town second_town, enc_word first_word, enc_word second_word, enc_change change) {
  //for postion 7
  timeval time_start,time_end;
  gettimeofday(&time_start,NULL);
  
  map<enc_change, int> to_remove = count_adjust (first_town, second_town, first_word, second_word, change);
  map<enc_change, int>::iterator it;
  for (it=to_remove.begin(); it != to_remove.end(); it++) {
    corr_counts[first_town][second_town][it->first] -= it->second;
    corr_counts[second_town][first_town][(it->first%512)*512 + (it->first/512)] -= it->second;
    corr_totals[first_town][second_town][it->first/512] -= it->second;
    corr_totals[second_town][first_town][it->first%512] -= it->second;
  }
  
    gettimeofday(&time_end,NULL);
    times[7] += diff_ms (time_end,time_start);
    totals[7]++;
}

double word_match_prob (enc_town first_town, enc_town second_town, enc_word first_word, enc_word second_word, bool adding_word, enc_change change) {
  //for postion 8
  timeval time_start,time_end;
  gettimeofday(&time_start,NULL);
  
  double prob_to_return = 1;
  map<enc_change, int> change_counts = count_adjust (first_town, second_town, first_word, second_word, change);
  map<enc_change, int>::iterator it;
  int a_to_b;
  int a_to_anything;
  for (it=change_counts.begin(); it != change_counts.end(); it++) {
    if (it->second > 0) {
      a_to_b = corr_counts[first_town][second_town][it->first];
      a_to_anything = corr_totals[second_town][first_town][it->first/512];
      if (adding_word) {
	a_to_b += it->second;
	a_to_anything += it->second;
      }
      prob_to_return *= pow((a_to_b + GAMMA)/(a_to_anything + (char_counts[second_town]*GAMMA)), it->second);
      /*if(prob_to_return<0)
      {
	cout << "prob_to_return: " << prob_to_return << "\n";
	cout << "a_to_b: " << a_to_b << "\n";
	cout << "corr_counts[first_town][second_town][it->first]: " << corr_counts[first_town][second_town][it->first] << "\n";
	//cout << "char_counts[second_town]: " << char_counts[second_town] << "\n";
	cout << "\n";
      }*/
      //cout << "here!\n";
      /*if(a_to_b<=0||a_to_anything<=0||char_counts[second_town]<=0)
      {
	cout << "a_to_b: " << a_to_b << "\n";
	cout << "a_to_anything: " << a_to_anything << "\n";
	cout << "char_counts[second_town]: " << char_counts[second_town] << "\n";
	cout << "\n";
      }*/
    }
  }
    gettimeofday(&time_end,NULL);
    times[8] += diff_ms (time_end,time_start);
    totals[8]++;
    
  return prob_to_return;
}

double total_log_prob (map<enc_town, map<wstring, float> >& town_dicts) {
  //for postion 9
  timeval time_start,time_end;
  gettimeofday(&time_start,NULL);
  
  double to_return = 0;
  map<cog_class, enc_word*>::iterator it1;
  vector<enc_town>::iterator it2;
  for (it1=cognate_classes.begin(); it1 != cognate_classes.end(); it1++) {
    //wcout << it1->first << "\t" << to_return << endl;
    double exp_prob = class_counts[it1->first]/arf_total;
    for (int i=0; i < town_enc_ct; i++) {
      enc_word curr_word = it1->second[i];
      if (curr_word >= 0) {
	float curr_count = town_dicts[i][word_decoding[curr_word]];
	to_return += log(binomial_prob(curr_count, arf_totals[i], exp_prob));
	for (it2=neighbors[i].begin(); it2 != neighbors[i].end(); it2++) {
	  enc_word new_neighbor_word = it1->second[*it2];
	  if (new_neighbor_word >= 0) {
	    to_return += log(word_match_prob(*it2, i, new_neighbor_word, curr_word, false, find_change(new_neighbor_word, curr_word))) + log(word_match_prob(i, *it2, curr_word, new_neighbor_word, false, find_change(curr_word, new_neighbor_word)));
	  }
	}
      }
      else
	to_return += log(pow(1-exp_prob, arf_totals[i]));
    }
  }
  //for position 9
    gettimeofday(&time_end,NULL);
    times[9] += diff_ms (time_end,time_start);
    totals[9]++;
  return to_return;
}

void find_cognates (map<enc_town, map<wstring, float> >& town_dicts, const char* out1, const char* out2, const char* initfile) {
  neighbors = new vector<enc_town> [town_enc_ct];
  cognates = new map<enc_word, cog_class> [town_enc_ct];
  word_lists = new vector<enc_word> [town_enc_ct];
  word_counts = new enc_word [town_enc_ct];
  map<wstring, enc_word>* word_encoding = new map<wstring, enc_word>();
  // initialize everything
  for (int i=0; i < town_enc_ct; i++)
    word_counts[i] = 0;
  corr_counts = new short** [town_enc_ct];
  corr_totals = new short** [town_enc_ct];
  for (int i=0; i < town_enc_ct; i++)
    arf_total += arf_totals[i];
  for (int i=0; i < town_enc_ct; i++) {
    corr_counts[i] = new short* [town_enc_ct];
    corr_totals[i] = new short* [town_enc_ct];
    for (int j=0; j < town_enc_ct; j++) {
      corr_counts[i][j] = new short [(enc_ct+1)*512];
      corr_totals[i][j] = new short [enc_ct];
      for (int k=0; k < (enc_ct+1)*512; k++)
	corr_counts[i][j][k] = 0;
      for (int k=0; k < enc_ct; k++)
	corr_totals[i][j][k] = 0;
    }
  }
  set<wstring>* words_so_far = new set<wstring>();
  map<enc_town, map<wstring, float> >::iterator it1;
  // assign each word an integer for encoding and increment the counts we need to keep track of
  for (it1=town_dicts.begin(); it1 != town_dicts.end(); it1++) {
    neighbors[it1->first] = find_neighbors (town_dicts, it1->first);
    it1->second.erase (L"");
    map<wstring, float>::iterator it2;
    for (it2=it1->second.begin(); it2 != it1->second.end(); it2++) {
      enc_word curr_word;
      if (words_so_far->count(it2->first) == 0) {
	(*word_encoding)[it2->first] = word_enc_ct;
	word_decoding.push_back (it2->first);
	total_word_counts.push_back (it2->second);
	split_dict.push_back (split_word(it2->first));
	words_so_far->insert (it2->first);
	curr_word = word_enc_ct++;
      }
      else {
	curr_word = (*word_encoding)[it2->first];
	total_word_counts[curr_word] += it2->second;
      }
      //word_lists[it1->first].push_back (curr_word);
      //++word_counts[it1->first];
    }
  }
  // if a word appears frequently enough across Igbo, add it to the word lists of the towns in which it appears
  for (it1=town_dicts.begin(); it1 != town_dicts.end(); it1++) {
    map<wstring, float>::iterator it2;
    for (it2=it1->second.begin(); it2 != it1->second.end(); it2++) {
      if (total_word_counts[(*word_encoding)[it2->first]] >= MIN_COUNT) {
	word_lists[it1->first].push_back ((*word_encoding)[it2->first]);
	++word_counts[it1->first];
      }
    }
  }
  delete words_so_far;
  delete word_encoding;
  // this part is commented out when we already have its results stored to file (see below)
  for (int i=0; i < town_enc_ct; i++) {
    for (int j=0; j < word_counts[i]; j++) {
      if (total_word_counts[word_lists[i][j]] >= MIN_COUNT) {
	add_class (cog_class_ct);
	cognate_classes[cog_class_ct][i] = word_lists[i][j];
	class_counts[cog_class_ct] += town_dicts[i][word_decoding[word_lists[i][j]]];
	cognates[i][word_lists[i][j]] = cog_class_ct++;
      }
    }
  }
  // make some guesses
  for (int i=0; i < town_enc_ct; i++) {
    wcout << i << endl;
    map<wstring, float> town_dict = town_dicts[i];
    map<enc_word, cog_class>::iterator it1;
    vector<enc_town>::iterator it2;
    map<enc_word, cog_class>::iterator it3;
    for (it1=cognates[i].begin(); it1 != cognates[i].end(); it1++) {
      float word_count = town_dict[word_decoding[it1->first]];
      if (total_word_counts[it1->first] >= MIN_COUNT && word_count > 1) {
	enc_word best_match = -1;
	cog_class best_class = it1->second;
	float best_ratio = 2;
	for (it2=neighbors[i].begin(); it2 != neighbors[i].end(); it2++) {
	  if (i > *it2) {
	    map<wstring, float> neighbor_dict = town_dicts[*it2];
	    if (neighbor_dict.count(word_decoding[it1->first]) == 1 && cognate_classes[cognates[*it2][it1->first]][i] == -1) {
	      best_match = it1->first;
	      best_class = cognates[*it2][it1->first];
	      break;
	    }
	    for (it3=cognates[*it2].begin(); it3 != cognates[*it2].end(); it3++) {
	      float neighbor_count = neighbor_dict[word_decoding[it3->first]];
	      if (total_word_counts[it3->first] >= MIN_COUNT && neighbor_count > 1 && neighbor_count >= .7*word_count && neighbor_count <= 1.3*word_count) {
		if (cognate_classes[it3->second][i] == -1 && town_dict.count(word_decoding[it3->first]) == 0 && find_change(it1->first, it3->first) > 0) {
		  float curr_ratio = neighbor_count/word_count;
		  if (curr_ratio < 1)
		    curr_ratio = 1/curr_ratio;
		  if (curr_ratio < best_ratio) {
		    best_match = it3->first;
		    best_class = it3->second;
		    best_ratio = curr_ratio;
		  }
		}
	      }
	    }
	  }
	}
	if (best_match >= 0) {
	  cog_class old_class = it1->second;
	  it1->second = best_class;
	  //wcout << old_class << "\t" << best_class << endl;
	  cognate_classes[it1->second][i] = it1->first;
	  class_counts[it1->second] += word_count;
	  for (int j=0; j < town_enc_ct; j++) {
	    enc_word new_neighbor_word = cognate_classes[it1->second][j];
	    if (j != i && new_neighbor_word >= 0) {
	      //wcout << word_decoding[it1->first] << "\t" << word_decoding[new_neighbor_word] << endl;
	      enc_change change = find_change (new_neighbor_word, it1->first);
	      //wcout << change << endl;
	      add_counts (i, j, new_neighbor_word, it1->first, change);
	      //if (change > 0)
		//wcout << represent_change (change) << endl;
	    }
	  }
	  if (it1->second != old_class)
	    remove_class (old_class);
	}
      }
    }
  }
  // read initial classes from file
  /*ifstream initial (initfile);
  add_class (cog_class_ct);
  bool empty_class = true;
  while (initial.good()) {
    // a line break indicates a new cognate class
    if (initial.peek() == '\n') {
      empty_class = true;
      initial.ignore (1, '\n');
      //add_class (cognate_classes, ++cog_class_ct);
    }
    else {
      // each line has the (encoded) town and word for one member of the current class
      int town;
      char town_string [4];
      initial.getline (town_string, 4, '\t');
      stringstream town_str (town_string);
      town_str >> town;
      town_str.flush();
      enc_word word;
      char word_string [10];
      initial.getline (word_string, 10, '\t');
      stringstream word_str (word_string);
      word_str >> word;
      word_str.flush();
      initial.ignore (500, '\n');
      // if the word is frequent enough across Igbo, add it to the current class
      if (total_word_counts[word] >= MIN_COUNT) {
	if (empty_class)
	  add_class (++cog_class_ct);
	empty_class = false;
	cognate_classes[cog_class_ct][town] = word;
	class_counts[cog_class_ct] += town_dicts[town][word_decoding[word]];
	cognates[town][word] = cog_class_ct;
	// adjust the sound correspondence counts for members of the class in neighboring towns
	for (int i=0; i < town_enc_ct; i++) {
	  enc_word neighbor_word = cognate_classes[cog_class_ct][i];
	  if (i != town && neighbor_word >= 0) {
	    enc_change change = find_change (neighbor_word, word);
	    add_counts (i, town, neighbor_word, word, change);
	  }
	}
      }
    }
  }
  initial.close();*/
  // Gibbs sampling process
  srand (0);
  int iter = 0;
  while (iter < ITER) {
    //for postion 10
    timeval time_start,time_end;
    gettimeofday (&time_start, NULL);
    
    if (iter % 100 == 0)
      wcout << iter << "\t" << total_log_prob(town_dicts) << endl;
    ++iter;
    // choose a random word in a random town
    enc_town curr_town = rand() % town_enc_ct; 
    enc_word curr_word_enc = word_lists[curr_town][rand() % word_counts[curr_town]];
    wstring curr_word = word_decoding[curr_word_enc];
    cog_class curr_class = cognates[curr_town][curr_word_enc];
    double class_probs [cog_class_ct];
    double total_prob = 0;
    
    for (int i=0; i < cog_class_ct; i++)
      class_probs[i] = 0;
    //wcout << curr_word_enc << "\t" << curr_word << "\t" << curr_town << "\t" << curr_class << endl;
    float curr_count = town_dicts[curr_town][curr_word];
    // start by removing word from class
    cognate_classes[curr_class][curr_town] = -1;
    class_counts[curr_class] -= curr_count;
    bool singleton = false;
    if (class_counts[curr_class] == 0)
      singleton = true;
    vector<enc_town>::iterator it;
    // adjust all the counts with its neighbors when we remove the word
    for (int i=0; i < town_enc_ct; i++) {
      enc_word curr_neighbor_word = cognate_classes[curr_class][i];
      if (i != curr_town && curr_neighbor_word >= 0) {
	remove_counts (i, curr_town, curr_neighbor_word, curr_word_enc, find_change (curr_neighbor_word, curr_word_enc));
      }
    }
    
    // calculate the probability for every class
    map<cog_class, enc_word*>::iterator it1;
    for (it1=cognate_classes.begin(); it1 != cognate_classes.end(); it1++) {
      
  
      
    
    
    
      cog_class new_class = it1->first;
      // the word currently in the class from the given town
      enc_word old_word_enc = cognate_classes[new_class][curr_town];
      //wcout << cognates[curr_town][curr_index] << endl;
      //wcout << curr_index << endl;
      // the probability we find
      double change_prob;
      map<enc_change, int>::iterator it2;
      bool skip = false;
      // if there's already a word there, don't replace it
      if (old_word_enc >= 0) {
	change_prob = 0;
	skip = true;
      }
      else {
	bool has_near_neighbor = false;
	for (it=neighbors[curr_town].begin(); it != neighbors[curr_town].end(); it++) {
	  enc_word neighbor_word = cognate_classes[new_class][*it];
	  // if a word has an identical neighbor in the class, put it there
	  //time (&time_start2);
	  if (neighbor_word == curr_word_enc) {
	    change_prob = 1;
	    skip = true;
	    break;
	  }
	  // if a class doesn't have at least one neighbor a distance of 1 away from the word, skip it
	  else if (neighbor_word >= 0) {
	    //wcout << neighbor_word << "\t" << curr_word_enc << endl;
	    if (find_change (neighbor_word, curr_word_enc) > 0) {
	      //wcout << 9 << endl;
	      has_near_neighbor = true;
	      break;
	    }
	  }
	  //time (&time_end2);
	  //times[11] += difftime (time_end2,time_start2);
	  //totals[11]++;
	  if (!has_near_neighbor && new_class != curr_class) {
	    change_prob = 0;
	    skip = true;
	  }
	}
      }
      
      
      //time (&time_start2);
      // if the above case is true, we need to calculate the probability
      if (!skip) {
	// start with the language model component
	change_prob = 1;
	double exp_prob = 1;
	for(int i=0; i<town_enc_ct; i++)
	{
	  enc_word word_from_neighbor= cognate_classes[new_class][i];
	  exp_prob *=(class_counts[new_class] + curr_count)/arf_total;
	  float count=0;
	  if(word_from_neighbor>=0)
	  {
	    count=town_dicts[i][word_decoding[word_from_neighbor]];
	  }
	  change_prob*=binomial_prob (count, arf_totals[curr_town], exp_prob) ;
	}
	//(class_counts[new_class] + curr_count)/arf_total;
	// fudge factor to encourage merging
	//if (new_class == curr_class && singleton)
	//  change_prob *= pow (.02, split_dict[curr_word_enc].size());
	// calculate the probability of the given word being a cognate of each of its potential neighbors
	for (it=neighbors[curr_town].begin(); it != neighbors[curr_town].end(); it++) {
	  enc_word new_neighbor_word = cognate_classes[new_class][*it];
	  if (new_neighbor_word >= 0) {
	    // the probability of the neighboring word
	    //double exp_prob = town_dicts[*it][word_decoding[new_neighbor_word]]/arf_totals[*it];
	    // multiply by the product of the distributional and phonological probabilities of cognate correspondence for each neighbor in the cognate class
	    change_prob *=  word_match_prob (*it, curr_town, new_neighbor_word, curr_word_enc, true, find_change (new_neighbor_word, curr_word_enc))* word_match_prob (curr_town, *it, curr_word_enc, new_neighbor_word, true, find_change ( curr_word_enc, new_neighbor_word));
	  }
	}
      }
      // add the probability to the list
      class_probs[new_class] = change_prob;
      total_prob += change_prob;
      
      
      //time (&time_end2);
      //times[12] += difftime (time_end2,time_start2);
      //totals[12]++;
    
      //if (change_prob > 0)
      ///wcout << change_prob << "\t" << new_class << endl;
    }
    //time (&time_start2);
    // randomly assign our word to a cognate class, weighted by the probabilities of it going there as calculated above
    double running_total = 0;
    int target = rand();
    cog_class chosen;
    if (total_prob == 0)
      chosen = curr_class;
    else {
      for (int i=0; i < cog_class_ct; i++) {
	running_total += class_probs[i]*RAND_MAX/total_prob;
	if (target < running_total) {
	  chosen = i;
	  break;
	}
      }
    }
    //wcout << chosen << endl;
    // if the word's former class is empty, remove it
    if (chosen != curr_class && singleton)
      remove_class (curr_class);
    // mark the connection everywhere, adjust the sound correspondence counts of the cognate class's other members
    cognate_classes[chosen][curr_town] = curr_word_enc;
    class_counts[chosen] += curr_count;
    cognates[curr_town][curr_word_enc] = chosen;
    for (int i=0; i < town_enc_ct; i++) {
      enc_word new_neighbor_word = cognate_classes[chosen][i];
      if (i != curr_town && new_neighbor_word >= 0)
	add_counts (i, curr_town, new_neighbor_word, curr_word_enc, find_change (new_neighbor_word, curr_word_enc));
    }
   // time (&time_end2);
   //times[13] += difftime (time_end2,time_start2);
   // totals[13]++;

     //for position 10
    gettimeofday (&time_end, NULL);
    times[10] += diff_ms (time_end,time_start);
    totals[10]++;
  }
  // print out all the sound correspondence counts and cognate classes
  ofstream outfile (out1);
  for (int i=0; i < town_enc_ct; i++) {
    for (int j=0; j < town_enc_ct; j++) {
      if (corr_totals[i][j] > 0) {
	outfile << town_decoding[i].first << "," << town_decoding[i].second << "\t" << town_decoding[j].first << "," << town_decoding[j].second << endl;
	multimap<int, enc_change> counts;
	for (int k=0; k < (enc_ct+1)*512; k++) {
	  if (corr_counts[i][j][k] > 0 && k/512 != k%512)
	    counts.insert (pair<int, enc_change> (corr_counts[i][j][k], k));
	}
	multimap<int, enc_change>::reverse_iterator it;
	for (it=counts.rbegin(); it != counts.rend(); it++)
	  outfile << it->first << "\t" << WChar_to_UTF8 (represent_change(it->second).c_str()) << "\t" << it->second << endl;
	outfile << endl;
      }
    }
  }
  ofstream outfile2 (out2);
  map<cog_class, enc_word*>::iterator it4;
  for (it4=cognate_classes.begin(); it4 != cognate_classes.end(); it4++) {
    for (int i=0; i < town_enc_ct; i++) {
      if (it4->second[i] >= 0)
	outfile2 << town_decoding[i].first << "," << town_decoding[i].second << "\t" << it4->second[i] << "\t" << WChar_to_UTF8 (word_decoding[it4->second[i]].c_str()) << endl;
      //outfile2 << i << "\t" << it4->second[i] << "\t" << WChar_to_UTF8 (word_decoding[it4->second[i]].c_str()) << endl;
    }
    outfile2 << endl;
  }
}

int main (int argc, char* argv[]) {
  for (int i=0; i<=10; i++)
  {
    times[i]=0;
    totals[i]=0;
  }
  
  map<enc_town, map<wstring, float> > town_dicts;
  encoding[NULL_CHAR] = enc_ct;
  decoding[enc_ct++] = NULL_CHAR;
  encoding[L""] = 0;
  encoding[L"!"] = enc_ct;
  decoding[enc_ct++] = L"!";
  gather_lists (town_dicts, argv[1]);
  
  //enc_town curr_town = rand() % town_enc_ct; 
  cout << "finding cognates:";
  find_cognates (town_dicts, argv[3], argv[4], argv[2]);
  cout.precision(15);
  cout << "\n\n\n";
  cout << "Times:\n";
  for (int i=0; i<=10; i++)
  {
    if (totals[i]!=0)
      std::cout << i  << "\t" << (times[i]+0.0/totals[i]) << "\t" << times[i] << "/" << totals[i] << "\t" << (times[i]+0.0)/totals[10] <<"\t" << (times[i]+0.0)/times[10] << "\n";
    //<< "\t" << (times[i].count()/totals[i])
  }
  cout << "end times\n";
  return 0;
}
