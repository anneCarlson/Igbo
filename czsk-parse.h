#include <boost/algorithm/string.hpp>
#include <fstream>
#include <map>
#include <string>
#include <vector>
using namespace std;
using namespace boost;

#define BD '%'
#define START "^"
#define UNK L"@"

wstring czsk_parse (wchar_t* str, bool czech) {
  int word_length = wcslen(str);
  wstring result;
  int i = 0;
  if (czech) {
    while (i < word_length) {
      if (str[i] == 67 || str[i] == 99) { // c
	result += str[i++];
	if (i < word_length) {
	  if (str[i] == 104) // h
	    result += str[i++];
	}
	result += BD;
      }
      else if (str[i] == 79 || str[i] == 111) { // o
	result += str[i++];
	if (i < word_length) {
	  if (str[i] == 117) // u
	    result += str[i++];
	}
	result += BD;
      }
      else if (str[i] == 89) { // y
	result += 73;
	result += BD;
	++i;
      }
      else if (str[i] == 121) { // y
	result += 105;
	result += BD;
	++i;
      }
      else if (str[i] == 221) { // y
	result += 205;
	result += BD;
	++i;
      }
      else if (str[i] == 253) { // y
	result += 237;
	result += BD;
	++i;
      }
      else if (str[i+1] == 105 || str[i+1] == 237) { // i
	if (str[i] == 68) // D
	  result += 270;
	else if (str[i] == 100) // d
	  result += 271;
	else if (str[i] == 78) // N
	  result += 327;
	else if (str[i] == 110) // n
	  result += 328;
	else if (str[i] == 84) // T
	  result += 356;
	else if (str[i] == 116) // t
	  result += 357;
	else
	  result += str[i];
	++i;
	result += BD;
	result += str[i++];
	result += BD;
      }
      else if (str[i+1] == 283) {
	if (str[i] == 68) // D
	  result += 270;
	else if (str[i] == 100) // d
	  result += 271;
	else if (str[i] == 78) // N
	  result += 327;
	else if (str[i] == 110) // n
	  result += 328;
	else if (str[i] == 84) // T
	  result += 356;
	else if (str[i] == 116) // t
	  result += 357;
	else if (str[i] == 66 || str[i] == 98 || str[i] == 70 || str[i] == 102 || str[i] == 80 || str[i] == 112 || str[i] == 86 || str[i] == 118) { // b, f, p, v
	  result += str[i];
	  result += BD;
	  result += 106;
	}
	else if (str[i] == 77 || str[i] == 109) { // m
	  result += str[i];
	  result += BD;
	  result += 328;
	}
	++i;
	result += BD;
	result += 101;
	result += BD;
	++i;
      }
      else {
	result += str[i++];
	result += BD;
      }
    }
  }
  else {
    while (i < word_length) {
      if (str[i] == 67 || str[i] == 99) { // c
	result += str[i++];
	if (i < word_length) {
	  if (str[i] == 104) // h
	    result += str[i++];
	}
	result += BD;
      }
      if (str[i] == 68) { // D
	if (str[i+1] == 122 || str[i+1] == 382) { // z
	  result += str[i++];
	  result += str[i++];
	}
	else if (str[i+1] == 101 || str[i+1] == 105 || str[i+1] == 237) { // e, i
	  ++i;
	  result += 270;
	  result += BD;
	  result += str[i++];
	}
	else
	  result += str[i++];
	result += BD;
      }
      if (str[i] == 100) { // d
	if (str[i+1] == 122 || str[i+1] == 382) { // z
	  result += str[i++];
	  result += str[i++];
	}
	else if (str[i+1] == 101 || str[i+1] == 105 || str[i+1] == 237) { // e, i
	  ++i;
	  result += 271;
	  result += BD;
	  result += str[i++];
	}
	else
	  result += str[i++];
	result += BD;
      }
      else if (str[i] == 89) { // y
	result += 73;
	result += BD;
	++i;
      }
      else if (str[i] == 121) { // y
	result += 105;
	result += BD;
	++i;
      }
      else if (str[i] == 221) { // y
	result += 205;
	result += BD;
	++i;
      }
      else if (str[i] == 253) { // y
	result += 237;
	result += BD;
	++i;
      }
      else if (str[i] == 73 || str[i] == 105) { // i
	result += str[i++];
	if (str[i] == 97 || str[i] == 101 || str[i] == 117) // a, e, u
	  result += str[i++];
	result += BD;
      }
      else if (str[i+1] == 101 || str[i+1] == 105 || str[i+1] == 237) { // e, i
	if (str[i] == 78) // N
	  result += 327;
	else if (str[i] == 110) // n
	  result += 328;
	else if (str[i] == 84) // T
	  result += 356;
	else if (str[i] == 116) // t
	  result += 357;
	else if (str[i] == 76) // L
	  result += 317;
	else if (str[i] == 108) // l
	  result += 318;
	else
	  result += str[i];
	++i;
	result += BD;
      }
      else {
	result += str[i++];
	result += BD;
      }
    }
  }
  return result;
}
  
void czsk_counts (map<wstring, int>& unigram_counts, map<wstring, map<wstring, int> >& bigram_counts, bool czech, const char* corpfile) {
  ifstream corpus (corpfile);
  string prev_word = START;
  map<string, int> str_unigram_counts;
  map<string, map<string, int> > str_bigram_counts;
  while (corpus.good()) {
    str_unigram_counts[START]++;
    string line;
    getline (corpus, line);
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
    corpus.ignore (500, '\n');
    prev_word = START;
  }
  map<string, int>::iterator it1;
  map<string, map<string, int> >::iterator it2;
  unigram_counts[UNK] = 0;
  for (it1=str_unigram_counts.begin(); it1 != str_unigram_counts.end(); it1++)
    unigram_counts[czsk_parse(UTF8_to_WChar(it1->first.c_str()), czech)] = it1->second;
  for (it2=str_bigram_counts.begin(); it2 != str_bigram_counts.end(); it2++) {
    wstring curr_string = czsk_parse (UTF8_to_WChar(it2->first.c_str()), czech);
    for (it1=it2->second.begin(); it1 != it2->second.end(); it1++)
      bigram_counts[curr_string][czsk_parse(UTF8_to_WChar(it1->first.c_str()), czech)] = it1->second;
  }
}

void czsk_wordlist (map<string, int>& counts, bool czech, const char* corpfile) {
  ifstream corpus (corpfile);
  map<string, int> str_counts;
  while (corpus.good()) {
    string line;
    getline (corpus, line);
    vector<string> splitline;
    split (splitline, line, is_any_of("\t "), token_compress_on);
    vector<string>::iterator it1;
    for (it1=splitline.begin(); it1 != splitline.end(); it1++) {
      if (*it1 != "/" && *it1 != "") {
	str_counts[*it1]++;
      }
    }
    corpus.ignore (500, '\n');
  }
  counts = str_counts;
}
