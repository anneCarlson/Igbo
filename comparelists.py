import sys
import codecs
import math
import wordlist

# from Stavros Korokithakis
# http://www.korokithakis.net/posts/finding-the-levenshtein-distance-in-python/
# modified by us
def editdist(first, second):
    """Find the Levenshtein distance between two strings and store the path taken."""
    first_length = len(first) + 1
    second_length = len(second) + 1
    distance_matrix = [[0] * second_length for x in range(first_length)]
    path_matrix = [[None] * second_length for x in range(first_length)]
    distance_matrix[0][0] = 0
    path_matrix[0][0] = []
    for i in range(1, first_length):
        distance_matrix[i][0] = i
        path_matrix[i][0] = add_to_list(path_matrix[i-1][0], (first[i-1],None))
    for j in range(1, second_length):
        distance_matrix[0][j] = j
        path_matrix[0][j] = add_to_list(path_matrix[0][j-1], (None,second[j-1]))
    for i in xrange(1, first_length):
        for j in range(1, second_length):
            deletion = distance_matrix[i-1][j] + 1
            insertion = distance_matrix[i][j-1] + 1
            substitution = distance_matrix[i-1][j-1]
            if first[i-1] != second[j-1]:
                substitution += 1
            minimum_distance = min(deletion, insertion, substitution)
            distance_matrix[i][j] = minimum_distance
            path_matrix[i][j] = []
            if minimum_distance == deletion:
                path_matrix[i][j] += add_to_list(path_matrix[i-1][j], (first[i-1],None))
            if minimum_distance == insertion:
                path_matrix[i][j] += add_to_list(path_matrix[i][j-1], (None,second[j-1]))
            if minimum_distance == substitution:
                path_matrix[i][j] += add_to_list(path_matrix[i-1][j-1], (first[i-1],second[j-1]))
    return (distance_matrix[first_length-1][second_length-1], path_matrix[first_length-1][second_length-1])

def add_to_list(previous_list, to_add):
    """Add an item to the end of each list of a given list of lists."""
    if previous_list == []:
        return [[to_add]]
    else:
        return [item + [to_add] for item in previous_list]

def compare(first_town, second_town):
    """Create a list of all pairs of words from the word lists of two towns no further than the maximum Levenshtein distance away from each other. Also track the average minimum distance of the towns: sum_i,j(dist(a_i,b_j)(freq(a_i)^factor)(freq(b_j)^factor))/sum_i,j((freq(a_i)^factor)(freq(b_j)^factor))"""
    distance_dictionary = {}
    avg_min_dist_numer = 0
    avg_min_dist_denom = 0
    frequency_factor = 1
    x = 0
    print len(worddict[first_town])
    for first_word in worddict[first_town]:
        first_frequency = worddict[first_town][first_word]
        if first_frequency >= 3:
            first_word = first_word.decode('utf-8')
            if x % 10 == 0:
                print x
            x += 1
            for second_word in worddict[second_town]:
                second_frequency = worddict[second_town][second_word]
                if second_frequency >= 3:
                    second_word = second_word.decode('utf-8')
                    distance_and_paths = editdist(first_word, second_word)
                    distance = distance_and_paths[0]
                    frequency_of_pair = math.pow(first_frequency,frequency_factor) * math.pow(second_frequency,frequency_factor)
                    avg_min_dist_numer += distance * frequency_of_pair
                    avg_min_dist_denom += frequency_of_pair
                    if distance <= 2:
                        if distance not in distance_dictionary:
                            distance_dictionary[distance] = {}
                        paths = [filter(lambda t: t[0] != t[1], path) for path in distance_and_paths[1]]
                        for path in paths:
                            path = represent_path(path) 
                            if path not in distance_dictionary[distance]:
                                distance_dictionary[distance][path] = []
                            distance_dictionary[distance][path].append((first_word, second_word))
    return (distance_dictionary, float(avg_min_dist_numer/avg_min_dist_denom))

def write_dictionary(first_town, second_town, distance_info, maximum_distance, outfile):
    distance_dictionary = distance_info[0]
    avg_min_dist = distance_info[1]
    output = codecs.open(outfile,'w',encoding='utf-8')
    output.write('Comparison: ' + first_town + ' and ' + second_town + ' (' + str(avg_min_dist) + ')\n')
    for distance in distance_dictionary:
        if distance <= maximum_distance:
            output.write('\n' + str(distance) + ' changes:\n')
            paths = distance_dictionary[distance]
            for path in sorted(paths, key=lambda k: -len(paths[k])):
                output.write('\t\t' + path + '\t(' + str(len(paths[path])) + ')\n')
                for pair in paths[path]:
                    if True:
                    #not (pair[0].encode('utf-8') in worddict[second_town+'\n'] and pair[1].encode('utf-8') in worddict[first_town+'\n']):
                        output.write(path + '\t' + pair[0] + ', ' + pair[1] + '\n')

def represent_path(path):
    representation = ''
    for change in path:
        if change[0] == None:
            representation += '[-' + change[1] + ']'
        elif change[1] == None:
            representation += '[+' + change[0] + ']'
        else:
            representation += '[' + change[0] + change[1] + ']'
    return representation

if __name__ == '__main__':
    worddict = wordlist.wordlist(sys.argv[1])
    write_dictionary(sys.argv[2], sys.argv[3], compare(sys.argv[2]+'\n', sys.argv[3]+'\n'), int(sys.argv[5]), sys.argv[4])
