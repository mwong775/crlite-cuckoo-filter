import numpy as np
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick
import csv

lup_round = []
fp = []
percent_fp = []

rows = ('lookup round', 'false positives', 'percent fp\'s')

with open('../csv/cert_fp_lookup.csv') as file:
    reader = csv.DictReader(file, delimiter=',')
    for row in reader:
        lup_round.append(int(row['lookup round']))
        fp.append(int(row[' false positives']))
        percent_fp.append(float(row[' percent fp\'s']))

print(lup_round)
print(fp)
print(percent_fp)

fp_decr = []

for i in range(1, len(fp)):
    prev = fp[i - 1]
    current = fp[i]
    decr = (prev - current) * 100 / prev
    fp_decr.append(decr)
fp_decr  = np.array([x * -1 for x in fp_decr])
print(fp_decr)
# lup_round.pop(0)

fig, fp1 = plt.subplots()
fp1.plot(lup_round, percent_fp, marker = 'o', label="false positives")
fp1.legend(loc = "upper right")
fp1.set_xlabel("Lookups")
fp1.set_ylabel("False Positive Rates")
fp1.yaxis.set_major_formatter(mtick.PercentFormatter())
plt.title("False Positive Rates Per Lookup")

fp2 = fp1.twinx()
fp2.plot(lup_round, fp)
fp2.set_ylabel("False Positives")

fig.tight_layout()

"""
cell_text = [lup_round, fp, np.array(['%1.5f' % x for x in percent_fp])]

# Add a table at the bottom of the axes
table = plt.table(cellText=cell_text,
                        rowLabels=rows,
                        cellLoc='center',
                        rowLoc='center',
                      loc='bottom', 
                      bbox=[0.2, -0.5, 0.8, 0.32])

table.set_fontsize(20)
table.scale(1.5, 1.5)  # may help

# Adjust layout to make room for the table:
plt.subplots_adjust(left=0.2, bottom=0.2)
"""

plt.grid(True)
plt.show()
# fig.savefig("../figures/cert_fp_lookup.png")

exit()
