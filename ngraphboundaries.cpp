// groups graphemes into "phonemic" units by Gibbs Sampling
// after Goldwater, Griffiths and Johnson
#include <string>
#include <map>
#include <set>
#include <cmath>
#include "wchar.h"
#include "wordlist.h"
using namespace std;

#define A_NAUGHT 20.
#define TAU 2.
#define P_SHARP .9999999999999999999
#define BD '%'
#define NBD '^'
#define ITERATIONS 10000000

typedef map<string, int> TownStrings;
typedef map<wstring, int> TownWStrings;
typedef map<string, map<string, int> > TownList;

void strdict_to_wstrdict (TownStrings& string_dict, TownWStrings& wstring_dict) {
  TownStrings::iterator it1;
  for (it1=string_dict.begin(); it1 != string_dict.end(); it1++) {
    wstring converted = UTF8_to_WChar(it1->first.c_str());
    wstring bounded;
    wstring::iterator it2;
    for (it2=converted.begin(); it2 != converted.end(); it2++) {
      bounded += *it2;
      bounded += NBD;
    }
    wstring_dict[bounded] = it1->second;
  }
}

void gibbs_sampling (TownWStrings& town_dict) {
  double combined_prob;
  double split_prob;
  map<wstring, int> unigram_counts;
  set<wchar_t> grapheme_set;
  int total_count = 0;
  int word_count = town_dict.size();
  int boundary_count = 0;
  wstring word_array[word_count];
  int x = 0;
  TownWStrings::iterator it1;
  for (it1=town_dict.begin(); it1 != town_dict.end(); it1++) {
    total_count += it1->second;
    word_array[x++] = it1->first;
    wstring pure_word;
    for (int i=0; i < it1->first.length(); i+=2) {
      grapheme_set.insert(it1->first[i]);
      pure_word += it1->first[i];
    }
    unigram_counts[pure_word] = it1->second;
  }
  float inv_grapheme_num = 1./grapheme_set.size();
  srand (time(NULL));
  for (int i=0; i != ITERATIONS; i++) {
    int rand_index = rand() % word_count;
    wstring target_word = word_array[rand_index];
    int target_word_count = town_dict[target_word];
    int target_word_length = target_word.length();
    if (target_word_length > 2) {
      int target_index = (rand() % ((target_word_length-1)/2))*2+1;
      wstring before;
      wstring after;
      bool target_found = false;
      bool boundary_present;
      for (int j=1; j <= target_word_length; j+=2) {
	if (!target_found) {
	  if (j == target_index) {
	    target_found = true;
	    boundary_present = (target_word[j] == BD);
	    before += target_word[j-1];
	  }
	  else if (target_word[j] == BD)
	    before.clear();
	  else
	    before += target_word[j-1];
	}
	else {
	  after += target_word[j-1];
	  if (target_word[j] == BD)
	    break;
	}
      }
      wstring combined = before;
      combined += after;
      int total_count_else = total_count-target_word_count;
      int combined_count_else = unigram_counts[combined];
      int before_count_else = unigram_counts[before];
      int before_length = before.length();
      int after_count_else = unigram_counts[after];
      int after_length = after.length();
      int combined_length = before_length + after_length;
      int boundary_count_else = boundary_count;
      int is_before_after = 0;
      if (!boundary_present) {
	combined_count_else -= target_word_count;
	boundary_count_else -= target_word_count;
      }
      else {
	total_count_else -= target_word_count;
	before_count_else -= target_word_count;
	after_count_else -= target_word_count;
      }
      if (before == after)
	is_before_after++;
      combined_prob = (combined_count_else + (A_NAUGHT*P_SHARP*pow(1-P_SHARP,combined_length-1)*pow(inv_grapheme_num,combined_length)))/(total_count_else + A_NAUGHT);
      split_prob = ((boundary_count + (TAU/2))
		    *(before_count_else + (A_NAUGHT*P_SHARP*pow(1-P_SHARP,before_length-1)*pow(inv_grapheme_num,before_length)))
		    *(after_count_else + is_before_after + (A_NAUGHT*P_SHARP*pow(1-P_SHARP,after_length-1)*pow(inv_grapheme_num,after_length))))
	/((total_count_else + 1 + TAU)
	  *(total_count_else + A_NAUGHT)
	  *(total_count_else + 1 + A_NAUGHT));
      float ratio = combined_prob/split_prob;
      bool no_boundary = false;
      if (rand() < RAND_MAX*(ratio/(ratio+1)))
	no_boundary = true;
      if (no_boundary) {
	if (boundary_present) {
	  town_dict.erase(target_word);
	  target_word[target_index] = NBD;
	  town_dict[target_word] = target_word_count;
	  word_array[rand_index] = target_word;
	  total_count -= target_word_count;
	  if (unigram_counts.count(combined) == 0)
	    unigram_counts[combined] = 0;
	  unigram_counts[combined] += target_word_count;
	  unigram_counts[before] -= target_word_count;
	  unigram_counts[after] -= target_word_count;
	  if (unigram_counts[before] == 0)
	    unigram_counts.erase(before);
	  if (unigram_counts[after] == 0)
	    unigram_counts.erase(after);
	}
      }
      else {
	if (!boundary_present) {
	  town_dict.erase(target_word);
	  target_word[target_index] = BD;
	  town_dict[target_word] = target_word_count;
	  word_array[rand_index] = target_word;
	  total_count += target_word_count;
	  boundary_count += target_word_count;
	  if (unigram_counts.count(before) == 0)
	    unigram_counts[before] = 0;
	  if (unigram_counts.count(after) == 0)
	    unigram_counts[after] = 0;
	  unigram_counts[before] += target_word_count;
	  unigram_counts[after] += target_word_count;
	  unigram_counts[combined] -= target_word_count;
	  if (unigram_counts[combined] == 0)
	    unigram_counts.erase(combined);
	}
      }
      if (i % 100000 == 0) {
	string target_utf8 = WChar_to_UTF8 (target_word.c_str());
	//cout << target_utf8 << endl;
	//cout << split_prob << endl;
	//cout << combined_prob << endl;
	cout << i << endl;
      }
    }
  }
  /*map<wstring, int>::iterator it3;
  ofstream outfile ("unigram-results.txt");
  for (it3=unigram_counts.begin(); it3 != unigram_counts.end(); it3++) {
    if (it3->second > 0)
      outfile << WChar_to_UTF8(it3->first.c_str()) << "\t" << it3->second << endl;
      }*/
}

void printlist (TownList town_dicts, const char* outfile) {
  ofstream output (outfile);
  TownList::iterator it1;
  for (it1=town_dicts.begin(); it1 != town_dicts.end(); it1++) {
    output << "Coordinates:\t" << it1->first << endl;
    TownWStrings town_dict;
    strdict_to_wstrdict (town_dicts[it1->first], town_dict);
    gibbs_sampling (town_dict);
    multimap<int, wstring> inverse_town_dict;
    TownWStrings::iterator it2;
    for (it2=town_dict.begin(); it2 != town_dict.end(); it2++)
      inverse_town_dict.insert(pair<int, wstring> (it2->second, it2->first));
    multimap<int, wstring>::reverse_iterator it3;
    for (it3=inverse_town_dict.rbegin(); it3 != inverse_town_dict.rend(); it3++)
      output << it3->first << '\t' << WChar_to_UTF8(it3->second.c_str()) << endl;
    output << '\n';
  }
}

int main (int argc, char* argv[]) {
  TownList town_dicts;
  wordlist (town_dicts, argv[1]);
  printlist (town_dicts, argv[2]);
  return 0;
}
