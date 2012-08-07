#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <list>
#include "compare-with-lm2.h"
using namespace std;

#define GAMMA 1.

typedef unsigned char enc_town;
typedef unsigned short enc_word;

map<Town, enc_town> town_encoding;
map<enc_town, Town> town_decoding;
enc_town town_enc_ct = 0;

map<wstring, Word> split_dict;

int* char_counts;
float* arf_totals;

enum state {NONE, DIFF, SAME};

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

bool calculate_pair2 (map<wstring, Word>& first_split, map<wstring, Word>& second_split, enc_change& change, wstring first_word, wstring second_word) {
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
}

void gather_lists (map<Town, map<wstring, float> >& town_dicts, const char* listfile) {
  ifstream lists (listfile);
  bool header = true;
  Town curr_town;
  map<Town, set<enc_char> > chars;
  map<Town, float> arfs;
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
	curr_town = coordinates;
	arfs[curr_town] = 0;
	town_encoding[coordinates] = town_enc_ct;
	town_decoding[town_enc_ct++] = coordinates;
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
	if (split_dict.count(wide_word) == 0)
	  split_dict[wide_word] = word_chars;
	Word::iterator it;
	for (it=word_chars.begin(); it != word_chars.end(); it++)
	  chars[curr_town].insert (*it);
	town_dicts[curr_town][wide_word] = arf;
      }
    }
  }
  map<Town, set<enc_char> >::iterator it1;
  char_counts = new int [town_enc_ct];
  for (it1=chars.begin(); it1 != chars.end(); it1++)
    char_counts[town_encoding[it1->first]] = it1->second.size();
  map<Town, float>::iterator it2;
  arf_totals = new float [town_enc_ct];
  for (it2=arfs.begin(); it2 != arfs.end(); it2++)
    arf_totals[town_encoding[it2->first]] = it2->second;
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

vector<enc_town> find_neighbors (map<Town, map<wstring, float> >& town_dicts, Town base) {
  list<Town> possible_neighbors;
  map<Town, map<wstring, float> >::iterator it1;
  list<Town>::iterator it2;
  for (it1=town_dicts.begin(); it1 != town_dicts.end(); it1++) {
    Town test = it1->first;
    if (test != base) {
      bool to_add = true;
      it2=possible_neighbors.begin();
      while (it2 != possible_neighbors.end()) {
	Town curr = *it2;
	if (pow(dist(base, curr), 2) + pow(dist(curr, test), 2) < pow(dist(base, test), 2))
	  to_add = false;
	if (pow(dist(base, test), 2) + pow(dist(test, curr), 2) < pow(dist(base, curr), 2))
	  it2 = possible_neighbors.erase (it2);
	else
	  ++it2;
      }
      if (to_add)
	possible_neighbors.push_back (test);
    }
  }
  vector<enc_town> to_return;
  for (it2=possible_neighbors.begin(); it2 != possible_neighbors.end(); it2++)
    to_return.push_back (town_encoding[*it2]);
  return to_return;
}

vector<pair<wstring, enc_change> > find_potential_cognates (wstring word, map<wstring, float> second_town_dict) {
  vector<pair<wstring, enc_change> > to_return;
  map<wstring, float>::iterator it;
  for (it=second_town_dict.begin(); it != second_town_dict.end(); it++) {
    enc_change change;
    if (one_change(split_dict[word], split_dict[it->first], change)) {
      to_return.push_back (pair<wstring, enc_change> (it->first, change));
    }
  }
  return to_return;
}

double binomial_prob (float second_arf, float second_arf_total, float first_prob) {
  int c = (int) second_arf;
  int n = (int) second_arf_total;
  int i = 0;
  double to_return = 1;
  while (++i <= c)
    to_return *= first_prob*(n-i)/i;
  while (++i <= n)
    to_return *= 1-first_prob;
  return to_return;
}

double cognate_prob (map<wstring, float>& town_dict, float first_prob, float second_arf_total, int** corr_counts, int corr_total, int token_count, vector<pair<wstring, enc_change> >& cognate_list, wstring word) {
  map<wstring, double> cog_probs;
  double cognate_odds = 1-pow(1-first_prob, second_arf_total);
  double sum = 0;
  vector<pair<wstring, enc_change> >::iterator it1;
  for (it1=cognate_list.begin(); it1 != cognate_list.end(); it1++) {
    float corr_prob = (corr_counts[it1->second / 512][it1->second % 512] + GAMMA)/(corr_total + token_count*GAMMA);
    cog_probs[it1->first] = binomial_prob (town_dict[it1->first], second_arf_total, first_prob)*corr_prob;
    sum += cog_probs[it1->first];
  }
  return cog_probs[word] * cognate_odds/sum;
}

