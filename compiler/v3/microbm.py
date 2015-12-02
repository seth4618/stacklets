import sys
import csv
from subprocess import *
import StringIO
from parse import *

titles = ["n","pthreads","called","forked","elapsed"]

def benchmark(n, pthreads, reps):
  with open('mb.csv', 'wb') as myfile:
    wr = csv.writer(myfile, quoting=csv.QUOTE_ALL)
    wr.writerow(titles)
    for i in range(0, reps):
      result = [n, pthreads]
      output = Popen(["./fib", str(n), str(pthreads)], stdout=PIPE).communicate()[0]
      buf = StringIO.StringIO(output)
      for line in buf:
          r = parse("called {called} times\n", line)
          if r is not None:
            result.append(r['called'])
            continue
          r = parse("forked {forked} times\n", line)
          if r is not None:
            result.append(r['forked'])
            continue
          r = parse("time elapsed: {elapsed}\n", line)
          if r is not None:
            result.append(r['elapsed'])
            continue
      print result
      wr.writerow(result)

if __name__ == "__main__":
  if len(sys.argv) < 3:
    sys.exit("error: ./microbm.py n pthreads <reps>")
  if len(sys.argv) >= 4:
    reps = int(sys.argv[3]);
  else:
    reps = 5
  benchmark(int(sys.argv[1]), int(sys.argv[2]), reps)
