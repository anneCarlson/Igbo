#include <fstream>
#include "languagemodeling.h"
using namespace std;

int main (int argc, char* argv[]) {
  map<wstring, int> first_unigram_cts;
  map<wstring, int> second_unigram_cts;
  map<wstring, map<wstring, int> > first_bigram_cts;
  map<wstring, map<wstring, int> > second_bigram_cts;
  map<wstring, int> unigram_merged;
  map<wstring, map<wstring, int> > bigram_merged;
  map<wstring, float> unigram_probs;
  map <wstring, map<wstring, float> > bigram_probs;

  counts (first_unigram_cts, first_bigram_cts, argv[2], argv[1]);
  counts (second_unigram_cts, second_bigram_cts, argv[3], argv[1]);
  merge_lists (first_unigram_cts, second_unigram_cts, unigram_merged);
  merge_lists (first_bigram_cts, second_bigram_cts, bigram_merged);
  calculate_probs (unigram_merged, unigram_probs, bigram_merged, bigram_probs);
  return 0;
}
