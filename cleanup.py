import sys
import re

def fixlines(corpfile,outfile):
    corpus = open(corpfile,'r')
    output = open(outfile,'w')

    state = ''
    town = ''
    village = ''
    lga = ''
    coords = (0,0)
    discourse = []
    other = []
    header = True

    x = 0
    for line in corpus:
        x += 1
        if line.split() == []:
            if header:
                discwrite = ''
                otherwrite = ''
                for topic in discourse:
                    discwrite += topic + ', '
                for otherline in other:
                    otherwrite += otherline
                output.write('State:\t' + state + '\nL.G.A:\t' + lga + '\nTown:\t' + town + '\nVillage:\t' + village + '\nCoordinates:\t' + str(coords[0]) + ',' + str(coords[1]) + '\nDiscourse:\t' + discwrite[:len(discwrite)-2] + '\n' + otherwrite)
            header = not header
            state = ''
            town = ''
            village = ''
            lga = ''
            discourse = []
            other = []
            output.write('\n')
        else:
            if header:
                splitline = re.split('\s*[:_\t]+\s*|\s{2,}|\n',line,re.UNICODE)
                if re.match('Discourse',line) or re.match('Area of Discusion',line) or re.match('Topic',line):
                    for topic in re.split('\s*[,/&.]\s*|\s+and\s+|\s{2,}|\n',splitline[1]):
                        discourse.append(topic)
                elif re.match('State',line):
                    state = splitline[1]
                elif re.match('LGA',line) or re.split('\W+',line,re.UNICODE)[:3] == ['L','G','A'] or re.match('Local',line):
                    lga = splitline[1]
                elif re.match('Town',line):
                    town = splitline[1]
                elif re.match('Village',line):
                    village = splitline[1]
                elif re.match('Coord',line):
                    coordstring = re.split(',*\s*',splitline[1])
                    coords = (float(coordstring[0]),float(coordstring[1]))
                else:
                    other.append(line)   
            else:
                output.write(line)

def timestamp(corpfile,outfile):
    corpus = open(corpfile,'r')
    output = open(outfile,'w')
    
    header = True
    for line in corpus:
        q = False
        a = False
        if line.split() == []:
            header = not header
            output.write('\n')
            hh = 'xx'
            mm = 'xx'
            ss = 'xx'
        elif header:
            output.write(line)
        elif not header:
            timed = True
            first = line.split()[0]
            time = re.split('[A-Z]?[:;.A-Z]',first)
            if re.match('\A\S',line):
                if time[0] == '':
                    time = time[1:]
                if re.search('(\dQ)|(Q[:.]?\d)',first):
                    q = True
                elif re.search('(\d[Aa])|(A[:.]?\d)',first):
                    a = True
                if re.match('[QA]?[:.]?\d?\d[:;.]\d\d[:;.]\d\d',first):
                    hh = time[0][:2]
                    mm = time[1][:2]
                    ss = time[2][:2]
                elif re.match('[QA]?[:;.]?\d\d[:;.]\d\d',first):
                    mm = time[0][:2]
                    ss = time[1][:2]
                elif re.match('[QA]?[:;.]?\d\d',first):
                    ss = time[0][:2]
                else:
                    timed = False
            else:
                timed = False

            stamp = hh + ':' + mm + ':' + ss
            if timed:
                splitline = line.split(None,1)
                splitline.append('\n')
                line = splitline[1]
            if q:
                stamp += 'Q'
            elif a:
                stamp += 'A'

            output.write(stamp + '\t' + line)

if __name__ == '__main__':
    fixlines(sys.argv[1],sys.argv[2])
    timestamp(sys.argv[2],sys.argv[3])
