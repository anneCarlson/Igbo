# coding=utf-8

import sys
import re

def filterchars(infile,outfile):
    intext = open(infile,'r')
    outtext = open(outfile,'w')

    question = False
    for line in intext:
        splitline = line.split()                
        while len(splitline) > 0 and splitline[0] != '/' and re.match('[\W0-9_]',splitline[0],re.UNICODE):
            splitline = splitline[1:]
        while len(splitline) > 0 and re.match('[\W0-9_]',splitline[len(splitline)-1],re.UNICODE):
            splitline = splitline[:len(splitline)-1]

        if len(splitline) > 0:
            if re.match('[Q̄Q́Q][\W0-9_]*',splitline[0],re.UNICODE):
                question = True
            elif re.match('.?A[\W0-9_]*$',splitline[0]):
                question = False
                splitline = splitline[1:]
                if len(splitline) > 0:
                    if splitline[0] == 'Q:':
                        question = True
                    if splitline[0] == 'A:':
                        question = False
                        splitline = splitline[1:]

        if not question:
            toWrite = ''
            for word in splitline:
                toWrite += word + ' '
            outtext.write(toWrite + '\n')
                

def main(infile,outfile):
    filterchars(infile,outfile)

if __name__ == '__main__':
    main(sys.argv[1],sys.argv[2])
