#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <list>
#include <queue>
#include "compare-with-lm2.h"
using namespace std;


#define GAMMA 100
#define DELTA 1.0
#define EPSILON 10.0
#define ITER 100000
#define INCR 500
#define DIST_POWER 1
#define BIGRAM_POWER .5
#define MIN_COUNT 5

typedef unsigned char enc_town;
typedef int enc_word;
typedef int cog_class;

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
// roughly captures the phonetic distance between pairs of characters
short** char_distances;
// stores the sum of the distances from each sound between each pair of towns
float*** distance_sum;

// maps words to their encoding numbers
map<wstring, enc_word> word_encoding;
// maps numbers to the words they represent
vector<wstring> word_decoding;
// a counter for encoding words
enc_word word_enc_ct = 1;
// each town's list of words
vector<enc_word>* word_lists;
// the number of word types in each town
enc_word* word_counts;
// the (average reduced) frequency across all of Igbo for each word
vector<float> total_word_counts;
// stores the one change (if it is so) between pairs of words
map<enc_word, map<enc_word, enc_change> > word_pairs;
// stores the adjustments between pairs of words
map<enc_word, map<enc_word, pair<int, map<enc_change, int> > > > word_adjusts;
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

// first word counts by cognate class
map<cog_class, int> class_firsts;
// first word counts by town
map<enc_word, int>* town_firsts;
// bigram counts by cognate class
map<cog_class, map<cog_class, int> > class_bigrams;
// bigram counts by cognate class (mapped in reverse order)
map<cog_class, map<cog_class, int> > rev_class_bigrams;
// bigram counts by town
map<enc_word, map<enc_word, int> >* town_bigrams;
// bigram counts by town (mapped in reverse order)
map<enc_word, map<enc_word, int> >* reverse_bigrams;

enum state {NONE, DIFF, SAME};

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

