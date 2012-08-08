// calculates edit distance between words in two word lists
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <string>
#include <cmath>
#include <map>
#include "wordlist.h"
#include "wchar.h"
using namespace std;

#define FREQ_POWER 1
#define MIN_FREQ 3
#define MAX_DIST 1
#define NULL_CHAR L"&"
#define BD '%'
#define NBD '^'

struct charpair {
  wchar_t first;
  wchar_t second;
} ;

typedef vector<wstring> Word;
typedef map<wstring, int> TownWords;
typedef map<string, TownWords> TownList;
typedef vector<wstring> PathList;
typedef map<wstring, PathList> ChangeMap1;
typedef map<wstring, ChangeMap1> ChangeMap2;
typedef vector<wstring> SecondWords;
typedef map<wstring, SecondWords> WordPairs;
typedef map<wstring, WordPairs> PathMatches;

enum process {NUMBER, PATH, PAIR} ;

// outputs a string representing one change
// [-x] = delete x, [+x] = insert x, [xy] = change x to y
wstring represent_path (wstring* path, int distance) {
  wstring representation;
  for (int x=0; x != distance; x+=2) { // goes character-by-character
    if (path[x] == NULL_CHAR) {
      representation += L"[-";
      representation += path[x+1];
      representation += L"]";
    }
    else if (path[x+1] == NULL_CHAR) {
      representation += L"[+";
      representation += path[x];
      representation += L"]";
    }
    else {
      representation += L"[";
      representation += path[x];
      representation += L"*";
      representation += path[x+1];
      representation += L"]";
    }
  }
  return representation;
}

// helps fill in the path matrix (see below)
void add_to_vector (PathList& previous_vector, wstring* to_add) {
  wstring string_rep = represent_path (to_add, 2);
  if (previous_vector.size() == 0) {
    previous_vector.push_back (string_rep);
  }
  else {
    PathList::iterator it;
    // if there is more than one possible path, add to all of them
    for (it=previous_vector.begin(); it != previous_vector.end(); it++) {
      it->append (string_rep);
    }
  }
}

// calculates the edit distance between two words and tracks the differences
int edit_distance (Word first, Word second, PathList& paths) {
  int first_length = first.size() + 1;
  int second_length = second.size() + 1;
  Word::iterator it1 = first.begin();
  Word::iterator it2 = second.begin();
  int distance_matrix [first_length][second_length];
  PathList path_matrix [first_length][second_length];
  wstring charstring[2];
  bool match;
  distance_matrix[0][0] = 0;
  PathList newvector;
  newvector.push_back(L"");
  path_matrix[0][0] = newvector;
  // fill in the top and left edges of the matrix (i.e. one string is empty)
  for (int i=1; i != first_length; i++) {
    path_matrix[i][0] = path_matrix[i-1][0];
    distance_matrix[i][0] = i;
    charstring[0] = *(it1++);
    charstring[1] = NULL_CHAR;
    add_to_vector (path_matrix[i][0], charstring);
  }
  for (int j=1; j != second_length; j++) {
    path_matrix[0][j] = path_matrix[0][j-1];
    distance_matrix[0][j] = j;
    charstring[0] = NULL_CHAR;
    charstring[1] = *(it2++);
    add_to_vector (path_matrix[0][j], charstring);
  }
  it1 = first.begin();
  for (int i=1; i != first_length; i++) {
    it2 = second.begin();
    for (int j=1; j != second_length; j++) {
      int deletion = distance_matrix[i-1][j] + 1;
      int insertion = distance_matrix[i][j-1] + 1;
      int substitution = distance_matrix[i-1][j-1];
      match = true;
      if (*it1 != *it2) {
	substitution++;
        match = false;
      }
      int minimum_distance = min(deletion, min(insertion, substitution));
      distance_matrix[i][j] = minimum_distance;
      PathList* currcell = &path_matrix[i][j];
      PathList::iterator it;
      // add the changes to the current cell of the path matrix here
      if (minimum_distance == deletion) {
        PathList delcell = path_matrix[i-1][j];
        charstring[0] = *it1;
        charstring[1] = NULL_CHAR;
        add_to_vector (delcell, charstring);
	for (it=delcell.begin(); it != delcell.end(); it++)
	  currcell->push_back (*it);
      }
      if (minimum_distance == insertion) {
        PathList inscell = path_matrix[i][j-1];
        charstring[0] = NULL_CHAR;
        charstring[1] = *it2;
        add_to_vector (inscell, charstring);
	for (it=inscell.begin(); it != inscell.end(); it++)
	  currcell->push_back (*it);
      }
      if (minimum_distance == substitution) {
        PathList subcell = path_matrix[i-1][j-1];
	// if the letters are identical, no need to track that
        if (!match) {
          charstring[0] = *it1;
          charstring[1] = *it2;
          add_to_vector (subcell, charstring);
        }
	for (it=subcell.begin(); it != subcell.end(); it++)
	  currcell->push_back (*it);
      }
      it2++;
    }
    it1++;
  }
  paths = path_matrix[first_length-1][second_length-1];
  return distance_matrix[first_length-1][second_length-1];
}

