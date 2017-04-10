import math
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

df = pd.read_csv("precision_comparison")
df["relative_error"] = abs(df.hyperloglog_count / df.exact_count - 1)
fig, axes = plt.subplots(math.ceil((15-4)/3.0), 3, figsize=(5.7, 7))
sns.set(font_scale=0.9)
for i in range(4, 15):
    img = sns.boxplot(x='samplesize', y='relative_error',
                      data=df[df.hyperloglog_precision == i],
                      ax=axes.flat[i-4],
                      linewidth=0.3)
    img.tick_params(axis='y', pad=3)
    img.set_xlabel('')
    img.set_ylabel('')
    # everything breaks when I try to use LogFormatter here so we just use this ugly solution
    img.set_xticklabels(["10e{:d}".format(int(math.log10(int(t.get_text()))))
                         for t in img.get_xticklabels()])
    img.set_title('Precision {}'.format(i))
axes.flat[11].axis("off")
fig.tight_layout()
plt.savefig("precision_comparison.pgf")

df = pd.read_csv("~/code/thrill/build/single_precision")
df["relative_error"] = abs(df.hyperloglog_count / df.exact_count - 1)
grouped = df.groupby("samplesize")
plotFrame = pd.DataFrame({
                          "5% quantile": grouped.quantile(0.05)["relative_error"],
                          "mean": grouped.mean()["relative_error"],
                          "95% quantile": grouped.quantile(0.95)["relative_error"]})
axis = plotFrame.plot(title="Precision 14", figsize=(5.77, 3.5))
axis.set_xlabel('sample size', fontsize=10)
axis.set_ylabel('relative error')
plt.savefig("single_precision.pgf")
