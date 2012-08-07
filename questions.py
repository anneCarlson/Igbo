import sys
import re

def questions(corpfile,outfile,runfile):
    corpus = open(corpfile,'r')
    output = open(outfile,'w')
    runs = open(runfile,'w')

    header = True
    currline = 0
    consec = 0
    q = False
    a = False
    for line in corpus:
        if consec == 50:
            runs.write(str(currline) + '\t' + line)
        currline += 1
        consec += 1
        if line.split() == []:
            header = not header
            q = False
            a = False
            output.write('\n')
        elif header:
            output.write(line)
        elif not header:
            splitline = line.split(None,1)
            splitline.append('\n')
            timestamp = splitline[0]

            line = splitline[1]
            if re.search('[x\d]Q',timestamp):
                q = True
                a = False
                line = 'Q\t' + line
                timestamp = timestamp[:len(timestamp)-1]
            elif re.search('[x\d]A',timestamp):
                a = True
                q = False
                line = 'A\t' + line
                timestamp = timestamp[:len(timestamp)-1]
            
            splitline = line.split(None,1)
            splitline.append('\n')
            if re.match('Q([.:]?(\t|\s{2,})|[.:])',line):
                q = True
                a = False
                consec = 0
                line = splitline[1]
            elif re.match('A([.:]?(\t|\s{2,})|[.:])',line):
                a = True
                q = False
                consec = 0
                line = splitline[1]

            if q:
                timestamp += 'Q'
            elif a:
                timestamp += 'A'
            #else:
                #timestamp += 'X'

            output.write(timestamp + '\t' + line)

if __name__ == '__main__':
    questions(sys.argv[1],sys.argv[2],sys.argv[3])
