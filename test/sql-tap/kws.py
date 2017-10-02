input_words = open('kw.txt').read().split()
output = open('output.txt', 'w')

for word in input_words:
    output.write('\t"' + word + '",\n')

output.close()