//writes bigram counts to outFile
void write_bigrams_to_file(map<Town, vector<wstring> >& town_vectors, char* outFile)
{
  ofstream output (outFile);
  map<Town, vector<wstring> >::iterator i;
  //i = town_vectors.begin();
  //int loops=0;
  for(i = town_vectors.begin(); i != town_vectors.end(); i++)
  //while (loops < 10)
  {
	//i++;
	//loops++;
	Town thisTown = i->first;
	//cout << "Coordinates:\t" << thisTown.first << "," << thisTown.second << "\n";
	vector<wstring> words = i->second;
	map<pair<wstring,wstring>, int > towns_map;
	wstring first_word;
	wstring second_word;
	vector<wstring>::iterator j;
	for(j = words.begin(); j != words.end(); j++)
	{
	    if(j == words.begin())
		first_word=*j;
	    else
	    {
		second_word=first_word;
		first_word=*j;
		pair<wstring,wstring> bigram;
		bigram.first=first_word;
		bigram.second=second_word;
		if(towns_map.find(bigram)==towns_map.end())
		{
		    towns_map[bigram]=1;
		}
		else
		{
		    towns_map[bigram]=towns_map[bigram]+1;
		}
		/*if(first_word.compare(parse(UTF8_to_WChar(",")))==0&&second_word.compare(parse (UTF8_to_WChar("ya")))==0)
		{
		    cout << towns_map[bigram] << "\n";
		}*/
	    }
	}
	output << "Coordinates:\t" << thisTown.first << "," << thisTown.second << endl;
	multimap<int, pair<wstring,wstring> > inverse_town_dict;
	map<pair<wstring,wstring>, int>::iterator it;
	for (it=towns_map.begin(); it != towns_map.end(); it++)
	    inverse_town_dict.insert(pair<int, pair<wstring,wstring> > (it->second, it->first));
	multimap<int, pair<wstring,wstring> >::reverse_iterator it3;
	for (it3=inverse_town_dict.rbegin(); it3 != inverse_town_dict.rend(); it3++)
	    output << it3->first << '\t' << WChar_to_UTF8(it3->second.second.c_str())<< '\t' << WChar_to_UTF8(it3->second.first.c_str()) << endl;
	output << '\n';
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
void gather_lists (map<enc_town, map<wstring, float> >& town_dicts, vector<set<enc_char> >& chars, const char* listfile) {
  ifstream lists (listfile);
  Word new_word;
  word_decoding.push_back(L"");
  total_word_counts.push_back(0);
  split_dict.push_back (new_word);
  bool header = true;
  enc_town curr_town;
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
	if (word != "") {
	  wstring wide_word = UTF8_to_WChar (word);
	  Word word_chars = split_word (wide_word);
	  Word::iterator it;
	  for (it=word_chars.begin(); it != word_chars.end(); it++)
	    chars[curr_town].insert (*it);
	  town_dicts[curr_town][wide_word] = arf;
	  if (word_encoding.count(wide_word) == 0) {
	    word_encoding[wide_word] = word_enc_ct++;
	    word_decoding.push_back (wide_word);
	    total_word_counts.push_back (arf);
	    split_dict.push_back (word_chars);
	  }
	  else
	    total_word_counts[word_encoding[wide_word]] += arf;
	}
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

// brings the list of bigram counts into the proper data structure
// ONLY CALL AFTER GATHER_LISTS
void gather_bigrams (map<enc_town, map<wstring, float> >& town_dicts, const char* listfile) {
  ifstream lists (listfile);
  bool header = true;
  enc_town curr_town = 0;
  town_bigrams = new map<enc_word, map<enc_word, int> > [town_enc_ct];
  reverse_bigrams = new map<enc_word, map<enc_word, int> > [town_enc_ct];
  town_firsts = new map<enc_word, int> [town_enc_ct];
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
      header = false;
    }
    else {
      if (lists.peek() == '\n') {
	header = true;
	lists.ignore (15,'\n');
	++curr_town;
      }
      else {
	char count_string [6];
	int count;
	lists.getline (count_string, 256, '\t');
	stringstream convert (count_string);
	convert >> count;
	convert.flush();
	char first_word [500];
	lists.getline (first_word, 500, '\t');
	char second_word [500];
	lists.getline (second_word, 500);
	wstring wide_first_word = UTF8_to_WChar (first_word);
	wstring wide_second_word = UTF8_to_WChar (second_word);
	enc_word first_encoded = word_encoding[wide_first_word];
	enc_word second_encoded = word_encoding[wide_second_word];
	if (total_word_counts[first_encoded] < MIN_COUNT)
	  first_encoded = 0;
	if (total_word_counts[second_encoded] < MIN_COUNT)
	  second_encoded = 0;
	//map<enc_word, int>* first_dict = &town_bigrams[curr_town][first_encoded];
	if (town_bigrams[curr_town][first_encoded].count(second_encoded) == 0)
	  town_bigrams[curr_town][first_encoded][second_encoded] = 0;
	town_bigrams[curr_town][first_encoded][second_encoded] += count;
	//map<enc_word, int>* second_dict = &reverse_bigrams[curr_town][second_encoded];
	if (reverse_bigrams[curr_town][second_encoded].count(first_encoded) == 0)
	  reverse_bigrams[curr_town][second_encoded][first_encoded] = 0;
	reverse_bigrams[curr_town][second_encoded][first_encoded] += count;
	if (town_firsts[curr_town].count(first_encoded) == 0)
	  town_firsts[curr_town][first_encoded] = 0;
	town_firsts[curr_town][first_encoded] += count;
	//wcout << curr_town << "\t" << first_word << "\t" << first_encoded << "\t" << second_word << "\t" << second_encoded << "\t" << town_bigrams[curr_town][first_encoded][second_encoded] << "\t" << town_firsts[curr_town][first_encoded] << endl;
      }
    }
  }
}


//void char_distance_calc (const char* charfile) {
void char_distance_calc (vector<set<enc_char> >& char_vector, const char* charfile) {

  char_distances = new short* [enc_ct];
  for (int i=0; i < enc_ct; i++) {
    char_distances[i] = new short [enc_ct];
  }
  short features [enc_ct][7];
  ifstream chars (charfile);
  while (chars.good()) {
    enc_char curr_enc_char;
    char char_string [5];
    chars.getline (char_string, 50, '\t');
    stringstream char_stream (char_string);
    char_stream >> curr_enc_char;
    char_stream.flush();
    chars.getline (char_string, 50, '\t');
    wstring curr_char = UTF8_to_WChar (char_string);
    for (int i=0; i < 7; i++) {
      short curr_feature;
      features[curr_enc_char][i] = chars.get()-48;
    }
    //wcout << curr_enc_char << "\t" << features[curr_enc_char][0] << "," << features[curr_enc_char][1] << "," << features[curr_enc_char][2] << "," << features[curr_enc_char][3] << "," << features[curr_enc_char][4] << "," << features[curr_enc_char][5] << "," << features[curr_enc_char][6] << endl;
    chars.ignore (1);
    if (chars.peek() == -1)
      chars.ignore (1);
  }
  char_distances[0][0] = 1;
  for (int i=1; i < enc_ct; i++) {
    if (features[i][0] == 0) {
      char_distances[0][i] = pow(3,6);
      char_distances[i][0] = char_distances[0][i];
    }
    else if (features[i][0] == 1) {
      char_distances[0][i] = 4*3*2;
      char_distances[i][0] = char_distances[0][i];
    }
    else {
      char_distances[0][i] = 2*2*2;
      char_distances[i][0] = char_distances[0][i];
    }
    for (int j=i; j < enc_ct; j++) {
      if (features[i][0] == 0) {
	if (features[j][0] == 0) {
	  char_distances[i][j] = 1;
	  char_distances[j][i] = char_distances[i][j];
	}
	else {
	  char_distances[i][j] = pow(3,6);
	  char_distances[j][i] = char_distances[i][j];
	}
      }
      else if (features[i][0] == 1) {	
	if (features[j][0] == 1) {
	  char_distances[i][j] = (abs(features[j][1]-features[i][1])+1)*(abs(features[j][2]-features[i][2])+1)*(abs(features[j][3]-features[i][3])+1)*(abs(features[j][4]-features[i][4])+1);
	  char_distances[j][i] = char_distances[i][j];
	}
	else {
	  char_distances[i][j] = pow(3,5);
	  char_distances[j][i] = char_distances[i][j];
	}
      }
      else {
	if (features[j][0] == 1) {
	  char_distances[i][j] = pow(3,5);
	  char_distances[j][i] = char_distances[i][j];
	}
	else {
	  char_distances[i][j] = (abs(features[j][4]-features[i][4])+1)*(abs(features[j][5]-features[i][5])+1)*(abs(features[j][6]-features[i][6])+1);
	  char_distances[j][i] = char_distances[i][j];
	}
      }
    }
  }
  vector<set<enc_char> >::iterator it;
  for (it=char_vector.begin(); it != char_vector.end(); it++)
    it->insert (0);
  set<enc_char>::iterator it1;
  set<enc_char>::iterator it2;
  distance_sum = new float** [town_enc_ct];
  for (int i=0; i < town_enc_ct; i++) {
    distance_sum[i] = new float* [town_enc_ct];
    for (int j=0; j < town_enc_ct; j++) {
      distance_sum[i][j] = new float [enc_ct];
      for (it1=char_vector[i].begin(); it1 != char_vector[i].end(); it1++) {
	distance_sum[i][j][*it1] = 0;
	for (it2=char_vector[j].begin(); it2 != char_vector[j].end(); it2++)
	  distance_sum[i][j][*it1] += 1./char_distances[*it1][*it2];
      }
    }
  }
}

enc_change invert_change (enc_change change) {
  enc_char first = change / 512;
  enc_char second = change % 512;
  return second*512 + first;
}

/*void print_arfs (map<Town, map<wstring, float> >& town_dicts, const char* outfile) {
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
}*/

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
  int c = (int) second_arf;
  int n = (int) second_arf_total;
  int i = 0;
  double to_return = 1;
  while (++i <= c)
    to_return *= first_prob*(n-i)/i;
  while (++i <= n)
    to_return *= 1-first_prob;
  return pow(to_return, DIST_POWER);
}

