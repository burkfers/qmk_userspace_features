"""
Python hydrogen notebook to desing mouse acceleration sigmoid functions.

To run this, create a venv, activate it and install its dependncies:

```shell
python -m venv --prompt maccel .venv
# activate the venv, command printed when created
pip install pandas plotly ipywidgets jupytext
```

and then either:

- convert this file to notebook .ipynb format and open this file 
  with `jupyter-lab` command:
- open it directly in VSCode with respective jupyter extension, or
- download, install and open it with https://nteract.io/.
"""

# %%*
from typing import NamedTuple

import ipywidgets as ipyw
import numpy as np
import pandas as pd
import plotly.express as px

# %%
factor_floor = 1
x_step = 0.1


def sigmoid_x_from_y_ratio(steepness, midpoint, y_ratio) -> float:
    """Find the x-point where the curve reaches `y_ratio` of max-y."""
    assert 0 <= y_ratio <= 1, y_ratio

    # The exp() reaches 0 or 1 when -+inf.
    y_ratio = min(max(y_ratio, 0.005), 0.995)
    x = midpoint - np.log((1.0 / y_ratio) - 1.0) / steepness

    return x


def sigmoid(
    steepness, velocity_midpoint, factor_min, factor_max
) -> tuple[np.array, np.array]:
    # Plot till the curve reaches 99.5% of max-y.
    x_end = sigmoid_x_from_y_ratio(steepness, velocity_midpoint, 0.995)
    x = np.arange(0, x_end, x_step)

    e = np.exp(-steepness * (x - velocity_midpoint))
    y = factor_min + (factor_max - factor_min) / (1 + e)

    y = np.where(y < factor_floor, factor_floor, y)

    return x, y


def sigmoid_mix(
    low_steepness,
    high_steepness,
    velocity_midpoint,
    factor_min,
    factor_max,
    mix_ratio,
) -> pd.DataFrame:
    """
    :param mix_ratio:
        it decides where the 2 sigmoids overlap vs y-ratio,
        so that when:
        - 1: mix everywhere
        - 0.5: mix from 0.25% of low-sigmoid output till 0.75% of high's
        - 0: no mix, hard brake in midpoint
    """
    assert 0 <= mix_ratio <= 1, mix_ratio

    x, y = sigmoid(low_steepness, velocity_midpoint, factor_min, factor_max)
    dfl = pd.DataFrame({"x": x, "ylow": y}).set_index("x")

    x, y = sigmoid(high_steepness, velocity_midpoint, factor_min, factor_max)
    dfh = pd.DataFrame({"x": x, "yhigh": y}).set_index("x")

    df = pd.concat((dfl, dfh), axis=1)

    ## Split weigts in 3 segments (no-mix, mix, no-mix) around x-low/high.
    #
    mix_ratio /= 2
    xlow = sigmoid_x_from_y_ratio(low_steepness, velocity_midpoint, 0.5 - mix_ratio)
    xhigh = sigmoid_x_from_y_ratio(high_steepness, velocity_midpoint, 0.5 + mix_ratio)
    df[["wlow", "whigh"]] = np.NaN
    low_only = df.index < xlow
    high_only = df.index > xhigh
    x_mix = (df.index >= xlow) & (df.index <= xhigh)
    df.loc[low_only, "wlow"] = 1.0
    df.loc[low_only, "whigh"] = 0.0
    df.loc[high_only, "wlow"] = 0.0
    df.loc[high_only, "whigh"] = 1.0
    mix_weigts = np.linspace(0, 1, x_mix.sum(), endpoint=True)
    df.loc[x_mix, "wlow"] = 1 - mix_weigts
    df.loc[x_mix, "whigh"] = mix_weigts

    df["y"] = df.wlow * df.ylow + df.whigh * df.yhigh
    return df


def plot_accel_curve(
    low_steepness=1.0,
    high_steepness=1.0,
    velocity_midpoint=5,
    factor_min=0.9,
    factor_max=6.0,
    mix_ratio=0.5,
):
    df = sigmoid_mix(
        low_steepness,
        high_steepness,
        velocity_midpoint,
        factor_min,
        factor_max,
        mix_ratio,
    )

    params_color = "green"
    params_attrs = {
        "line_dash": "dash",
        "line_color": params_color,
        "annotation_font_color": params_color,
    }
    fig = px.line(
        df["y ylow yhigh".split()],
        title="<b>Mouse-aceleration</b>"
        f"<br>low_steepness: {float(low_steepness):0.2}"
        f"<br>high_steepness: {float(high_steepness):0.2}",
    )
    fig["data"][0]["line"]["width"] = 4
    fig["data"][1]["line"]["dash"] = "dash"
    fig["data"][2]["line"]["dash"] = "dash"
    fig.update_xaxes(title="mouse_input_velocity")
    fig.update_yaxes(title="velocity_factor")
    fig.add_hline(factor_min, annotation_text="factor_min", **params_attrs)
    fig.add_hline(factor_max, annotation_text="factor_max", **params_attrs)
    fig.add_vline(
        velocity_midpoint,
        annotation_text="velocity_midpoint",
        annotation_position="right",
        **params_attrs,
    )
    valid_factors = df.y > factor_floor
    if valid_factors.sum() > 0:
        x_break = valid_factors.idxmax()
        fig.add_annotation(
            x=x_break,
            y=1,
            text="take-off point",
            arrowcolor=params_color,
            font_color=params_color,
        )

    return fig


plot_accel_curve()

# %%
from ipywidgets import interact

# %%
interact(
    plot_accel_curve,
    low_steepness=(0.2, 6, 0.1),
    high_steepness=(0.2, 6, 0.1),
    velocity_midpoint=(1, 10, 0.5),
    factor_min=(-2, 1, 0.05),
    factor_max=(2, 20, 1),
    mix_ratio=(0, 1, 0.1),
)


# %%
