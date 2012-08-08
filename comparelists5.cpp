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
#define MIN_FREQ 1
#define MAX_DIST 8
#define NULL_CHAR '%'

struct charpair {
  wchar_t first;
  wchar_t second;
} ;
struct wordpair {
  wchar_t* first;
  wchar_t* second;
} ;

void add_to_vector (vector<vector<charpair> >& previous_vector, charpair to_add) {
  if (previous_vector.size() == 0) {
    vector<charpair> newvector;
    newvector.push_back (to_add);
    previous_vector.push_back (newvector);
  }
  else {
    vector<vector<charpair> >::iterator it;
    for (it=previous_vector.begin(); it != previous_vector.end(); it++) {
      it->push_back (to_add);
    }
  }
}

int edit_distance (const wchar_t* first, const wchar_t* second, vector<vector<charpair> >& paths) {
  int first_length = wcslen(first) + 1;
  int second_length = wcslen(second) + 1;
  int** distance_matrix = new int* [first_length];
  vector<vector<charpair> >** path_matrix = new vector<vector<charpair> >* [first_length];
  for (int i=0; i != first_length; i++) {
    distance_matrix[i] = new int [second_length];
    path_matrix[i] = new vector<vector<charpair> > [second_length];
  }
  distance_matrix[0][0] = 0;
  vector<vector<charpair> > newvector;
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
      vector<vector<charpair> >* currcell = &path_matrix[i][j];
      vector<vector<charpair> >::iterator it;
      if (minimum_distance == deletion) {
	charpair delpair;
	delpair.first = first[i-1];
	delpair.second = NULL_CHAR;
	vector<vector<charpair> > delcell = path_matrix[i-1][j];
	add_to_vector (delcell, delpair);
	for (it=delcell.begin(); it != delcell.end(); it++)
	  currcell->push_back (*it);
      }
      if (minimum_distance == insertion) {
	charpair inspair;
	inspair.first = NULL_CHAR;
	inspair.second = second[j-1];
	vector<vector<charpair> > inscell = path_matrix[i][j-1];
	add_to_vector (inscell, inspair);
	for (it=inscell.begin(); it != inscell.end(); it++)
	  currcell->push_back (*it);
      }
      if (minimum_distance == substitution) {
	charpair subpair;
	subpair.first = first[i-1];
	subpair.second = second[j-1];
	vector<vector<charpair> > subcell = path_matrix[i-1][j-1];
	add_to_vector (subcell, subpair);
	for (it=subcell.begin(); it != subcell.end(); it++)
	  currcell->push_back (*it);
      }
    }
  }
  paths = path_matrix[first_length-1][second_length-1];
  int distance = distance_matrix[first_length-1][second_length-1];
  for (int i=0; i != first_length; i++) {
    delete [] path_matrix[i];
    delete [] distance_matrix[i];
  }
  delete [] path_matrix;
  delete [] distance_matrix;
  return distance;
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

// returns the average minimum distance between words in the set
float compare (map<string, map<string, int> >& town_dicts, map<int, map<string, map<string, vector<string> > > >& distance_dict, /*map<wchar_t[], map<wchar_t[], charpair[]> >& word_matches,*/ string first_town, string second_town) {
  float avg_min_dist_numer = 0;
  float avg_min_dist_denom = 0;
  map<string, int>* first_dict = &town_dicts[first_town];
  map<string, int>* second_dict = &town_dicts[second_town];
  int x = 0;
  map<string, int>::iterator it1;
  map<string, int>::iterator it2;
  vector<vector<charpair> >::iterator it3;
  vector<charpair>::iterator it4;
  for (it1=first_dict->begin(); it1 != first_dict->end(); it1++) {
    int first_frequency = it1->second;
    if (first_frequency >= MIN_FREQ) {
      if (x % 1 == 0)
	cout << x++ << endl;
      int minimum_distance = 50;
      for (it2=second_dict->begin(); it2 != second_dict->end(); it2++) {
	int second_frequency = it2->second;
	if (second_frequency >= MIN_FREQ) {
	  wchar_t* first_word = new wchar_t;
	  first_word = UTF8_to_WChar (it1->first.c_str());
	  wchar_t* second_word = new wchar_t;
	  second_word = UTF8_to_WChar (it2->first.c_str());
	  vector<vector<charpair> >* paths = new vector<vector<charpair> >;
	  int distance = edit_distance(first_word, second_word, *paths);
	  if (distance < minimum_distance)
	    minimum_distance = distance;
	  if (distance_dict.count(distance) == 0) {
	    map<string, map<string, vector<string> > > newmap;
	    distance_dict[distance] = newmap;
	  }
	  for (it3=paths->begin(); it3 != paths->end(); it3++) {
	    wchar_t charstring[2*distance+1];
	    int y = 0;
	    for (it4=it3->begin(); it4 != it3->end(); it4++) {
	      if (it4->first != it4->second) {
		charstring[y++] = it4->first;
		charstring[y++] = it4->second;
	      }
	    }
	    charstring[y] = L'\0';
	    string path = represent_path (charstring, 2*distance);
	    if (distance_dict[distance].count(path) == 0) {
	      map<string, vector<string> > newmap;
	      distance_dict[distance][path] = newmap;
	    }
	    if (distance_dict[distance][path].count(it1->first) == 0) {
	      vector<string> newvector;
	      distance_dict[distance][path][it1->first] = newvector;
	    }
	    distance_dict[distance][path][it1->first].push_back(it2->first);
	  }
	  delete paths;
	}
      }
      avg_min_dist_numer += minimum_distance*pow(first_frequency, FREQ_POWER);
      avg_min_dist_denom += pow(first_frequency, FREQ_POWER);
    }
  }
  return avg_min_dist_numer/avg_min_dist_denom;
}

void printlist (map<int, map<string, map<string, vector<string> > > >& distance_dict, float avg_min_dist, string first_town, string second_town, const char* outfile) {
  ofstream output (outfile);
  map<int, map<string, map<string, vector<string> > > >::iterator it1;
  map<string, map<string, vector<string> > >::iterator it2;
  multimap<int, string>::reverse_iterator it3;
  map<string, vector<string> >::iterator it4;
  vector<string>::iterator it5;
  output << "Comparison: " << first_town << " and " << second_town << " (" << avg_min_dist << ")" << endl;
  for (it1=distance_dict.begin(); it1 != distance_dict.end(); it1++) {
    if (it1->first <= MAX_DIST) {
      output << "\n" << it1->first << " changes:" << endl;
      map<string, map<string, vector<string> > > paths = it1->second;
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
}

int main (int argc, char* argv[]) {
  map<string, map<string, int> > town_dicts;
  map<int, map<string, map<string, vector<string> > > > distance_dict;
  //map<wchar_t[], map<wchar_t[], charpair[]> > word_matches;
  wordlist (town_dicts, argv[1]);
  float avg_min_dist = compare(town_dicts, distance_dict, /*word_matches,*/ argv[2], argv[3]);
  printlist (distance_dict, avg_min_dist, argv[2], argv[3], argv[4]);
  return 0;
}
