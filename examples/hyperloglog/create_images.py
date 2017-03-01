import math
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

df = pd.read_csv("precision_comparison")
df["relative_error"] = abs(df.hyperloglog_count / df.exact_count - 1)
fig, axes = plt.subplots(math.ceil((15-4)/3.0), 3, figsize=(6, 7))
for i in range(4, 15):
    img = sns.boxplot(x='samplesize', y='relative_error',
                      data=df[df.hyperloglog_precision == i],
                      ax=axes.flat[i-4])
    axes.flat[i-4].set_title('Precision {}'.format(i))
axes.flat[11].axis("off")
fig.tight_layout()
plt.savefig("precision_comparison.pgf")

df = pd.read_csv("~/code/thrill/build/single_precision")
df["relative_error"] = abs(df.hyperloglog_count / df.exact_count - 1)
grouped = df.groupby("samplesize")
plotFrame = pd.DataFrame({
                          "5%": grouped.quantile(0.05)["relative_error"],
                          "mean": grouped.mean()["relative_error"],
                          "95%": grouped.quantile(0.95)["relative_error"]})
plotFrame.plot(title="Precision 14", figsize=(6, 2))
plt.savefig("single_precision.pgf")
