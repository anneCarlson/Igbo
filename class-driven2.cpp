#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <list>
#include <queue>
#include "compare-with-lm2.h"
using namespace std;

#define GAMMA 1.
#define DELTA 1.5

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

void add_class (map<cog_class, enc_word*>& cognate_classes, cog_class to_use) {
  cognate_classes[to_use] = new enc_word [town_enc_ct];
  for (int i=0; i < town_enc_ct; i++)
    cognate_classes[to_use][i] = -1;
}

enc_change find_change (vector<wstring>& word_decoding, map<enc_word, map<enc_word, enc_change> >& word_pairs, enc_word first_word, enc_word second_word) {
  if (first_word == second_word)
    wcout << "identical words!" << endl;
  if (word_pairs[first_word].count(second_word) == 0) {
    enc_change change;
    if (one_change(split_dict[word_decoding[first_word]], split_dict[word_decoding[second_word]], change)) {
      word_pairs[first_word][second_word] = change;
      word_pairs[second_word][first_word] = (change%512)*512 + (change/512);
    }
    else {
      word_pairs[first_word][second_word] = 0;
      word_pairs[second_word][first_word] = 0;
    }
  }
  return word_pairs[first_word][second_word];
}

map<enc_change, int> count_adjust (Word first_word, enc_change change) {
  map<enc_change, int> to_return;
  Word::iterator it;
  for (it=first_word.begin(); it != first_word.end(); it++) {
    if (to_return.count(*it*512+*it) == 0)
      to_return[*it*512+*it] = 0;
    ++to_return[*it*512+*it];
  }
  --to_return[(change/512)*512+(change/512)];
  ++to_return[change];
  return to_return;
}

void add_counts (short* first_corr_counts, short* second_corr_counts, short* first_corr_totals, short* second_corr_totals, Word& first_word, enc_change change) {
  wcout << represent_change(change) << endl;
  map<enc_change, int> to_add = count_adjust (first_word, change);
  map<enc_change, int>::iterator it;
  for (it=to_add.begin(); it != to_add.end(); it++) {
    first_corr_counts[it->first] += it->second;
    second_corr_counts[(it->first%512)*512 + (it->first/512)] += it->second;
    first_corr_totals[it->first/512] += it->second;
    second_corr_totals[it->first%512] += it->second;
  }
}

void remove_counts (short* first_corr_counts, short* second_corr_counts, short* first_corr_totals, short* second_corr_totals, Word& first_word, enc_change change) {
  map<enc_change, int> to_remove = count_adjust (first_word, change);
  map<enc_change, int>::iterator it;
  for (it=to_remove.begin(); it != to_remove.end(); it++) {
    first_corr_counts[it->first] -= it->second;
    second_corr_counts[(it->first%512)*512 + (it->first/512)] -= it->second;
    first_corr_totals[it->first/512] -= it->second;
    second_corr_totals[it->first%512] -= it->second;
  }
}

