import numpy as np
import matplotlib.pyplot as plt
import os
data_hazards = []
branch_acc = []
with open('output_file', 'r') as file: 
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

fig, ax = plt.subplots(2, 1)

print(data_hazards)
print(branch_acc)

ax[0].hist(data_hazards, bins=10)

# Add labels and a title
ax[0].set_xlabel("# of Data Hazards")
ax[0].set_ylabel("Frequency")
ax[0].set_title("Data Hazards Per Benchmark")
ax[0].set_xlim(0, 200000)

ax[1].hist(branch_acc, bins=10)

# Add labels and a title
ax[1].set_xlabel("FTBNT Accuracy")
ax[1].set_ylabel("Frequency")
ax[1].set_title("FTBNT Accuracy (%)")
ax[1].set_xlim(0, 1)

# Display the plot
plt.show()
