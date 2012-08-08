import sys
import math
import re
import wordlist

def distancechart(wordlist,outfile):
    output = open(outfile,'w')
    distances = {}
    sorted_wordlist = sorted(wordlist)
    for first_coords in sorted_wordlist:
        first_coord_list = re.split('[,\n]',first_coords)
        first_latitude = float(first_coord_list[0])
        first_longitude = float(first_coord_list[1])
        for second_coords in sorted_wordlist:
            second_coord_list = re.split('[,\n]',second_coords)
            second_latitude = float(second_coord_list[0])
            second_longitude = float(second_coord_list[1])

            latitude_difference = second_latitude-first_latitude
            longitude_difference = second_longitude-first_longitude
            pythagorean_distance = math.sqrt(math.pow(latitude_difference,2) + math.pow(longitude_difference,2))
            if pythagorean_distance > 0:
                distances[pythagorean_distance] = ((first_latitude,first_longitude),(second_latitude,second_longitude))

    for distance in sorted(distances):
        coordinate_pair = distances[distance]
        output.write('(' + str(coordinate_pair[0][0]) + ',' + str(coordinate_pair[0][1]) + '), (' + str(coordinate_pair[1][0]) + ',' + str(coordinate_pair[1][1]) + ')\t' + str(distance) + '\n')

if __name__ == '__main__':
    distancechart(wordlist.wordlist(sys.argv[1]),sys.argv[2])