void add_cognate (map<wstring, wstring>& first_cognates, map<wstring, wstring>& second_cognates, int** first_corr_counts, int** second_corr_counts, int& first_corr_totals, int& second_corr_totals, wstring first_word, wstring second_word, enc_char first_char, enc_char second_char) {
  first_cognates[first_word] = second_word;
  second_cognates[second_word] = first_word;
  ++first_corr_counts[first_char][second_char];
  ++second_corr_counts[second_char][first_char];
  ++first_corr_totals;
  ++second_corr_totals;
}

void remove_cognate (map<wstring, wstring>& first_cognates, map<wstring, wstring>& second_cognates, int** first_corr_counts, int** second_corr_counts, int& first_corr_totals, int& second_corr_totals, wstring first_word, wstring second_word, enc_char first_char, enc_char second_char, bool erase_both) {
  if (erase_both)
    first_cognates.erase (first_word);
  second_cognates.erase (second_word);
  --first_corr_counts[first_char][second_char];
  --second_corr_counts[second_char][first_char];
  --first_corr_totals;
  --second_corr_totals;
}

enc_change find_change (vector<pair<wstring, enc_change> >& potential_cognates, wstring second_word) {
  vector<pair<wstring, enc_change> >::iterator it;
  for (it=potential_cognates.begin(); it != potential_cognates.end(); it++) {
    if (it->first == second_word)
      return it->second;
  }
}

