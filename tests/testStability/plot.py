import matplotlib.pyplot as plt
import numpy as np
import sys

filename = sys.argv[1]

data = np.loadtxt(filename)

plt.figure()
plt.hist(data)
plt.xlabel("# emulations (mean/std=" + str(round(np.mean(data),1)) + "/" + str(round(np.std(data),1)) + ")")
plt.ylabel("p")
plt.show()