// subroutine for adding a pair of words to our storage dictionary (see below)
int calculate_pair (TownWords& first_dict, TownWords& second_dict, PathMatches* distance_dict, ChangeMap2& word_matches, wstring first_word, wstring second_word) {
  Word first_split;
  Word second_split;
  wstring curr_char;
  for (int i=1; i <= first_word.length(); i+=2) {
    curr_char += first_word[i-1];
    if (first_word[i] == BD) {
      first_split.push_back(curr_char);
      curr_char.clear();
    }
  }
  first_split.push_back(curr_char);
  curr_char.clear();
  for (int j=1; j <= second_word.length(); j+=2) {
    curr_char += second_word[j-1];
    if (second_word[j] == BD) {
      second_split.push_back(curr_char);
      curr_char.clear();
    }
  }
  second_split.push_back(curr_char);
  curr_char.clear();
  PathList paths;
  PathList::iterator it1;
  int distance = edit_distance(first_split, second_split, paths);
  //word_matches[first_word][second_word] = paths;
  // adding everything takes up too much memory, unfortunately
  if (true/*first_dict.count(second_word) == 0 && second_dict.count(first_word) == 0*/) {
    if (distance <= MAX_DIST) {
      for (it1=paths.begin(); it1 != paths.end(); it1++) {
	if (distance_dict[distance].count(*it1) == 0) {
	  WordPairs newmap;
	  distance_dict[distance][*it1] = newmap;
	}
	if (distance_dict[distance][*it1].count(first_word) == 0) {
	  SecondWords newvector;
	  distance_dict[distance][*it1][first_word] = newvector;
	}
	distance_dict[distance][*it1][first_word].push_back(second_word);
      }
    }
  }
  return distance;
}

// returns the average minimum distance between words in the set
// dictionary maps distance to path of changes to word in first town to all words in second town which differ from the first word by the given path
float compare (TownList& town_dicts, PathMatches* distance_dict, ChangeMap2& word_matches, string first_town, string second_town) {
  float avg_min_dist_numer = 0;
  float avg_min_dist_denom = 0;
  TownWords* first_dict = &town_dicts[first_town];
  TownWords* second_dict = &town_dicts[second_town];
  int x = 0;
  int y;
  TownWords::iterator it1;
  TownWords::iterator it2;
  for (it1=first_dict->begin(); it1 != first_dict->end(); it1++) {
    int first_frequency = it1->second;
    // although many words only appear once, it takes too much time to handle these
    if (first_frequency >= MIN_FREQ) {
      if (x % 1 == 0)
	cout << x++ << endl;
      int minimum_distance = 50;
      //ChangeMap1 newmap;
      //word_matches[it1->first] = newmap;
      y = 0;
      for (it2=second_dict->begin(); it2 != second_dict->end(); it2++) {
	int second_frequency = it2->second;
	if (second_frequency >= MIN_FREQ) {
	  y++;
	  if (first_dict->count(it2->first) == 0 && second_dict->count(it1->first) == 0) {
	    int distance = calculate_pair (*first_dict, *second_dict, distance_dict, word_matches, it1->first, it2->first);
	    if (distance < minimum_distance)
	      minimum_distance = distance;
	  }
	}
      }
      avg_min_dist_numer += minimum_distance*pow(first_frequency, FREQ_POWER);
      avg_min_dist_denom += pow(first_frequency, FREQ_POWER);
    }
  }
  cout << x << ", " << y << endl;
  return avg_min_dist_numer/avg_min_dist_denom;
}