void find_cognates (map<Town, map<wstring, float> >& town_dicts) {
  vector<enc_town> neighbors [town_enc_ct];
  map<wstring, wstring> cognates [town_enc_ct][town_enc_ct];
  map<wstring, vector<pair<wstring, enc_change> > > potential_cognates [town_enc_ct][town_enc_ct];
  int** corr_counts [town_enc_ct][town_enc_ct];
  int corr_totals [town_enc_ct][town_enc_ct];
  for (int i=0; i < town_enc_ct; i++) {
    for (int j=0; j < town_enc_ct; j++) {
      corr_counts[i][j] = new int* [enc_ct];
      corr_totals[i][j] = 0;
      for (int k=0; k < enc_ct; k++) {
	corr_counts[i][j][k] = new int [enc_ct];
	for (int l=0; l < enc_ct; l++)
	  corr_counts[i][j][k][l] = 0;
      }
    }
  }
  map<wstring, enc_word> word_encoding [town_enc_ct];
  map<enc_word, wstring> word_decoding [town_enc_ct];
  enc_word word_enc_cts [town_enc_ct];
  for (int i=0; i < town_enc_ct; i++)
    word_enc_cts[i] = 0;
  map<Town, map<wstring, float> >::iterator it1;
  // initialization
  for (it1=town_dicts.begin(); it1 != town_dicts.end(); it1++) {
    enc_town town_num = town_encoding[it1->first];
    neighbors[town_num] = find_neighbors (town_dicts, it1->first);
    map<wstring, float>::iterator it2;
    for (it2=it1->second.begin(); it2 != it1->second.end(); it2++) {
      word_encoding[town_num][it2->first] = word_enc_cts[town_num];
      word_decoding[town_num][word_enc_cts[town_num]++] = it2->first;
      map<Town, map<wstring, float> >::iterator it3;
      for (it3=town_dicts.begin(); it3 != town_dicts.end(); it3++) {
	if (it1 != it3 && it3->second.count(it2->first) == 1)
	  cognates[town_num][town_encoding[it3->first]][it2->first] = it2->first;
      }
    }
  }
  // Gibbs sampling process
  srand (0);
  for (int i=0; i < 1000; i++) {
    if (i % 100 == 0)
      wcout << i << endl;
    enc_town first_town = rand() % town_enc_ct;
    enc_town second_town = neighbors[first_town][rand() % neighbors[first_town].size()];
    wstring first_word = word_decoding[first_town][rand() % word_enc_cts[first_town]];
    if (cognates[first_town][second_town][first_word] != first_word) {
      if (potential_cognates[first_town][second_town].count(first_word) == 0)
	potential_cognates[first_town][second_town][first_word] = find_potential_cognates (first_word, town_dicts[town_decoding[second_town]]);
      vector<pair<wstring, enc_change> > to_choose = potential_cognates[first_town][second_town][first_word];
      if (to_choose.size() > 0) {
	pair<wstring, enc_change> choice = to_choose[rand() % to_choose.size()];
	wstring second_word = choice.first;
	enc_change curr_change = choice.second;
	if (cognates[second_town][first_town][second_word] != second_word) {
	  enc_char first_char = curr_change / 512;
	  enc_char second_char = curr_change % 512;
	  double first_prob = town_dicts[town_decoding[first_town]][first_word]/arf_totals[first_town];
	  float second_arf_total = arf_totals[second_town];
	  double prob = cognate_prob (town_dicts[town_decoding[second_town]], first_prob, second_arf_total, corr_counts[first_town][second_town], corr_totals[first_town][second_town], char_counts[first_town]*char_counts[second_town], to_choose, second_word);
	  wstring prev_cognate = cognates[first_town][second_town][first_word];
	  wstring prev_cog_cognate = cognates[second_town][first_town][second_word];
	  state prev_state;
	  if (prev_cognate == second_word)
	    prev_state = SAME;
	  else if (prev_cognate.length() > 0)
	    prev_state = DIFF;
	  else
	    prev_state = NONE;
	  int curr_rand = rand();
	  if (curr_rand <= prob*RAND_MAX) {
	    if (prev_state != SAME) {
	      add_cognate (cognates[first_town][second_town], cognates[second_town][first_town], corr_counts[first_town][second_town], corr_counts[second_town][first_town], corr_totals[first_town][second_town], corr_totals[second_town][first_town], first_word, second_word, first_char, second_char);
	      // adjust all the other towns' cognates
	      if (prev_state == DIFF) {
		state prev_cog_state;
		if (prev_cog_cognate.length() > 0)
		  prev_cog_state = DIFF;
		else
		  prev_cog_state = NONE;
		enc_change prev_change = find_change (potential_cognates[first_town][second_town][first_word], prev_cognate);
		remove_cognate (cognates[first_town][second_town], cognates[second_town][first_town], corr_counts[first_town][second_town], corr_counts[second_town][first_town], corr_totals[first_town][second_town], corr_totals[second_town][first_town], first_word, prev_cognate, prev_change / 512, prev_change % 512, false);
		if (prev_cog_state == DIFF) {
		  if (potential_cognates[second_town][first_town][prev_cognate].size() == 0)
		    potential_cognates[second_town][first_town][prev_cognate] = find_potential_cognates (prev_cognate, town_dicts[town_decoding[first_town]]);
		  enc_change prev_cog_change = find_change (potential_cognates[second_town][first_town][prev_cognate], prev_cog_cognate);
		  remove_cognate (cognates[second_town][first_town], cognates[first_town][second_town], corr_counts[second_town][first_town], corr_counts[first_town][second_town], corr_totals[second_town][first_town], corr_totals[first_town][second_town], prev_cognate, prev_cog_cognate, prev_cog_change / 512, prev_cog_change % 512, false);
		}
	      }
	    }
	  }
	  else if (prev_state == SAME)
	    remove_cognate (cognates[first_town][second_town], cognates[second_town][first_town], corr_counts[first_town][second_town], corr_counts[second_town][first_town], corr_totals[first_town][second_town], corr_totals[second_town][first_town], first_word, second_word, first_char, second_char, true);
	}
      }
    }
  }
  ofstream outfile ("gibbs-results/corrs.txt");
  for (int i=0; i < town_enc_ct; i++) {
    for (int j=0; j < town_enc_ct; j++) {
      if (corr_totals[i][j] > 0) {
	outfile << town_decoding[i].first << "," << town_decoding[i].second << "\t" << town_decoding[j].first << "," << town_decoding[j].second << endl;
	multimap<int, enc_change> counts;
	for (int k=0; k < enc_ct; k++) {
	  for (int l=0; l < enc_ct; l++) {
	    if (corr_counts[i][j][k][l] > 0)
	      counts.insert (pair<int, enc_change> (corr_counts[i][j][k][l], k*512+l));
	  }
	}
	multimap<int, enc_change>::reverse_iterator it;
	for (it=counts.rbegin(); it != counts.rend(); it++)
	  outfile << it->first << "\t" << WChar_to_UTF8 (represent_change(it->second).c_str()) << "\t" << it->second << endl;
	outfile << endl;
      }
    }
  }
  ofstream outfile2 ("gibbs-results/cogs.txt");
  for (int i=0; i < town_enc_ct; i++) {
    for (int j=0; j < town_enc_ct; j++) {
      if (cognates[i][j].size() > 0) {
	outfile2 << town_decoding[i].first << "," << town_decoding[i].second << "\t" << town_decoding[j].first << "," << town_decoding[j].second << endl;
	map<wstring, wstring>::iterator it;
	for (it=cognates[i][j].begin(); it != cognates[i][j].end(); it++) {
	  if (it->second.length() > 0 && it->first != it->second)
	    outfile2 << WChar_to_UTF8(it->first.c_str()) << ", " << WChar_to_UTF8(it->second.c_str()) << endl;
	}
	outfile2 << endl;
      }
    }
  }
}

