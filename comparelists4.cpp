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
#define MAX_DIST 2
#define NULL_CHAR '%'

struct charpair {
  wchar_t first;
  wchar_t second;
} ;

typedef map<string, int> TownWords;
typedef map<string, TownWords> TownList;
typedef vector<charpair> ChangeList;
typedef vector<ChangeList> PathList;
typedef vector<string> SecondWords;
typedef map<string, SecondWords> WordPairs;
typedef map<string, WordPairs> PathMatches;

enum process {NUMBER, PATH, PAIR} ;

void add_to_vector (PathList& previous_vector, charpair to_add) {
  if (previous_vector.size() == 0) {
    ChangeList newvector;
    newvector.push_back (to_add);
    previous_vector.push_back (newvector);
  }
  else {
    PathList::iterator it;
    for (it=previous_vector.begin(); it != previous_vector.end(); it++) {
      it->push_back (to_add);
    }
  }
}

int edit_distance (const wchar_t* first, const wchar_t* second, PathList& paths) {
  int first_length = wcslen(first) + 1;
  int second_length = wcslen(second) + 1;
  int distance_matrix [first_length][second_length];
  PathList path_matrix [first_length][second_length];
  distance_matrix[0][0] = 0;
  PathList newvector;
  path_matrix[0][0] = newvector;
  for (int i=1; i != first_length; i++) {
    path_matrix[i][0] = path_matrix[i-1][0];
    distance_matrix[i][0] = i;
    charpair to_add;
    to_add.first = first[i-1];
    to_add.second = NULL_CHAR;
    add_to_vector (path_matrix[i][0], to_add);
  }
  for (int j=1; j != second_length; j++) {
    path_matrix[0][j] = path_matrix[0][j-1];
    distance_matrix[0][j] = j;
    charpair to_add;
    to_add.first = NULL_CHAR;
    to_add.second = second[j-1];
    add_to_vector (path_matrix[0][j], to_add);
  }
  for (int i=1; i != first_length; i++) {
    for (int j=1; j != second_length; j++) {
      int deletion = distance_matrix[i-1][j] + 1;
      int insertion = distance_matrix[i][j-1] + 1;
      int substitution = distance_matrix[i-1][j-1];
      if (first[i-1] != second[j-1])
	substitution++;
      int minimum_distance = min(deletion, min(insertion, substitution));
      distance_matrix[i][j] = minimum_distance;
      PathList* currcell = &path_matrix[i][j];
      PathList::iterator it;
      if (minimum_distance == deletion) {
	charpair delpair;
	delpair.first = first[i-1];
	delpair.second = NULL_CHAR;
	PathList delcell = path_matrix[i-1][j];
        add_to_vector (delcell, delpair);
	for (it=delcell.begin(); it != delcell.end(); it++)
	  currcell->push_back (*it);
      }
      if (minimum_distance == insertion) {
	charpair inspair;
	inspair.first = NULL_CHAR;
	inspair.second = second[j-1];
	PathList inscell = path_matrix[i][j-1];
        add_to_vector (inscell, inspair);
	for (it=inscell.begin(); it != inscell.end(); it++)
	  currcell->push_back (*it);
      }
      if (minimum_distance == substitution) {
	charpair subpair;
	subpair.first = first[i-1];
	subpair.second = second[j-1];
	PathList subcell = path_matrix[i-1][j-1];
        add_to_vector (subcell, subpair);
	for (it=subcell.begin(); it != subcell.end(); it++)
	  currcell->push_back (*it);
      }
    }
  }
  paths = path_matrix[first_length-1][second_length-1];
  return distance_matrix[first_length-1][second_length-1];
}

string represent_path (wchar_t* path, int distance) {
  stringstream representation;
  string::iterator it;
  for (int x=0; x != distance; x+=2) {
    if (path[x] == NULL_CHAR) {
      wchar_t to_convert[] = {path[x+1], L'\0'};
      representation << "[-" << WChar_to_UTF8 (to_convert) << "]";
    }
    else if (path[x+1] == NULL_CHAR) {
      wchar_t to_convert[] = {path[x], L'\0'};
      representation << "[+" << WChar_to_UTF8 (to_convert) << "]";
    }
    else {
      wchar_t to_convert[] = {path[x], path[x+1], L'\0'};
      representation << "[" << WChar_to_UTF8 (to_convert) << "]";
    }
  }
  return representation.str();
}