void add_class (cog_class to_use) {  
  cognate_classes[to_use] = new enc_word [town_enc_ct];
  class_counts[to_use] = 0;
  class_firsts[to_use] = 0;
  for (int i=0; i < town_enc_ct; i++)
    cognate_classes[to_use][i] = -1;
}

void remove_class (cog_class to_remove) {
  cognate_classes.erase (to_remove);
  class_counts.erase (to_remove);
  class_firsts.erase (to_remove);
  empty_classes.push (to_remove);
  class_bigrams.erase (to_remove);
  rev_class_bigrams.erase (to_remove);
  map<cog_class, map<cog_class, int> >::iterator it;
  for (it=class_bigrams.begin(); it != class_bigrams.end(); it++)
    it->second.erase (to_remove);
  for (it=rev_class_bigrams.begin(); it != rev_class_bigrams.end(); it++)
    it->second.erase (to_remove);
}

// returns the correspondence between two words if they're a distance of 1 away and 0 otherwise
enc_change find_change (enc_word first_word, enc_word second_word) {  
  //if (first_word == second_word)
  //wcout << "identical words: " << word_decoding[first_word] << ", " << word_decoding[second_word] << endl;
  enc_change change = 0;
  if (word_pairs[first_word].count(second_word) == 0) {
    if (one_change(split_dict[first_word], split_dict[second_word], change)) {
      word_pairs[first_word][second_word] = change;
      word_pairs[second_word][first_word] = invert_change(change);
    }
    else {
      word_pairs[first_word][second_word] = 0;
      word_pairs[second_word][first_word] = 0;
    }
  }
  else
    change = word_pairs[first_word][second_word];
  return change;
}

// returns a list of letter correspondences between two given words
map<enc_change, int> count_adjust (enc_town first_town, enc_town second_town, enc_word first_word, enc_word second_word, enc_change change) {
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
    return first_counts;
  }
  // if the words are identical, we're good
  else if (first_word == second_word) {
    /*wcout << word_decoding[first_word] << endl;
    map<enc_change, int>::iterator it;
    for (it=first_counts.begin(); it != first_counts.end(); it++)
      wcout << represent_change (it->first) << "\t" << it->second << endl;*/
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
    return to_return;
  }
}

map<enc_change, int> find_adjust (enc_town first_town, enc_town second_town, enc_word first_word, enc_word second_word, enc_change change) {
  if (word_adjusts[first_word].count(second_word) == 0) {
    word_adjusts[first_word][second_word] = pair<int, map<enc_change, int> > (0, count_adjust (first_town, second_town, first_word, second_word, change));
    if (first_word != second_word) {
      map<enc_change, int> newmap;
      word_adjusts[second_word][first_word] = pair<int, map<enc_change, int> > (0, newmap);
      map<enc_change, int>::iterator it;
      for (it=word_adjusts[first_word][second_word].second.begin(); it != word_adjusts[first_word][second_word].second.end(); it++)
	word_adjusts[second_word][first_word].second[invert_change(it->first)] = it->second;
    }
  }
  return word_adjusts[first_word][second_word].second;
}

void add_counts (enc_town curr_town, enc_word curr_word, float curr_count, cog_class new_class, bool initial) {
  cognates[curr_town][curr_word] = new_class;
  cognate_classes[new_class][curr_town] = curr_word;
  class_counts[new_class] += curr_count;
  map<enc_word, int>::iterator it1;
  for (it1=town_bigrams[curr_town][curr_word].begin(); it1 != town_bigrams[curr_town][curr_word].end(); it1++) {
    cog_class second_class = cognates[curr_town][it1->first];
    if (class_bigrams[new_class].count(second_class) == 0) {
      class_bigrams[new_class][second_class] = 0;
      rev_class_bigrams[second_class][new_class] = 0;
    }
    class_bigrams[new_class][second_class] += it1->second;
    rev_class_bigrams[second_class][new_class] += it1->second;
    class_firsts[new_class] += it1->second;
  }
  //class_firsts[new_class] += town_firsts[curr_town][curr_word];
  for (it1=reverse_bigrams[curr_town][curr_word].begin(); it1 != reverse_bigrams[curr_town][curr_word].end(); it1++) {
    cog_class first_class = cognates[curr_town][it1->first];
    if ((!initial || first_class == 0) && first_class != new_class) {
      if (class_bigrams[first_class].count(new_class) == 0) {
	class_bigrams[first_class][new_class] = 0;
	rev_class_bigrams[new_class][first_class] = 0;
      }
      class_bigrams[first_class][new_class] += it1->second;
      rev_class_bigrams[new_class][first_class] += it1->second;
      if (initial && first_class == 0)
	class_firsts[first_class] += it1->second;
    }
  }
  for (int i=0; i < town_enc_ct; i++) {
    enc_word neighbor_word = cognate_classes[new_class][i];
    if (neighbor_word >= 0) {
      enc_change change = find_change (neighbor_word, curr_word);
      map<enc_change, int> to_add = find_adjust (i, curr_town, neighbor_word, curr_word, change);
      ++word_adjusts[neighbor_word][curr_word].first;
      if (curr_word != neighbor_word)
	++word_adjusts[curr_word][neighbor_word].first;
      map<enc_change, int>::iterator it;
      for (it=to_add.begin(); it != to_add.end(); it++) {
	corr_counts[i][curr_town][it->first] += it->second;
	corr_counts[curr_town][i][invert_change(it->first)] += it->second;
	corr_totals[i][curr_town][it->first/512] += it->second;
	corr_totals[curr_town][i][it->first%512] += it->second;
      }
    }
  }
}

