import sys

def truncate(corpfile,outfile):
    corpus = open(corpfile,'r')
    output = open(outfile,'w')

    for x in range(394529):
        line = corpus.readline()
        #if 255343 <= x <= 257827 and x%2 == 0:
            #line = '/' + line
        output.write(line)
    output.write('\n')

def main(corpfile,truncfile):
    truncate(corpfile,truncfile)

if __name__ == '__main__':
    main(sys.argv[1],sys.argv[2])