// prints a representation of our dictionary
void printlist (PathMatches* distance_dict, float avg_min_dist, string first_town, string second_town, const char* outfile) {
  ofstream output (outfile);
  PathMatches::iterator it2;
  multimap<int, wstring>::reverse_iterator it3;
  WordPairs::iterator it4;
  SecondWords::iterator it5;
  output << "Comparison: " << first_town << " and " << second_town << " (" << avg_min_dist << ")" << endl;
  for (int x=0; x <= MAX_DIST; x++) {
    output << "\n" << x << " changes:" << endl;
    PathMatches paths = distance_dict[x];
    // sort by frequency
    multimap<int, wstring> path_counts;
    int total = 0;
    for (it2=paths.begin(); it2 != paths.end(); it2++) {
      pair<int, wstring> path_count_pair (it2->second.size(), it2->first);
      path_counts.insert (path_count_pair);
    }
    for (it3=path_counts.rbegin(); it3 != path_counts.rend(); it3++) {
      total += it3->first;
      output << "\t\t" << WChar_to_UTF8(it3->second.c_str()) << "\t(" << it3->first << ")" << endl;
      for (it4=paths[it3->second].begin(); it4 != paths[it3->second].end(); it4++) {
	for (it5=it4->second.begin(); it5 != it4->second.end(); it5++) {
	  // we'll probably want to exclude some cases later on
	  if (true)
	    output << WChar_to_UTF8(it3->second.c_str()) << "\t" << WChar_to_UTF8(it4->first.c_str()) << ", " << WChar_to_UTF8(it5->c_str()) << endl;
	}
      }
    }
    output << total << endl;
  }
}

// ignore this
/*int gather_dict (PathMatches* distance_dict, const char* dictfile) {
  ifstream dictionary (dictfile);
  int distance = -1;
  int y = 0;
  char path [256];
  char first [256];
  char second [256];
  dictionary.ignore (256, '\n');
  process curr_step = NUMBER;
  while (dictionary.good()) {
    if (curr_step == NUMBER) {
      distance++;
      curr_step = PATH;
      dictionary.ignore (256, '\n');
    }
    else if (curr_step == PATH) {
      curr_step = PAIR;
      dictionary.ignore (2, '\n');
      dictionary.getline (path, 256, '\t');
      if (distance == 0)
	path[0] = '\0';
      WordPairs newmap;
      distance_dict[distance][path] = newmap;
      dictionary.ignore (256, '\n');
    }
    else {
      if (dictionary.peek() == '\n') {
	curr_step = NUMBER;
	dictionary.get();
      }
      else {
	SecondWords newvector;
	if (distance == 0) {
	  dictionary.getline (first, 256, ',');
	  path[0] = '\0';
	  dictionary.get();
	  if (distance_dict[distance][path].count(first) == 0)
	    distance_dict[distance][path][first] = newvector;
	  dictionary.getline (second, 256);
	  distance_dict[distance][path][first].push_back(second);
	}
	else {
	  dictionary.getline (path, 256, '\t');
	  if ('+' == path[1] && 'a' == path[2] && '\0' == path[4])
	    y ++;
	  dictionary.getline (first, 256, ',');
	  dictionary.get();
	  if (distance_dict[distance][path].count(first) == 0)
	    distance_dict[distance][path][first] = newvector;
	  dictionary.getline (second, 256);
	  distance_dict[distance][path][first].push_back(second);
	  if (dictionary.peek() == '\t')
	    curr_step = PATH;
	}
      }
    }
  }
  cout << y << endl;
  }*/

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

int main (int argc, char* argv[]) {
  TownList town_dicts;
  PathMatches distance_dict[MAX_DIST+1];
  ChangeMap2 word_matches;
  gather_lists (town_dicts, argv[1]);
  float avg_min_dist = compare(town_dicts, distance_dict, word_matches, argv[2], argv[3]);
  //gather_dict (distance_dict, argv[1]);
  //float avg_min_dist = 0.0;
  printlist (distance_dict, avg_min_dist, argv[2], argv[3], argv[4]);
  return 0;
}
