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
typedef int enc_word;
typedef int cog_class;

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
	if (split_dict.count(wide_word) == 0)
	  split_dict[wide_word] = word_chars;
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

vector<enc_town> find_neighbors (map<enc_town, map<wstring, float> >& town_dicts, enc_town base_enc) {
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

void add_class (map<cog_class, enc_word*>& cognate_classes, cog_class cog_class_ct) {
  cognate_classes[cog_class_ct] = new enc_word [town_enc_ct];
  for (int i=0; i < town_enc_ct; i++)
    cognate_classes[cog_class_ct][i] = -1;
}

enc_change find_change (map<wstring, map<wstring, enc_change> >& word_pairs, wstring first_word, wstring second_word) {
  if (word_pairs[first_word].count(second_word) == 0) {
    enc_change change;
    if (one_change(split_dict[first_word], split_dict[second_word], change))
      word_pairs[first_word][second_word] = change;
    else
      word_pairs[first_word][second_word] = 0;
  }
  return word_pairs[first_word][second_word];
}

void find_cognates (map<enc_town, map<wstring, float> >& town_dicts, const char* out1, const char* out2) {
  vector<enc_word> word_lists [town_enc_ct];
  enc_word word_counts [town_enc_ct];
  for (int i=0; i < town_enc_ct; i++)
    word_counts[i] = 0;
  map<wstring, enc_word>* word_encoding = new map<wstring, enc_word>();
  vector<wstring> word_decoding;
  enc_word word_enc_ct = 0;
  vector<enc_town> neighbors [town_enc_ct];
  //map<wstring, vector<pair<wstring, enc_change> > > potential_cognates [town_enc_ct][town_enc_ct];
  map<cog_class, enc_word*> cognate_classes;
  map<cog_class, float> class_counts;
  // if the change is -1, the words have a distance of greater than 1
  map<wstring, map<wstring, enc_change> > word_pairs;
  cog_class cog_class_ct = 1;
  float arf_total = 0;
  for (int i=0; i < town_enc_ct; i++)
    arf_total += arf_totals[i];
  short corr_counts [town_enc_ct][town_enc_ct][(enc_ct+1)*512];
  short corr_totals [town_enc_ct][town_enc_ct][enc_ct];
  for (int i=0; i < town_enc_ct; i++) {
    for (int j=0; j < town_enc_ct; j++) {
      for (int k=0; k < (enc_ct+1)*512; k++)
	corr_counts[i][j][k] = 0;
      for (int k=0; k < enc_ct; k++)
	corr_totals[i][j][k] = 0;
    }
  }
  set<wstring>* words_so_far = new set<wstring>();
  map<enc_town, map<wstring, float> >::iterator it1;
  // initialization
  for (it1=town_dicts.begin(); it1 != town_dicts.end(); it1++) {
    neighbors[it1->first] = find_neighbors (town_dicts, it1->first);
    map<wstring, float>::iterator it2;
    for (it2=it1->second.begin(); it2 != it1->second.end(); it2++) {
      enc_word curr_word;
      if (words_so_far->count (it2->first) == 0) {
	(*word_encoding)[it2->first] = word_enc_ct;
	word_decoding.push_back (it2->first);
	words_so_far->insert (it2->first);
	curr_word = word_enc_ct++;
      }
      else
	curr_word = (*word_encoding)[it2->first];
      word_lists[it1->first].push_back (curr_word);
      ++word_counts[it1->first];
    }
  }
  delete words_so_far;
  delete word_encoding;
  cog_class* cognates [town_enc_ct];
  for (int i=0; i < town_enc_ct; i++) {
    cognates[i] = new cog_class [word_counts[i]];
    for (int j=0; j < word_counts[i]; j++)
      cognates[i][j] = 0;
  }
  map<enc_word, cog_class> non_unique_words;
  map<enc_word, cog_class> unique_words;
  for (int i=0; i < town_enc_ct; i++) {
    for (int j=0; j < word_counts[i]; j++) {
      enc_word curr_word = word_lists[i][j];
      cog_class matching_cognate_class;
      if (non_unique_words.count(curr_word) == 1)
	matching_cognate_class = non_unique_words[curr_word];
      else if (unique_words.count(curr_word) == 1) {
	matching_cognate_class = unique_words[curr_word];
	non_unique_words[curr_word] = matching_cognate_class;
	unique_words.erase (curr_word);
      }
      else {
	add_class (cognate_classes, cog_class_ct);
	matching_cognate_class = cog_class_ct++;
	unique_words[curr_word] = matching_cognate_class;
      }
      cognate_classes[matching_cognate_class][i] = curr_word;
      class_counts[matching_cognate_class] += town_dicts[i][word_decoding[curr_word]];
      cognates[i][j] = matching_cognate_class;
      for (int k=0; k < town_enc_ct; k++) {
	if (cognate_classes[matching_cognate_class][k] >= 0 && k != i) {
	  Word::iterator it;
	  for (it=split_dict[curr_word].begin(); it != split_dict[curr_word].end(); it++) {
	    ++corr_counts[i][k][*it*512+it];
	    ++corr_counts[k][i][*it*512+it];
	    ++corr_totals[i][k][*it];
	    ++corr_totals[k][i][*it];
	  }
	}
      }
    }
  }
  // Gibbs sampling process
  srand (0);
  int iter = 0;
  while (iter < 5000) {
    enc_town curr_town = rand() % town_enc_ct;
    enc_word curr_index = rand() % word_counts[curr_town];
    enc_word curr_word_enc = word_lists[curr_town][curr_index];
    wstring curr_word = word_decoding[curr_word_enc];
    if (non_unique_words.count(curr_word_enc) == 0) {
      if (iter % 100 == 0)
	wcout << iter << endl;
      wcout << curr_word_enc << "\t" << curr_word << "\t" << curr_town << endl;
      ++iter;
      float curr_word_count = town_dicts[curr_town][curr_word];
      bool singleton = true;
      vector<pair<enc_town, enc_change> > matching_towns;
      wcout << cognates[curr_town][curr_index] << endl;
      wcout << curr_index << endl;
      for (int i=0; i < town_enc_ct; i++) {
	//wcout << cognate_classes[cognates[curr_town][curr_index]][i] << endl;
	if (cognate_classes[cognates[curr_town][curr_index]][i] >= 0 && i != curr_town) {
	  //wcout << i << endl;
	  singleton = false;
	  matching_towns.push_back (pair<enc_town, enc_change> (i, 0));
	}
      }
      //wcout << 2 << endl;
      if (!singleton) {
	//wcout << 15 << endl;
	cog_class curr_class = cognates[curr_town][curr_index];
	double cog_prob = class_counts[curr_class]/arf_total;
	vector<pair<enc_town, enc_change> >::iterator it;
	for (it=matching_towns.begin(); it != matching_towns.end(); it++) {
	  it->second = find_change (word_pairs, word_decoding[cognate_classes[curr_class][it->first]], curr_word);
	  cog_prob *= (corr_counts[it->first][curr_town][it->second]+GAMMA)/(corr_totals[it->first][curr_town][it->second/512]+GAMMA);
	}
	double not_cog_prob = ((class_counts[curr_class]-curr_word_count)/arf_total) * (curr_word_count/arf_total);
	if (rand() > RAND_MAX*cog_prob/(cog_prob+not_cog_prob)) {
	  cognate_classes[curr_class][curr_town] = -1;
	  add_class (cognate_classes, cog_class_ct);
	  class_counts[curr_class] -= town_dicts[curr_town][curr_word];
	  class_counts[cog_class_ct] += town_dicts[curr_town][curr_word];
	  cognate_classes[cog_class_ct][curr_town] = curr_word_enc;
	  cognates[curr_town][curr_index] == cog_class_ct++;
	  vector<pair<enc_town, enc_change> >::iterator it;
	  // make this for every letter, not just the one changed (ditto below)
	  for (it=matching_towns.begin(); it != matching_towns.end(); it++) {
	    --corr_counts[it->first][curr_town][it->second];
	    --corr_counts[curr_town][it->first][it->second];
	    --corr_totals[it->first][curr_town];
	    --corr_totals[curr_town][it->first];
	  }
	}
      }
      else {
	//wcout << 5 << endl;
	cog_class curr_class = rand() % cog_class_ct;
	while (cognate_classes.count(curr_class) == 0)
	  curr_class = rand() % cog_class_ct;
	double cog_prob = (class_counts[curr_class]+curr_word_count)/arf_total;
	for (int i=0; i < town_enc_ct; i++) {
	  if (cognate_classes[curr_class][i] >= 0) {
	    matching_towns.push_back (pair<enc_town, enc_change> (i, 0));
	  }
	}
	vector<pair<enc_town, enc_change> >::iterator it;
	for (it=matching_towns.begin(); it != matching_towns.end(); it++) {
	  it->second = find_change (word_pairs, word_decoding[cognate_classes[curr_class][it->first]], curr_word);
	  cog_prob *= (corr_counts[it->first][curr_town][it->second]+GAMMA)/(corr_totals[it->first][curr_town]+GAMMA);
	}
	double not_cog_prob = (class_counts[curr_class]/arf_total) * (curr_word_count/arf_total);
	//wcout << cog_prob << "\t" << not_cog_prob << endl;
	if (rand() <= RAND_MAX*cog_prob/(cog_prob+not_cog_prob)) {
	  if (curr_word_enc == 136739) {
	    wcout << "hi" << endl;
	  }
	  cognate_classes.erase (cognates[curr_town][curr_index]);
	  class_counts.erase (cognates[curr_town][curr_index]);
	  class_counts[curr_class] += town_dicts[curr_town][curr_word];
	  cognate_classes[curr_class][curr_town] = curr_word_enc;
	  cognates[curr_town][curr_index] = curr_class;
	  vector<pair<enc_town, enc_change> >::iterator it;
	  for (it=matching_towns.begin(); it != matching_towns.end(); it++) {
	    if (it->second > 0) {
	      ++corr_counts[it->first][curr_town][it->second];
	      ++corr_counts[curr_town][it->first][it->second];
	      ++corr_totals[it->first][curr_town];
	      ++corr_totals[curr_town][it->first];
	    }
	  }
	}
      }
    }
  }
  ofstream outfile (out1);
  for (int i=0; i < town_enc_ct; i++) {
    for (int j=0; j < town_enc_ct; j++) {
      if (corr_totals[i][j] > 0) {
	outfile << town_decoding[i].first << "," << town_decoding[i].second << "\t" << town_decoding[j].first << "," << town_decoding[j].second << endl;
	multimap<int, enc_change> counts;
	for (int k=0; k < enc_ct; k++) {
	  if (corr_counts[i][j][k] > 0)
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
  map<cog_class, enc_word*>::iterator it;
  for (it=cognate_classes.begin(); it != cognate_classes.end(); it++) {
    for (int i=0; i < town_enc_ct; i++) {
      if (it->second[i] >= 0)
	outfile2 << town_decoding[i].first << "," << town_decoding[i].second << "\t" << it->second[i] << "\t" << WChar_to_UTF8 (word_decoding[it->second[i]].c_str()) << endl;
    }
    outfile2 << endl;
  }
}

int main (int argc, char* argv[]) {
  map<enc_town, map<wstring, float> > town_dicts;
  encoding[NULL_CHAR] = enc_ct;
  decoding[enc_ct++] = NULL_CHAR;
  encoding[L""] = 0;
  encoding[L"!"] = enc_ct;
  decoding[enc_ct++] = L"!";
  gather_lists (town_dicts, argv[1]);
  find_cognates (town_dicts, argv[2], argv[3]);
  return 0;
}
