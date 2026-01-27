import numpy as np
import matplotlib.pyplot as plt
import os
data_hazards = []
branch_acc = []
with open('output', 'r') as file: 
    for i, line in enumerate(file):
        try:
            curr = int(line.strip())
            if i % 2 == 0:
                l.append(curr)
            else:
        except:
            pass

fig, ax = plt.subplots()

ax.hist(l, bins=10)

# Add labels and a title
ax.set_xlabel("# of Data Hazards")
ax.set_ylabel("Frequency")
ax.set_title("Data Hazards Per Benchmark")
ax.set_xlim(0, 200000)

# Display the plot
plt.show()
