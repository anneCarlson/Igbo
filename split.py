import sys
import re

def split(corpfile,orthfile,transfile,hicfile):
    corpus = open(corpfile,'r')
    ortho = open(orthfile,'w')
    trans = open(transfile,'w')
    hiccups = open(hicfile,'w')

    header = True
    prevline = ''
    currline = 0
    for line in corpus:
        currline += 1
        if line == '\n':
            header = not header
            ortho.write('\n')
            trans.write('\n')
        elif header:
            ortho.write(line)
            trans.write(line)
        elif not header:
            splitline = re.split('\t+',line,1)
            if 'Q' not in splitline[0]:
                splitprev = re.split('\t+',prevline,1)
                splitprev.append('***')
                if splitline[1][0] == '/':
                    if splitprev[1][0] != '/':
                        ortho.write(prevline)
                        trans.write(line)
                    else:
                        hiccups.write(str(currline) + '\t' + line)
                else:
                    if splitprev[1][0] != '/' and splitprev[1][0] != '*' and not (255261 <= currline <= 257742):
                        hiccups.write(str(currline) + '\t' + line)

        prevline = line

if __name__ == '__main__':
    split(sys.argv[1],sys.argv[2],sys.argv[3],sys.argv[4])
