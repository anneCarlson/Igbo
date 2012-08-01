#include <boost/algorithm/string.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <cmath>
#include "parser.h"
#include "wchar.h"
using namespace std;
using namespace boost;

#define ALPHA 1.
#define BETA 20.
#define START "^"
#define UNK L"@"

void counts (map<wstring, int>& unigram_counts, map<wstring, map<wstring, int> >& bigram_counts, pair<float, float> town, const char* corpfile) {
  ifstream corpus (corpfile);
  pair<float, float> coords;
  bool header = true;
  bool found_coords = false;
  string prev_word = START;
  map<string, int> str_unigram_counts;
  map<string, map<string, int> > str_bigram_counts;
  while (corpus.good()) {
    if (corpus.peek() == '\n') {
      corpus.ignore (256, '\n');
      header = !header;
      if (prev_word != START)
	str_unigram_counts[START]++;
      prev_word = START;
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
	  if (*(it+1) == ' ' && (*it == ',' || *it == '.' || *it == ':' || *it == ';' || *it == '!' || *it == '?')) {
	    it = line.insert (it, ' ');
	    it++;
	  }
	}
	vector<string> splitline;
	split (splitline, line, is_any_of("\t "), token_compress_on);
	vector<string>::iterator it1;
	for (it1=splitline.begin(); it1 != splitline.end(); it1++) {
	  if (*it1 != "/" && *it1 != "") {
	    str_unigram_counts[*it1]++;
	    str_bigram_counts[prev_word][*it1]++;
	    prev_word = *it1;
	  }
	}
      }
      else
	corpus.ignore (500, '\n');
    }
  }
  if (prev_word != START)
    str_unigram_counts[START]++;
  map<string, int>::iterator it1;
  map<string, map<string, int> >::iterator it2;
  unigram_counts[UNK] = 0;
  for (it1=str_unigram_counts.begin(); it1 != str_unigram_counts.end(); it1++)
    unigram_counts[parse(UTF8_to_WChar(it1->first.c_str()))] = it1->second;
  for (it2=str_bigram_counts.begin(); it2 != str_bigram_counts.end(); it2++) {
    wstring curr_string = parse(UTF8_to_WChar(it2->first.c_str()));
    for (it1=it2->second.begin(); it1 != it2->second.end(); it1++)
      bigram_counts[curr_string][parse(UTF8_to_WChar(it1->first.c_str()))] = it1->second;
  }
}

void calculate_probs (map<wstring, int>& unigram_counts, map<wstring, float>& unigram_probs, map<wstring, map<wstring, int> >& bigram_counts, map<wstring, map<wstring, float> >& bigram_probs) {
  int types = 0;
  int tokens = 0;
  ofstream output ("modelcounts.txt");
  map<wstring, int>::iterator it1;
  map<wstring, map<wstring, int> >::iterator it2;
  for (it1=unigram_counts.begin(); it1 != unigram_counts.end(); it1++) {
    types++;
    tokens += it1->second;
  }
  for (it1=unigram_counts.begin(); it1 != unigram_counts.end(); it1++)
    unigram_probs[it1->first] = (it1->second + ALPHA)/(tokens + (ALPHA*types));
  map<wstring, int> prev_counts;
  for (it2=bigram_counts.begin(); it2 != bigram_counts.end(); it2++) {
    prev_counts[it2->first] = 0;
    for (it1=it2->second.begin(); it1 != it2->second.end(); it1++)
      prev_counts[it2->first] += it1->second;
    if (true/*prev_counts[it2->first] != unigram_counts[it2->first]*/) {
      output << WChar_to_UTF8(it2->first.c_str()) << "\t" << prev_counts[it2->first] << " " << unigram_counts[it2->first] << endl;
      map<wstring, int>::iterator it3;
      for (it3=bigram_counts[it2->first].begin(); it3 != bigram_counts[it2->first].end(); it3++)
	output << it3->second << "\t" << WChar_to_UTF8(it3->first.c_str()) << endl;
    }
  }
  for (it2=bigram_counts.begin(); it2 != bigram_counts.end(); it2++) {
    for (it1=it2->second.begin(); it1 != it2->second.end(); it1++)
      bigram_probs[it2->first][it1->first] = (bigram_counts[it2->first][it1->first] + (BETA*unigram_probs[it1->first]))/(prev_counts[it2->first] + BETA);
  }
}

void merge_lists (map<wstring, int>& first_probs, map<wstring, int>& second_probs, map<wstring, int>& output) {
  map<wstring, int>::iterator it;
  for (it=first_probs.begin(); it != first_probs.end(); it++)
    output[it->first] = it->second;
  for (it=second_probs.begin(); it != second_probs.end(); it++) {
    output[it->first] += it->second;
  }
}

void merge_lists (map<wstring, map<wstring, int> >& first_probs, map<wstring, map<wstring, int> >& second_probs, map<wstring, map<wstring, int> >& output) {
  map<wstring, map<wstring, int> >::iterator it1;
  map<wstring, int>::iterator it2;
  for (it1=first_probs.begin(); it1 != first_probs.end(); it1++) {
    for (it2=it1->second.begin(); it2 != it1->second.end(); it2++)
      output[it1->first][it2->first] = it2->second;
  }
  for (it1=second_probs.begin(); it1 != second_probs.end(); it1++) {
    for (it2=it1->second.begin(); it2 != it1->second.end(); it2++)
      output[it1->first][it2->first] += it2->second;
  }
}

float find_prob (map<wstring, map<wstring, int> > bigram_counts, map<wstring, int> source_uni_cts, map<wstring, map<wstring, int> > source_bi_cts, map<wstring, float> unigram_probs, map<wstring, map<wstring, float> > bigram_probs) {
  float log_prob = 0;
  map<wstring, map<wstring, int> >::iterator it1;
  map<wstring, int>::iterator it2;
  for (it1=bigram_counts.begin(); it1 != bigram_counts.end(); it1++) {
    for (it2=it1->second.begin(); it2 != it1->second.end(); it2++) {
      if (unigram_probs.count(it2->first) == 0)
	unigram_probs[it2->first] = unigram_probs[UNK];
      if (bigram_probs[it1->first].count(it2->first) == 0)
	bigram_probs[it1->first][it2->first] = (source_bi_cts[it1->first][it2->first] + (BETA*unigram_probs[it2->first]))/(source_uni_cts[it1->first] + BETA);
      log_prob += log (bigram_probs[it1->first][it2->first]);
      if (bigram_probs[it1->first][it2->first] == 0)
	wcout << it1->first << ", " << it2->first << endl;
    }
  }
  return log_prob;
}