int calculate_pair (PathMatches* distance_dict, string first_word, string second_word) {
  wchar_t* first_utf = UTF8_to_WChar (first_word.c_str());
  wchar_t* second_utf = UTF8_to_WChar (second_word.c_str());
  PathList paths;
  //PathList::iterator it1;
  //ChangeList::iterator it2;
  int distance = edit_distance(first_utf, second_utf, paths);
  /*for (it1=paths.begin(); it1 != paths.end(); it1++) {
    wchar_t charstring [2*distance+1];
    int y = 0;
    for (it2=it1->begin(); it2 != it1->end(); it2++) {
      if (it2->first != it2->second) {
	charstring[y++] = it2->first;
	charstring[y++] = it2->second;
      }
    }
    charstring[y] = L'\0';
    string path = represent_path (charstring, 2*distance);
    if (distance_dict[distance].count(path) == 0) {
      WordPairs newmap;
      distance_dict[distance][path] = newmap;
    }
    if (distance_dict[distance][path].count(first_word) == 0) {
      SecondWords newvector;
      distance_dict[distance][path][first_word] = newvector;
    }
    distance_dict[distance][path][first_word].push_back(second_word);
    }
    return distance;*/
  return 0;
}

// returns the average minimum distance between words in the set
float compare (TownList& town_dicts, PathMatches* distance_dict, /*map<wchar_t[], map<wchar_t[], charpair[]> >& word_matches,*/ string first_town, string second_town) {
  float avg_min_dist_numer = 0;
  float avg_min_dist_denom = 0;
  TownWords* first_dict = &town_dicts[first_town];
  TownWords* second_dict = &town_dicts[second_town];
  int x = 0;
  TownWords::iterator it1;
  TownWords::iterator it2;
  for (it1=first_dict->begin(); it1 != first_dict->end(); it1++) {
    int first_frequency = it1->second;
    if (first_frequency >= MIN_FREQ) {
      if (x % 1 == 0)
	cout << x++ << endl;
      int minimum_distance = 50;
      for (it2=second_dict->begin(); it2 != second_dict->end(); it2++) {
	int second_frequency = it2->second;
	if (second_frequency >= MIN_FREQ) {
	  int distance = calculate_pair (distance_dict, it1->first, it2->first);
	  if (distance < minimum_distance)
	    minimum_distance = distance;
	}
      }
      avg_min_dist_numer += minimum_distance*pow(first_frequency, FREQ_POWER);
      avg_min_dist_denom += pow(first_frequency, FREQ_POWER);
    }
  }
  return avg_min_dist_numer/avg_min_dist_denom;
}

void printlist (PathMatches* distance_dict, float avg_min_dist, string first_town, string second_town, const char* outfile) {
  ofstream output (outfile);
  PathMatches::iterator it2;
  multimap<int, string>::reverse_iterator it3;
  WordPairs::iterator it4;
  SecondWords::iterator it5;
  output << "Comparison: " << first_town << " and " << second_town << " (" << avg_min_dist << ")" << endl;
  for (int x=0; x <= MAX_DIST; x++) {
    output << "\n" << x << " changes:" << endl;
    PathMatches paths = distance_dict[x];
    multimap<int, string> path_counts;
    for (it2=paths.begin(); it2 != paths.end(); it2++) {
      pair<int, string> path_count_pair (it2->second.size(), it2->first);
      path_counts.insert (path_count_pair);
    }
    for (it3=path_counts.rbegin(); it3 != path_counts.rend(); it3++) {
      output << "\t\t" << it3->second << "\t(" << it3->first << ")" << endl;
      for (it4=paths[it3->second].begin(); it4 != paths[it3->second].end(); it4++) {
	for (it5=it4->second.begin(); it5 != it4->second.end(); it5++) {
	  if (true)
	    output << it3->second << "\t" << it4->first << ", " << *it5 << endl;
	}
      }
    }
  }
}

int gather_dict (PathMatches* distance_dict, const char* dictfile) {
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
}

int main (int argc, char* argv[]) {
  TownList town_dicts;
  PathMatches distance_dict[50];
  //map<wchar_t[], map<wchar_t[], charpair[]> > word_matches;
  wordlist (town_dicts, argv[1]);
  float avg_min_dist = compare(town_dicts, distance_dict, /*word_matches,*/ argv[2], argv[3]);
  //gather_dict (distance_dict, argv[1]);
  //float avg_min_dist = 0.0;
  printlist (distance_dict, avg_min_dist, argv[2], argv[3], argv[4]);
  return 0;
}