void remove_counts (enc_town curr_town, enc_word curr_word, float curr_count, cog_class old_class) {
  cognate_classes[old_class][curr_town] = -1;
  class_counts[old_class] -= curr_count;
  map<enc_word, int>::iterator it1;
  for (it1=town_bigrams[curr_town][curr_word].begin(); it1 != town_bigrams[curr_town][curr_word].end(); it1++) {
    cog_class second_class = cognates[curr_town][it1->first];
    class_bigrams[old_class][second_class] -= it1->second;
    rev_class_bigrams[second_class][old_class] -= it1->second;
    if (class_bigrams[old_class][second_class] == 0) {
      class_bigrams[old_class].erase (second_class);
      rev_class_bigrams[second_class].erase (old_class);
    }
    else if (class_bigrams[old_class][second_class] < 0)
      wcout << "f " << curr_word << "," << it1->first << "\t" << old_class << "," << second_class << "\t" << rev_class_bigrams[second_class][old_class] << "\t" << it1->second << endl;
    class_firsts[old_class] -= it1->second;
  }
  for (it1=reverse_bigrams[curr_town][curr_word].begin(); it1 != reverse_bigrams[curr_town][curr_word].end(); it1++) {
    cog_class first_class = cognates[curr_town][it1->first];
    if (first_class != old_class) {
      class_bigrams[first_class][old_class] -= it1->second;
      rev_class_bigrams[old_class][first_class] -= it1->second;
      if (class_bigrams[first_class][old_class] == 0) {
	class_bigrams[first_class].erase (old_class);
	rev_class_bigrams[old_class].erase (first_class);
      }
      else if (class_bigrams[first_class][old_class] < 0)
	wcout << "r " << curr_word << "," << it1->first << "\t" << old_class << "," << first_class << "\t" << rev_class_bigrams[old_class][first_class] << "\t" << it1->second << endl;
      //class_firsts[first_class] -= it1->second;
    }
  }
  //class_firsts[old_class] -= town_firsts[curr_town][curr_word];
  for (int i=0; i < town_enc_ct; i++) {
    enc_word neighbor_word = cognate_classes[old_class][i];
    if (neighbor_word >= 0) {
      enc_change change = find_change (neighbor_word, curr_word);
      map<enc_change, int> to_remove = find_adjust (i, curr_town, neighbor_word, curr_word, change);
      --word_adjusts[neighbor_word][curr_word].first;
      --word_adjusts[curr_word][neighbor_word].first;
      if (word_adjusts[neighbor_word][curr_word].first == 0) {
	word_adjusts[neighbor_word].erase (curr_word);
	word_adjusts[curr_word].erase (neighbor_word);
      }
      map<enc_change, int>::iterator it;
      for (it=to_remove.begin(); it != to_remove.end(); it++) {
	corr_counts[i][curr_town][it->first] -= it->second;
	if (corr_counts[curr_town][i][it->first] < 0)
	  wcout << i << "\t" << curr_town << "\t" << neighbor_word << "\t" << word_decoding[neighbor_word] << "\t" << curr_word << "\t" << word_decoding[curr_word] << "\t" << endl;
	corr_counts[curr_town][i][invert_change(it->first)] -= it->second;
	corr_totals[i][curr_town][it->first/512] -= it->second;
	corr_totals[curr_town][i][it->first%512] -= it->second;
      }
    }
  }
  cognates[curr_town][curr_word] = 0;
}

// TEST THIS
double word_match_prob (enc_town first_town, enc_town second_town, enc_word first_word, enc_word second_word, bool adding_word, enc_change change) {
  double prob_to_return = 1;
  map<enc_change, int> change_counts = find_adjust (first_town, second_town, first_word, second_word, change);
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
      prob_to_return *= pow((a_to_b + GAMMA/char_distances[it->first/512][it->first%512]) / (a_to_anything + distance_sum[first_town][second_town][it->first/512]*GAMMA), it->second);
      //wcout << first_town << "," << second_town << "\t" << represent_change(it->first) << "\t" << a_to_anything << "," << a_to_b << "\t" << char_distances[it->first/512][it->first%512] << "\t" << distance_sum[first_town][second_town][it->first/512] << endl;
    }
  }
  //wcout << word_decoding[first_word] << "\t" << word_decoding[second_word] << "\t" << prob_to_return << endl;
  return prob_to_return;
}

double total_log_prob2 (map<enc_town, map<wstring, float> >& town_dicts) {
  double bin_prob = 0;
  double wm_prob = 0;
  double bgm_prob = 0;
  double to_return = 0;
  int class_count = cognate_classes.size();
  map<cog_class, enc_word*>::iterator it1;
  vector<enc_town>::iterator it2;
  int k=0;
  for (it1=cognate_classes.begin(); it1 != cognate_classes.end(); it1++) {
    k++;
    double exp_prob = class_counts[it1->first]/arf_total;
    for (int i=0; i < town_enc_ct; i++) {
      enc_word curr_word = it1->second[i];
      if (curr_word >= 0) {
	float curr_count = town_dicts[i][word_decoding[curr_word]];
	double curr_bin_prob = log(binomial_prob(curr_count, arf_totals[i], exp_prob));
	to_return += curr_bin_prob;
	bin_prob += curr_bin_prob;
	for (it2=neighbors[i].begin(); it2 != neighbors[i].end(); it2++) {
	  enc_word new_neighbor_word = it1->second[*it2];
	  if (new_neighbor_word >= 0) {
	    double curr_wm_prob = log(word_match_prob(i, *it2, curr_word, new_neighbor_word, false, find_change(curr_word, new_neighbor_word)));
	    to_return += curr_wm_prob;
	    wm_prob += curr_wm_prob;
	    // wcout << it1->first << "\t" << i << "\t" << curr_word << "\t" << *it2 << "\t" << new_neighbor_word << "\t" << word_match_prob(i, *it2, curr_word, new_neighbor_word, false, find_change(curr_word, new_neighbor_word)) << "\t" << binomial_prob(curr_count, arf_totals[i], exp_prob) << "\t" << to_return << endl;
	  }
	}
      }
      else {
	to_return += log(pow(pow(1-exp_prob, arf_totals[i]), DIST_POWER));
	bin_prob += log(pow(pow(1-exp_prob, arf_totals[i]), DIST_POWER));
      }
    }
    if (isnan(to_return) != 0 || to_return < -9999999999999999999.)
      break;
  }
  map<cog_class, map<cog_class, int> >::iterator it3;
  map<cog_class, int>::iterator it4;
  for (it3=rev_class_bigrams.begin(); it3 != rev_class_bigrams.end(); it3++) {
    for (it4=it3->second.begin(); it4 != it3->second.end(); it4++) {
      //wcout << it3->first << "\t" << it4->first << "\t" << it4->second << "\t" << class_counts[it3->first] << "\t" << class_firsts[it4->first] << "\t" << class_counts[it4->first] << "\t" << to_return << endl;
      for (int i=0; i < it4->second; i++) {
	double curr_bgm_prob = log((it4->second + ((class_counts[it3->first]+EPSILON)/(arf_total+class_count*EPSILON))*DELTA)/(class_firsts[it4->first] + DELTA));
	to_return += curr_bgm_prob;
	bgm_prob += curr_bgm_prob;
      }
      //wcout << to_return << endl;
    if (isnan(to_return) != 0 || to_return < -9999999999999999999.)
      return to_return;
    }
    //wcout << it3->first << "\t" << to_return << endl;
  }
  wcout << bin_prob << "\t" << wm_prob << "\t" << bgm_prob << endl;
  return to_return;
}

