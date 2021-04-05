import json
import sys
from matplotlib import rcParams
rcParams['font.sans-serif'] = ['Microsoft YaHei']
rcParams['axes.unicode_minus'] = False
import matplotlib.pyplot as plt
import numpy as np
import matplotlib.lines as mlines

print("后处理：生成SMM2 分数变化图")
 
livefile = r"E:\Work\Livestream\final\output_live\_debug.json"
legend_pos = 'upper left'
if len(sys.argv) >= 2:
    livefile = sys.argv[1]
if len(sys.argv) >= 3:
    legend_pos = sys.argv[2]

print("输入文件 " + livefile)

# read live json
fp = open(livefile, "r")
livejson = fp.read()
fp.close()
live = json.loads(livejson)

# read smm2 file
smm2_file_path:str = live["smm2_file"]
fp = open(smm2_file_path, "r")
smm2_file = fp.read()
fp.close()

# load all scores
smm2 = json.loads(smm2_file)
if len(smm2) < 2:
    print("比赛场次不足")
    exit(0)

ratings = []
for i in range(len(smm2)):
    match = smm2[i]
    if i == 0:
        ratings.append(match["ratingFrom"])
    ratings.append(match["ratingTo"])

# draw all scores by segment
plt.figure(figsize=(8, 4.5), dpi=240)
colors = [[203/255, 1/255, 47/255], [221/255, 200/255, 11/255], [166/255, 95/255, 194/255]]
gray = [127/255, 127/255, 127/255]
ratingFrom = 0
ratingTo = 0
for i in range(len(smm2)):
    match = smm2[i]
    ratingFrom = match["ratingFrom"]

    if ratingTo != ratingFrom and ratingTo != 0:
        plt.plot([-0.5 + i, -0.5 + i], [ratingTo, ratingFrom], linestyle='-', marker='.', color=gray, markersize=8)

    ratingTo = match["ratingTo"]
    matchResult = match["result"]
    plt.plot([-0.5 + i, 0.5 + i], [ratingFrom, ratingTo], linestyle='-', marker='.', color=colors[matchResult - 1], markersize=8)

plt.annotate(np.max(ratings), (np.argmax(ratings), np.max(ratings)+1), ha='center', va='bottom')
plt.annotate(np.min(ratings), (np.argmin(ratings), np.min(ratings)-1), ha='center', va='top')
plt.annotate(ratings[0], (-1.5, ratings[0]), ha='right', va='center')
plt.annotate(ratings[-1], (len(ratings), ratings[-1]), ha='left', va='center')

plt.xlim([-6, len(ratings) + 5])
plt.ylim([np.min(ratings)-10, np.max(ratings)+10])

plt.annotate("@Schvvarzer", (len(ratings)-2, np.min(ratings) - 6), ha='center', va='center')

plt.xlabel("局数")
plt.ylabel("对战积分")

plt.legend(handles=[
    mlines.Line2D([], [], color=gray, marker='.', markersize=8, label="断线"),
    mlines.Line2D([], [], color=colors[0], marker='.', markersize=8, label="胜利"),
    mlines.Line2D([], [], color=colors[1], marker='.', markersize=8, label="失败"),
    mlines.Line2D([], [], color=colors[2], marker='.', markersize=8, label="平局") 
], loc=legend_pos)

# save to output
output_path = smm2_file_path.replace(".json", "/" + legend_pos.replace(" ", "_") + "_ratingChart.png")
plt.savefig(output_path)
# plt.show()
print("输出文件 " + output_path)