double word_match_prob (vector<wstring>& word_decoding, map<enc_word, map<enc_word, enc_change> >& word_pairs, short* corr_counts, short* corr_totals, enc_word first_word, enc_word second_word, enc_word old_word, int total_chars, bool adding_word, bool removing_word) {
  double prob_to_return = 1;
  enc_change change = find_change (word_decoding, word_pairs, first_word, second_word);
  if (change == 0)
    return 0;
  map<enc_change, int> change_counts = count_adjust (split_dict[word_decoding[first_word]], change);
  map<enc_change, int> removing_change_counts;
  enc_change removing_change;
  if (removing_word) {
    removing_change = find_change (word_decoding, word_pairs, first_word, old_word);
    removing_change_counts = count_adjust (split_dict[word_decoding[first_word]], removing_change);
  }
  map<enc_change, int>::iterator it;
  int a_to_b;
  int a_to_anything;
  for (it=change_counts.begin(); it != change_counts.end(); it++) {
    a_to_b = corr_counts[it->first];
    a_to_anything = corr_totals[it->first/512];
    if (adding_word) {
      a_to_b += it->second;
      a_to_anything += it->second;
    }
    if (removing_word) {
      if (removing_change_counts.count(it->first) == 1) {
	int to_remove = removing_change_counts[it->first];
	a_to_b -= to_remove;
	a_to_anything -= to_remove;
      }
      else if (it->first == (removing_change/512)*512+(removing_change/512))
	a_to_b -= removing_change_counts[removing_change];
    }
    prob_to_return *= pow((a_to_b + GAMMA)/(a_to_anything + (total_chars*GAMMA)), it->second);
  }
  return prob_to_return;
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
  queue<cog_class> empty_classes;
  // if the change is 0, the words have a distance of greater than 1 (identical words are handled earlier)
  map<enc_word, map<enc_word, enc_change> > word_pairs;
  map<enc_word, cog_class> cognates [town_enc_ct];
  cog_class cog_class_ct = 1;
  float arf_total = 0;
  for (int i=0; i < town_enc_ct; i++)
    arf_total += arf_totals[i];
  short* corr_counts [town_enc_ct][town_enc_ct];
  short* corr_totals [town_enc_ct][town_enc_ct];
  for (int i=0; i < town_enc_ct; i++) {
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
  for (int i=0; i < town_enc_ct; i++) {
    for (int j=0; j < word_counts[i]; j++) {
      add_class (cognate_classes, cog_class_ct);
      cognate_classes[cog_class_ct][i] = word_lists[i][j];
      class_counts[cog_class_ct] += town_dicts[i][word_decoding[word_lists[i][j]]];
      cognates[i][word_lists[i][j]] = cog_class_ct++;
    }
  }
  // Gibbs sampling process
  srand (0);
  int iter = 0;
  while (iter < 10) {
    if (iter % 1 == 0)
      wcout << iter << endl;
    ++iter;
    enc_town curr_town = rand() % town_enc_ct;
    enc_word curr_word_enc = word_lists[curr_town][rand() % word_counts[curr_town]];
    wstring curr_word = word_decoding[curr_word_enc];
    cog_class curr_class = cognates[curr_town][curr_word_enc];
    cog_class new_class = rand() % cog_class_ct;
    while (cognate_classes.count(new_class) == 0)
      new_class = rand() % cog_class_ct;
    enc_word old_word_enc = cognate_classes[new_class][curr_town];
    wstring old_word;
    bool replacing = false;
    if (old_word_enc >= 0) {
      replacing = true;
      old_word = word_decoding[old_word_enc];
    }
    bool singleton = true;
    if (class_counts[curr_class] > town_dicts[curr_town][curr_word])
      singleton = false;
    float curr_word_count = town_dicts[curr_town][curr_word];
    //wcout << cognates[curr_town][curr_index] << endl;
    //wcout << curr_index << endl;
    double change_prob;
    double no_change_prob;
    vector<enc_town>::iterator it;
    map<enc_change, int>::iterator it2;
    bool skip = false;
    if (replacing) {
      if (old_word_enc == curr_word_enc)
	skip = true;
      else {
	bool has_neighbor = false;
	for (it=neighbors[curr_town].begin(); it != neighbors[curr_town].end(); it++) {
	  if (cognate_classes[new_class][*it] >= 0)
	    has_neighbor = true;
	  if (cognate_classes[curr_class][*it] == curr_word_enc || cognate_classes[new_class][*it] == old_word_enc) {
	    change_prob = 0;
	    no_change_prob = 1;
	    skip = true;
	    break;
	  }
	  else if (cognate_classes[new_class][*it] == curr_word_enc) {
	    change_prob = 1;
	    no_change_prob = 0;
	    skip = true;
	    break;
	  }
	}
	if (!has_neighbor) {
	  change_prob = 0;
	  no_change_prob = 1;
	  skip = true;
	}
      }
      float curr_count = town_dicts[curr_town][curr_word];
      float old_count = town_dicts[curr_town][old_word];
      //wcout << "r" << skip << endl;
      if (!skip) {
	change_prob = ((class_counts[new_class] + curr_count - old_count)/arf_total) * (old_count/arf_total);
	if (!singleton)
	  change_prob *= (class_counts[curr_class] - curr_count)/arf_total;
	no_change_prob = (class_counts[curr_class]/arf_total) * (class_counts[new_class]/arf_total);
	for (it=neighbors[curr_town].begin(); it != neighbors[curr_town].end(); it++) {
	  enc_word curr_neighbor_word = cognate_classes[curr_class][*it];
	  if (curr_neighbor_word >= 0) {
	    double exp_prob = town_dicts[*it][word_decoding[curr_neighbor_word]]/arf_totals[*it];
	    change_prob *= pow(1-exp_prob, arf_totals[curr_town]);
	    no_change_prob *= binomial_prob (curr_count, arf_totals[curr_town], exp_prob) * word_match_prob (word_decoding, word_pairs, corr_counts[*it][curr_town], corr_totals[*it][curr_town], curr_neighbor_word, curr_word_enc, old_word_enc, char_counts[curr_town], false, false);
	  }
	  enc_word new_neighbor_word = cognate_classes[new_class][*it];
	  if (new_neighbor_word >= 0) {
	    double exp_prob = town_dicts[*it][word_decoding[new_neighbor_word]]/arf_totals[*it];
	    change_prob *= binomial_prob (curr_count, arf_totals[curr_town], exp_prob) * word_match_prob (word_decoding, word_pairs, corr_counts[*it][curr_town], corr_totals[*it][curr_town], new_neighbor_word, curr_word_enc, old_word_enc, char_counts[curr_town], true, true);
	    no_change_prob *= binomial_prob (old_count, arf_totals[curr_town], exp_prob) * word_match_prob (word_decoding, word_pairs, corr_counts[*it][curr_town], corr_totals[*it][curr_town], new_neighbor_word, old_word_enc, -1, char_counts[curr_town], false, false);
	  }
	}
      }
      //wcout << change_prob << "\t" << no_change_prob << endl;
      if (rand() < RAND_MAX*change_prob/(change_prob+no_change_prob)) {
	if (singleton) {
	  cognate_classes.erase (curr_class);
	  class_counts.erase (curr_class);
	}
	else {
	  cognate_classes[curr_class][curr_town] = -1;
	  class_counts[curr_class] -= curr_count;
	  for (it=neighbors[curr_town].begin(); it != neighbors[curr_town].end(); it++) {
	    enc_word curr_neighbor_word = cognate_classes[curr_class][*it];
	    if (curr_neighbor_word >= 0) {
	      remove_counts (corr_counts[*it][curr_town], corr_counts[curr_town][*it], corr_totals[*it][curr_town], corr_totals[curr_town][*it], split_dict[word_decoding[curr_neighbor_word]], find_change(word_decoding, word_pairs, curr_neighbor_word, curr_word_enc));
	    }
	  }
	}
	cognate_classes[new_class][curr_town] = curr_word_enc;
	class_counts[new_class] += curr_count - old_count;
	cognates[curr_town][curr_word_enc] = new_class;
	for (it=neighbors[curr_town].begin(); it != neighbors[curr_town].end(); it++) {
	  enc_word new_neighbor_word = cognate_classes[new_class][*it];
	  if (new_neighbor_word >= 0) {
	    add_counts (corr_counts[*it][curr_town], corr_counts[curr_town][*it], corr_totals[*it][curr_town], corr_totals[curr_town][*it], split_dict[word_decoding[new_neighbor_word]], find_change(word_decoding, word_pairs, new_neighbor_word, curr_word_enc));
	    remove_counts (corr_counts[*it][curr_town], corr_counts[curr_town][*it], corr_totals[*it][curr_town], corr_totals[curr_town][*it], split_dict[word_decoding[new_neighbor_word]], find_change(word_decoding, word_pairs, new_neighbor_word, old_word_enc));
	  }
	}
	add_class (cognate_classes, cog_class_ct);
	class_counts[cog_class_ct] = old_count;
	cognates[curr_town][old_word_enc] = cog_class_ct++;
      }
    }
    else {
      bool has_neighbor = false;
      for (it=neighbors[curr_town].begin(); it != neighbors[curr_town].end(); it++) {
	if (cognate_classes[new_class][*it] >= 0)
	  has_neighbor = true;
	if (cognate_classes[curr_class][*it] == curr_word_enc) {
	  change_prob = 0;
	  no_change_prob = 1;
	  skip = true;
	  break;
	}
	else if (cognate_classes[new_class][*it] == curr_word_enc) {
	  change_prob = 1;
	  no_change_prob = 0;
	  skip = true;
	  break;
	}
      }
      if (!has_neighbor) {
	change_prob = 0;
	no_change_prob = 1;
	skip = true;
      }
      float curr_count = town_dicts[curr_town][curr_word];
      if (!skip) {
	//wcout << curr_word_enc << "\t" << curr_word << "\t" << curr_town << "\t" << curr_class << "\t" << new_class << endl;
	change_prob = (class_counts[new_class] + curr_count)/arf_total;
	if (!singleton)
	  change_prob *= (class_counts[curr_class] - curr_count)/arf_total;
	no_change_prob = (class_counts[curr_class]/arf_total) * (class_counts[new_class]/arf_total);
	//wcout << change_prob << "\t" << no_change_prob << endl;
	for (it=neighbors[curr_town].begin(); it != neighbors[curr_town].end(); it++) {
	  enc_word curr_neighbor_word = cognate_classes[curr_class][*it];
	  if (curr_neighbor_word >= 0) {
	    //wcout << "c " << word_decoding[curr_neighbor_word] << endl;
	    double exp_prob = town_dicts[*it][word_decoding[curr_neighbor_word]]/arf_totals[*it];
	    change_prob *= pow(1-exp_prob, arf_totals[curr_town]);
	    no_change_prob *= binomial_prob (curr_count, arf_totals[curr_town], exp_prob) * word_match_prob (word_decoding, word_pairs, corr_counts[*it][curr_town], corr_totals[*it][curr_town], curr_neighbor_word, curr_word_enc, -1, char_counts[curr_town], false, false);
	  }
	  //wcout << change_prob << "\t" << no_change_prob << endl;
	  enc_word new_neighbor_word = cognate_classes[new_class][*it];
	  if (new_neighbor_word >= 0) {
	    //wcout << "n " << word_decoding[new_neighbor_word] << endl;
	    double exp_prob = town_dicts[*it][word_decoding[new_neighbor_word]]/arf_totals[*it];
	    //wcout << exp_prob << "\t" << arf_totals[curr_town] << endl;
	    change_prob *= binomial_prob (curr_count, arf_totals[curr_town], exp_prob) * word_match_prob (word_decoding, word_pairs, corr_counts[*it][curr_town], corr_totals[*it][curr_town], new_neighbor_word, curr_word_enc, -1, char_counts[curr_town], true, false);
	    no_change_prob *= pow(1-exp_prob, arf_totals[curr_town]);
	  }
	  //wcout << change_prob << "\t" << no_change_prob << endl;
	}
	//wcout << change_prob << "\t" << no_change_prob << endl;
      }
      if (rand() < RAND_MAX*change_prob/(change_prob+no_change_prob)) {
	if (singleton) {
	  cognate_classes.erase (curr_class);
	  class_counts.erase (curr_class);
	}
	else {
	  cognate_classes[curr_class][curr_town] = -1;
	  class_counts[curr_class] -= curr_count;
	  for (it=neighbors[curr_town].begin(); it != neighbors[curr_town].end(); it++) {
	    enc_word curr_neighbor_word = cognate_classes[curr_class][*it];
	    if (curr_neighbor_word >= 0) {
	      remove_counts (corr_counts[*it][curr_town], corr_counts[curr_town][*it], corr_totals[*it][curr_town], corr_totals[curr_town][*it], split_dict[word_decoding[curr_neighbor_word]], find_change(word_decoding, word_pairs, curr_neighbor_word, curr_word_enc));
	    }
	  }
	}
	cognate_classes[new_class][curr_town] = curr_word_enc;
	class_counts[new_class] += curr_count;
	cognates[curr_town][curr_word_enc] = new_class;
	for (it=neighbors[curr_town].begin(); it != neighbors[curr_town].end(); it++) {
	  enc_word new_neighbor_word = cognate_classes[new_class][*it];
	  if (new_neighbor_word >= 0) {
	    add_counts (corr_counts[*it][curr_town], corr_counts[curr_town][*it], corr_totals[*it][curr_town], corr_totals[curr_town][*it], split_dict[word_decoding[new_neighbor_word]], find_change(word_decoding, word_pairs, new_neighbor_word, curr_word_enc));
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
	for (int k=0; k < (enc_ct+1)*512; k++) {
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