double total_log_prob (map<enc_town, map<wstring, float> >& town_dicts) {
  double to_return = 0;
  int class_count = cognate_classes.size();
  map<cog_class, enc_word*>::iterator it1;
  vector<enc_town>::iterator it2;
  int k=0;
  for (it1=cognate_classes.begin(); it1 != cognate_classes.end(); it1++) {
    k++;
    double exp_prob = class_counts[it1->first]/arf_total;
    for (int i=0; i < town_enc_ct; i++) {
      enc_word curr_word = it1->second[i];
      if (curr_word >= 0) {
	float curr_count = town_dicts[i][word_decoding[curr_word]];
	to_return += log(binomial_prob(curr_count, arf_totals[i], exp_prob));
	for (it2=neighbors[i].begin(); it2 != neighbors[i].end(); it2++) {
	  enc_word new_neighbor_word = it1->second[*it2];
	  if (new_neighbor_word >= 0) {
	    to_return += log(word_match_prob(i, *it2, curr_word, new_neighbor_word, false, find_change(curr_word, new_neighbor_word)));
	    // wcout << it1->first << "\t" << i << "\t" << curr_word << "\t" << *it2 << "\t" << new_neighbor_word << "\t" << word_match_prob(i, *it2, curr_word, new_neighbor_word, false, find_change(curr_word, new_neighbor_word)) << "\t" << binomial_prob(curr_count, arf_totals[i], exp_prob) << "\t" << to_return << endl;
	  }
	}
      }
      else
	to_return += log(pow(pow(1-exp_prob, arf_totals[i]), DIST_POWER));
    }
    if (isnan(to_return) != 0 || to_return < -9999999999999999999.)
      break;
  }
  map<cog_class, map<cog_class, int> >::iterator it3;
  map<cog_class, int>::iterator it4;
  for (it3=rev_class_bigrams.begin(); it3 != rev_class_bigrams.end(); it3++) {
    for (it4=it3->second.begin(); it4 != it3->second.end(); it4++) {
      //wcout << it3->first << "\t" << it4->first << "\t" << it4->second << "\t" << class_counts[it3->first] << "\t" << class_firsts[it4->first] << "\t" << class_counts[it4->first] << "\t" << to_return << endl;
      for (int i=0; i < it4->second; i++)
	to_return += log((it4->second + ((class_counts[it3->first]+EPSILON)/(arf_total+class_count*EPSILON))*DELTA)/(class_firsts[it4->first] + DELTA));
      //wcout << to_return << endl;
    if (isnan(to_return) != 0 || to_return < -9999999999999999999.)
      return to_return;
    }
    //wcout << it3->first << "\t" << to_return << endl;
  }
  return to_return;
}

