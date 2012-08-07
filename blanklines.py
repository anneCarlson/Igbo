corp = open('truncated.txt','r')
x = 0
y = 0
z = 0
for line in corp:
    x += 1
    z += 1
    if line.split() == []:
        print str(x) + ',' + str(z)
        x = 0
        y += 1
print y
