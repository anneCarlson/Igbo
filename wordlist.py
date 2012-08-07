import sys

def wordlist(corpfile):
    corpus = open(corpfile,'r')

    worddict = {}
    header = True
    coords = ''
    for line in corpus:
        splitline = line.split('\t',1)
        if line == '\n':
            header = not header
        elif header:
            if splitline[0] == 'Coordinates:':
                coords = splitline[1]
                if coords not in worddict:
                    worddict[coords] = {}
        elif not header:
            splitline = line.split('\t',1)
            timestamp = splitline[0]
            content = splitline[1]
            coordsdict = worddict[coords]

            for word in content.split():
                if word != '/':
                    if word not in coordsdict:
                        coordsdict[word] = 0
                    coordsdict[word] += 1

    return worddict

def printlist(worddict,outfile):
    output = open(outfile,'w')

    for coords in sorted(worddict):
        output.write('Coordinates:\t' + coords)
        coordsdict = worddict[coords]
        for (word,count) in sorted(coordsdict.iteritems(),key=lambda (k,v): (-v,k)):
            output.write(str(count) + '\t' + word + '\n')
        output.write('\n')

if __name__ == '__main__':
    printlist(wordlist(sys.argv[1]),sys.argv[2])
