'''
number of successful experiments and exceptions are then written to
filenameRun and filenameExc
'''

import sys
sys.path.append('$SYMAP2IC_PATH/src/helpers')
import os
import subprocess
import numpy as np
import netpower

filenameRun = 'run.txt'
filenameExc = 'exceptions.txt'
filenameOther = 'other.txt'

def run(hostname, port):
    if os.path.isfile(filenameRun):
        os.remove(filenameRun)
    if os.path.isfile(filenameExc):
        os.remove(filenameExc)
    if os.path.isfile(filenameOther):
        os.remove(filenameOther)

    pathTests = os.path.join(os.environ['SYMAP2IC_PATH'], 'bin/tests')

    while True:
        netpower.interrupt_port(hostname, port)

        cmd = ['./test-main', '--gtest_filter=*DecorrNetworkInf*']
        proc = subprocess.Popen(cmd, cwd=pathTests, stdout=subprocess.PIPE)

        lines = []
        while proc.poll() is None:
            line = proc.stdout.readline()
            lines.append(line)
            print line,
        print ''

        returncode = proc.wait()
        listException = [line for line in lines if 'exception' in line]

        if len(listException) == 0:
            with open(filenameOther, 'a') as otherFile:
                for line in lines:
                    otherFile.write(line + '\n')
            continue

        textException = listException[0]
        lineNumber = lines.index(textException)
        runs = int(lines[lineNumber - 2])

        with open(filenameRun, 'a') as runsFile:
            runsFile.write(str(runs) + '\n')
        with open(filenameExc, 'a') as textFile:
            textFile.write(textException)

if __name__ == "__main__":
    import sys
    assert(len(sys.argv) == 3), 'provide host name (e.g. netpower) and port number (e.g. 1)'
    run(str(sys.argv[1]), int(sys.argv[2]))