double find_change_prob (map<enc_town, map<wstring, float> >& town_dicts, enc_town curr_town, enc_word curr_word_enc, float curr_count, cog_class curr_class, cog_class new_class, int class_count) {
  // start with the language model component
  double to_return = 1;
  double old_exp_prob = (class_counts[new_class])/(arf_total);
  double new_exp_prob = (class_counts[new_class] + curr_count)/(arf_total);
  double old_dist_prob = pow(pow(1-old_exp_prob, arf_totals[curr_town]), DIST_POWER);
  double new_dist_prob = binomial_prob (curr_count, arf_totals[curr_town], new_exp_prob);
  to_return = new_dist_prob/old_dist_prob;
  for (int i=0; i<town_enc_ct; i++) {
    if (i != curr_town) {
      enc_word word_from_neighbor = cognate_classes[new_class][i];
      if (word_from_neighbor >= 0) {
	float count = town_dicts[i][word_decoding[word_from_neighbor]];
	old_dist_prob = binomial_prob (count, arf_totals[i], old_exp_prob);
	new_dist_prob = binomial_prob (count, arf_totals[i], new_exp_prob);
	to_return *= new_dist_prob/old_dist_prob;
      }
      else {
	old_dist_prob = pow(pow(1-old_exp_prob, arf_totals[i]), DIST_POWER);
	new_dist_prob = pow(pow(1-new_exp_prob, arf_totals[i]), DIST_POWER);
	to_return *= new_dist_prob/old_dist_prob;
      }
    }
  }
  double bigram_prob = 0;
  double wm_prob = 0;
  vector<enc_town>::iterator it;
  map<cog_class, int>::iterator it5;
  map<enc_word, cog_class>::iterator it6;
  add_counts (curr_town, curr_word_enc, curr_count, new_class, false);
  for (it5=class_bigrams[new_class].begin(); it5 != class_bigrams[new_class].end(); it5++) {
    for (int i=0; i < it5->second; i++)
      bigram_prob += log((it5->second + ((class_counts[it5->first]+EPSILON)/(arf_total+class_count*EPSILON))*DELTA)/(class_firsts[new_class] + DELTA));
  }
  for (it5=rev_class_bigrams[new_class].begin(); it5 != rev_class_bigrams[new_class].end(); it5++) {
    if (it5->first != new_class) {
      for (int i=0; i < it5->second; i++)
	bigram_prob += log((it5->second + ((class_counts[new_class]+EPSILON)/(arf_total+class_count*EPSILON))*DELTA)/(class_firsts[it5->first] + DELTA));
    }
  }
  for (it6=cognates[curr_town].begin(); it6 != cognates[curr_town].end(); it6++) {
    if (it6->second > 0) {
      for (it=neighbors[curr_town].begin(); it != neighbors[curr_town].end(); it++) {
	enc_word new_neighbor_word = cognate_classes[it6->second][*it];
	if (new_neighbor_word >= 0) {
	  wm_prob += log(word_match_prob(curr_town, *it, it6->first, new_neighbor_word, false, find_change(it6->first, new_neighbor_word)));
	  wm_prob += log(word_match_prob(*it, curr_town, new_neighbor_word, it6->first, false, find_change(new_neighbor_word, it6->first)));
	}
      }
    }
  }
  remove_counts (curr_town, curr_word_enc, curr_count, new_class);
  for (it5=class_bigrams[new_class].begin(); it5 != class_bigrams[new_class].end(); it5++) {
    for (int i=0; i < it5->second; i++)
      bigram_prob -= log((it5->second + ((class_counts[it5->first]+EPSILON)/(arf_total+class_count*EPSILON))*DELTA)/(class_firsts[new_class] + DELTA));
  }
  for (it5=rev_class_bigrams[new_class].begin(); it5 != rev_class_bigrams[new_class].end(); it5++) {
    if (it5->first != new_class) {
      for (int i=0; i < it5->second; i++)
	bigram_prob -= log((it5->second + ((class_counts[new_class]+EPSILON)/(arf_total+class_count*EPSILON))*DELTA)/(class_firsts[it5->first] + DELTA));
    }
  }
  for (it6=cognates[curr_town].begin(); it6 != cognates[curr_town].end(); it6++) {
    if (it6->second > 0) {
      for (it=neighbors[curr_town].begin(); it != neighbors[curr_town].end(); it++) {
	enc_word new_neighbor_word = cognate_classes[it6->second][*it];
	if (new_neighbor_word >= 0) {
	  wm_prob -= log(word_match_prob(curr_town, *it, it6->first, new_neighbor_word, false, find_change(it6->first, new_neighbor_word)));
	  wm_prob -= log(word_match_prob(*it, curr_town, new_neighbor_word, it6->first, false, find_change(new_neighbor_word, it6->first)));
	}
      }
    }
  }
  return to_return * exp (bigram_prob) * exp (wm_prob);
}