int main (int argc, char* argv[]) {
  //map<Town, vector<wstring> > town_vectors;
  map<Town, map<wstring, float> > town_dicts;
  encoding[NULL_CHAR] = enc_ct;
  decoding[enc_ct++] = NULL_CHAR;
  encoding[L""] = 0;
  encoding[L"!"] = enc_ct;
  decoding[enc_ct++] = L"!";
  /*float first_x;
  float second_x;
  float first_y;
  float second_y;
  stringstream convert1 (argv[2]);
  convert1 >> first_x;
  convert1.flush();
  stringstream convert2 (argv[3]);
  convert2 >> first_y;
  convert2.flush();
  stringstream convert3 (argv[4]);
  convert3 >> second_x;
  convert3.flush();
  stringstream convert4 (argv[5]);
  convert4 >> second_y;
  convert4.flush();
  pair<float, float> first_town (first_x, first_y);
  pair<float, float> second_town (second_x, second_y);*/
  /*vectorize_all (town_vectors, argv[1]);
  map<Town, vector<wstring> >::iterator it;
  for (it=town_vectors.begin(); it != town_vectors.end(); it++) {
    cout << it->first.first << ", " << it->first.second << endl;
    map<wstring, float> arfs;
    all_freqs (it->second, arfs);
    town_dicts[it->first] = arfs;
    }*/
  gather_lists (town_dicts, argv[1]);
  find_cognates (town_dicts);
  //map<enc_change, map<wstring, wstring> > first_to_second;
  //find_changes (town_dicts[first_town], town_dicts[second_town], first_to_second);
  /*map<enc_change, map<wstring, wstring> > second_to_first;
  find_changes (town_dicts[second_town], town_dicts[first_town], second_to_first);
  map<enc_change, map<wstring, wstring> >::iterator it1;
  map<wstring, wstring>::iterator it;
  ofstream good (argv[6]);
  ofstream bad (argv[7]);
  for (it1=first_to_second.begin(); it1 != first_to_second.end(); it1++) {
    good << "\t\t" << WChar_to_UTF8 (represent_change (it1->first).c_str()) << endl;
    bad << "\t\t" << WChar_to_UTF8 (represent_change (it1->first).c_str()) << endl;
    for (it=it1->second.begin(); it != it1->second.end(); it++) {
      if (it->first == second_to_first[invert_change(it1->first)][it->second])
	good << town_dicts[first_town][it->first]/town_dicts[second_town][it->second] << "\t" << WChar_to_UTF8 (it->first.c_str()) << ", " << WChar_to_UTF8 (it->second.c_str()) << endl;
      else
	bad << town_dicts[first_town][it->first]/town_dicts[second_town][it->second] << "\t" << WChar_to_UTF8 (it->first.c_str()) << ", " << WChar_to_UTF8 (it->second.c_str()) << endl;
    }
    good << "\n";
    bad << "\n";
    }*/
  //print_arfs (town_dicts, argv[6]);
  //print_changes (town_dicts, first_to_second, first_town, second_town, argv[6]);
  /*map<Town, map<wstring, float> >::iterator it2;
  for (it2=town_dicts.begin(); it2 != town_dicts.end(); it2++)
  find_neighbors (town_dicts, it2->first);*/
  return 0;
}
