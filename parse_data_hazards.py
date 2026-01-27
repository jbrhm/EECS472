import numpy as np
import matplotlib.pyplot as plt
import os
data_hazards = []
branch_acc = []
with open('output', 'r') as file: 
    for i, line in enumerate(file):
        try:
            if i % 2 == 0:
                curr = int(line.strip())
                data_hazards.append(curr)
            else:
                curr = float(line.strip())
                branch_acc.append(curr)
        except:
            pass

fig, ax = plt.subplots()

ax.hist(data_hazards, bins=10)

# Add labels and a title
ax.set_xlabel("# of Data Hazards")
ax.set_ylabel("Frequency")
ax.set_title("Data Hazards Per Benchmark")
ax.set_xlim(0, 200000)

# Display the plot
plt.show()