void find_cognates (map<enc_town, map<wstring, float> >& town_dicts, const char* out1, const char* out2, const char* initfile) {
  neighbors = new vector<enc_town> [town_enc_ct];
  cognates = new map<enc_word, cog_class> [town_enc_ct];
  word_lists = new vector<enc_word> [town_enc_ct];
  word_counts = new enc_word [town_enc_ct];
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
  // if a word appears frequently enough across Igbo, add it to the word lists of the towns in which it appears
  map<enc_town, map<wstring, float> >::iterator it1;
  for (it1=town_dicts.begin(); it1 != town_dicts.end(); it1++) {
    neighbors[it1->first] = find_neighbors (town_dicts, it1->first);
    map<wstring, float>::iterator it2;
    for (it2=it1->second.begin(); it2 != it1->second.end(); it2++) {
      if (total_word_counts[word_encoding[it2->first]] >= MIN_COUNT) {
	word_lists[it1->first].push_back (word_encoding[it2->first]);
	++word_counts[it1->first];
      }
      else
	class_counts[0] += it2->second;
    }
  }
  // this part is commented out when we already have its results stored to file (see below)
  for (int i=0; i < town_enc_ct; i++) {
    for (int j=0; j < word_counts[i]; j++) {
      add_class (cog_class_ct);
      /*cog_class curr_class = cognates[i][word_lists[i][j]];
	if (class_counts[curr_class] == 0 || class_firsts[curr_class] == 0)
	wcout << curr_class << "\t" << i << "\t" << word_lists[i][j] << "\t" << class_counts[curr_class] << "\t" << class_firsts[curr_class] << "\t" << word_decoding[word_lists[i][j]] << endl;*/
      //cognate_classes[cog_class_ct][i] = word_lists[i][j];
      //class_counts[cog_class_ct] += town_dicts[i][word_decoding[word_lists[i][j]]];
      cognates[i][word_lists[i][j]] = cog_class_ct++;
    }
  }
  for (int i=0; i < town_enc_ct; i++) {
    map<enc_word, cog_class>::iterator it;
    int l=0;
    for (it=cognates[i].begin(); it != cognates[i].end(); it++) {
      add_counts(i, it->first, town_dicts[i][word_decoding[it->first]], it->second, true);
      /*map<enc_word, int>::iterator it1;
      for (it1=town_bigrams[i][it->first].begin(); it1 != town_bigrams[i][it->first].end(); it1++) {
	if (total_word_counts[it1->first] >= MIN_COUNT) {
	  cog_class second_class = cognates[i][it1->first];
	  if (rev_class_bigrams[second_class].count(it->second) == 0)
	    rev_class_bigrams[second_class][it->second] = 0;
	  rev_class_bigrams[second_class][it->second] += it1->second;
	}
	}*/
    }
  }
  /*for (int i=0; i < town_enc_ct; i++) {
    map<enc_word, map<enc_word, int> >::iterator it4;
    for (it4=town_bigrams[i].begin(); it4 != town_bigrams[i].end(); it4++) {
      map<cog_class, int>* first_dict = &rev_class_bigrams[cognates[i][it4->first]];
      map<enc_word, int>::iterator it5;
      for (it5=it4->second.begin(); it5 != it4->second.end(); it5++) {
	cog_class second_class = cognates[i][it5->first];
	if (first_dict->count(second_class) == 0)
	  (*first_dict)[second_class] = 0;
	(*first_dict)[second_class] += it5->second;
      }
    }
    }*/
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
	    //wcout << cognate_classes[cognates[*it2][it1->first]][i] <<"\n";
	    if (neighbor_dict.count(word_decoding[it1->first]) == 1 && cognate_classes[cognates[*it2][it1->first]][i] == -1) {
	      best_match = it1->first;
	      best_class = cognates[*it2][it1->first];
	      break;
	    }
	    /*for (it3=cognates[*it2].begin(); it3 != cognates[*it2].end(); it3++) {
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
	      }*/
	  }
	}
	cog_class old_class = it1->second;
	if (best_match >= 0) {
	  if (it1->second != best_class) {
	    remove_counts (i, it1->first, word_count, it1->second);
	    remove_class (old_class);
	    //it1->second = best_class;
	    add_counts (i, it1->first, word_count, best_class, false);
	  }
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
	add_counts (town, word, town_dicts[town][word_decoding[word]], cog_class_ct);
      }
    }
  }
  initial.close();*/
  // Gibbs sampling process
  srand (0);
  int iter = 0;
  while (iter < ITER) {
    if (iter % INCR == 0) {
      wcout << iter << "\t" << total_log_prob(town_dicts) << "\t" << cognate_classes.size() << endl;
    }
    ++iter;
    // choose a random word in a random town
    enc_town curr_town = rand() % town_enc_ct; 
    enc_word curr_word_enc = word_lists[curr_town][rand() % word_counts[curr_town]];
    wstring curr_word = word_decoding[curr_word_enc];
    cog_class curr_class = cognates[curr_town][curr_word_enc];
    double class_probs [cog_class_ct];
    double total_prob = 0;
    int class_count = cognate_classes.size();
    for (int i=0; i < cog_class_ct; i++)
      class_probs[i] = 0;
    //wcout << curr_word_enc << "\t" << curr_word << "\t" << curr_town << "\t" << curr_class << endl;
    float curr_count = town_dicts[curr_town][curr_word];
    // start by removing word from class
    remove_counts (curr_town, curr_word_enc, curr_count, curr_class);
    bool singleton = false;
    cog_class to_create;
    if (class_counts[curr_class] < .01)
      singleton = true;
    map<cog_class, enc_word*>::iterator it1;
    vector<enc_town>::iterator it;
    // calculate the probability for every class
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
      if (new_class == 0 || old_word_enc >= 0) {
	change_prob = 0;
	skip = true;
      }
      else {
	bool has_near_neighbor = false;
	for (it=neighbors[curr_town].begin(); it != neighbors[curr_town].end(); it++) {
	  enc_word neighbor_word = cognate_classes[new_class][*it];
	  // if a word has an identical neighbor in the class, put it there
	  /*if (neighbor_word == curr_word_enc) {
	    change_prob = 1;
	    skip = true;
	    break;
	    }*/
	  // if a class doesn't have at least one neighbor a distance of 1 away from the word, skip it
	  /*else*/ if (neighbor_word >= 0) {
	    //wcout << neighbor_word << "\t" << curr_word_enc << endl;
	    if (neighbor_word == curr_word_enc || find_change (neighbor_word, curr_word_enc) > 0) {
	      //wcout << 9 << endl;
	      has_near_neighbor = true;
	      break;
	    }
	  }
	}
	if (!has_near_neighbor) {
	  if ((singleton && new_class != curr_class) || (!singleton && new_class != to_create)) {
	    change_prob = 0;
	    skip = true;
	  }
	}
      }
      // if the above case is true, we need to calculate the probability
      if (!skip)
	change_prob = find_change_prob (town_dicts, curr_town, curr_word_enc, curr_count, curr_class, new_class, class_count);
      // add the probability to the list
      class_probs[new_class] = change_prob;
      total_prob += change_prob;
    }
    // randomly assign our word to a cognate class, weighted by the probabilities of it going there as calculated above
    double running_total = 0;
    int target = rand();
    cog_class chosen;
    chosen = curr_class;
    double top_prob = 0;
    double top_log_prob = log(0);
    double chosen_log_prob = 0;
    double log_prob;
    cog_class best_prob = curr_class;
    bool done = false;
    for (int i=0; i < cog_class_ct; i++) {
      if (class_probs[i] > 0) {
	/*if (700 <= iter) {
	add_counts (curr_town, curr_word_enc, curr_count, i, false);
	log_prob = total_log_prob(town_dicts);
	if (log_prob > top_log_prob) {
	  best_prob = i;
	  top_log_prob = log_prob;
	}
	//wcout << iter << "\t" << i << "\t" << class_probs[i] << "\t" << log_prob << endl;
	remove_counts (curr_town, curr_word_enc, curr_count, i);
	}
	}*/
      running_total += class_probs[i]*RAND_MAX/total_prob;
      if (target < running_total) {
	chosen = i;
	break;
	//done = true;
	}
      /*if (class_probs[i] > top_prob) {
	chosen = i;
	top_prob = class_probs[i];
	//if (700 <= iter)
	//chosen_log_prob = log_prob;
	}*/
      }
    }
    /*if (700 <= iter && best_prob != chosen) {
      wcout << iter << "\t" << curr_town << ", " << curr_word << ", " << curr_word_enc << ", " << curr_class << "\t" << best_prob << ": " << class_probs[best_prob] << ", " << top_log_prob << "\t" << chosen << ": " << class_probs[chosen] << ", " << chosen_log_prob << endl;
      add_counts (curr_town, curr_word_enc, curr_count, best_prob, false);
      wcout << total_log_prob2(town_dicts) << endl;
      remove_counts (curr_town, curr_word_enc, curr_count, best_prob);
      add_counts (curr_town, curr_word_enc, curr_count, chosen, false);
      wcout << total_log_prob2(town_dicts) << endl;
      remove_counts (curr_town, curr_word_enc, curr_count, chosen);
    //break;
    }*/
    //wcout << iter << "\t" << curr_town << "\t" << curr_word << "\t" << curr_class << "\t" << chosen << endl;
    // if the word's former class is empty, remove it
    if (chosen != curr_class && singleton)
      remove_class (curr_class);
    // mark the connection everywhere, adjust the sound correspondence counts of the cognate class's other members
    add_counts (curr_town, curr_word_enc, curr_count, chosen, false);
    /*map<enc_word, map<enc_word, int> >::iterator it8;
    map<enc_word, int>::iterator it9;
    for (int i=0; i < town_enc_ct; i++) {
      for (it8=town_bigrams[i].begin(); it8 != town_bigrams[i].end(); it8++) {
	for (it9=it8->second.begin(); it9 != it8->second.end(); it9++) {
	  if (total_word_counts[it8->first] >= MIN_COUNT && total_word_counts[it9->first] >= MIN_COUNT && (rev_class_bigrams[cognates[i][it9->first]].count(cognates[i][it8->first]) == 0 || rev_class_bigrams[cognates[i][it9->first]][cognates[i][it8->first]] < 0))
	    wcout << i << "\t" << it8->first << "," << it9->first << "\t" << cognates[i][it8->first] << "," << cognates[i][it9->first] << "\t" << it9->second << endl;
	}
      }
      }*/
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
  
  /*multimap<int, pair<wstring,wstring> > inverse_town_dict;
	map<pair<wstring,wstring>, int>::iterator it;
	for (it=towns_map.begin(); it != towns_map.end(); it++)
	    inverse_town_dict.insert(pair<int, pair<wstring,wstring> > (it->second, it->first));
	multimap<int, pair<wstring,wstring> >::reverse_iterator it3;
	for (it3=inverse_town_dict.rbegin(); it3 != inverse_town_dict.rend(); it3++)
	    output << it3->first << '\t' << WChar_to_UTF8(it3->second.second.c_str())<< '\t' << WChar_to_UTF8(it3->second.first.c_str()) << endl;
	output << '\n';*/
	
  
  multimap<int, enc_word*> inverse_cog_class;
  map<cog_class, enc_word*>::iterator it4;
  for (it4=cognate_classes.begin(); it4 != cognate_classes.end(); it4++) 
  {
    enc_word* cog_class = it4->second;
    int occurances=0;
    bool first=true;
    int lastword;
    bool include=true;
    for (int i=0; i < town_enc_ct; i++)
    {
      if (it4->second[i] >= 0)
      {
	if(first)
	{
	  first=false;
	}
	else
	{
	  if(lastword==it4->second[i])
	    include=false;
	}
	lastword=it4->second[i];
	occurances=occurances+town_dicts[i][word_decoding[it4->second[i]].c_str()];
      }
    }
    if(include)
      inverse_cog_class.insert(pair<int, enc_word*> (occurances, cog_class));  
      }
  
  multimap<int, enc_word*>::reverse_iterator it5;
  for (it5=inverse_cog_class.rbegin(); it5 != inverse_cog_class.rend(); it5++) {
    for (int i=0; i < town_enc_ct; i++) {
      if (it5->second[i] >= 0)
	outfile2 << it5->first << "\t" << town_decoding[i].first << "," << town_decoding[i].second << "\t" << it5->second[i] << "\t" << WChar_to_UTF8 (word_decoding[it5->second[i]].c_str()) << endl;
      //outfile2 << i << "\t" << it4->second[i] << "\t" << WChar_to_UTF8 (word_decoding[it4->second[i]].c_str()) << endl;
    }
    outfile2 << endl;
    }
  
  /*map<cog_class, enc_word*>::iterator it4;
  for (it4=cognate_classes.begin(); it4 != cognate_classes.end(); it4++) {
    for (int i=0; i < town_enc_ct; i++) {
      if (it4->second[i] >= 0)
	outfile2 << it4->first << "\t" << town_decoding[i].first << "," << town_decoding[i].second << "\t" << it4->second[i] << "\t" << WChar_to_UTF8 (word_decoding[it4->second[i]].c_str()) << endl;
      //outfile2 << i << "\t" << it4->second[i] << "\t" << WChar_to_UTF8 (word_decoding[it4->second[i]].c_str()) << endl;
    }
    outfile2 << endl;
    }
  /*ofstream outfile3 ("nonsense/chars.txt");
  for (int i=0; i < enc_ct; i++)
  outfile3 << i << "\t" << WChar_to_UTF8 (decoding[i].c_str()) << endl;*/
}

int main (int argc, char* argv[]) {  
  map<enc_town, map<wstring, float> > town_dicts;
  encoding[NULL_CHAR] = enc_ct;
  decoding[enc_ct++] = NULL_CHAR;
  encoding[L""] = 0;
  encoding[L"!"] = enc_ct;
  decoding[enc_ct++] = L"!";
  
  /*map<Town, vector<wstring> > town_vectors;
  vectorize_all (town_vectors, argv[1]);
  write_bigrams_to_file(town_vectors, argv[2]);*/

  vector<set<enc_char> > chars;
  gather_lists (town_dicts, chars, argv[1]);
  gather_bigrams (town_dicts, argv[2]);
  char_distance_calc (chars, argv[3]);
  find_cognates (town_dicts, argv[5], argv[6], argv[4]);

  return 0;
}
