import numpy as np
import matplotlib.pyplot as plt
import os
data_hazards = []
branch_acc = []
bb_ilp = []
with open('output_file', 'r') as file: 
    for i, line in enumerate(file):
        try:
            if i % 3 == 1:
                curr = int(line.strip())
                data_hazards.append(curr)
            elif i % 3 == 2:
                curr = float(line.strip())
                branch_acc.append(curr)
            else:
                curr = float(line.strip())
                bb_ilp.append(curr)
        except:
            pass

# fig, ax = plt.subplots(2, 1)

print(data_hazards)
print(branch_acc)
print(bb_ilp)

# plt.hist(data_hazards, bins=10)
# plt.xlabel("# of Data Hazards")
# plt.ylabel("Frequency")
# plt.title("Data Hazards Per Benchmark")
# plt.xlim(0, 100000)

# plt.hist(branch_acc, bins=10)
# plt.xlabel("FTBNT Accuracy")
# plt.ylabel("Frequency")
# plt.title("FTBNT Accuracy (%)")
# plt.xlim(0, 1)

plt.hist(bb_ilp, bins=10)
plt.xlabel("ILP")
plt.ylabel("Frequency")
plt.title("ILP Per Block Body")
plt.xlim(0, 15)

plt.show()
