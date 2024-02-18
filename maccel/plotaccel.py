"""
Python hydrogen notebook to desing mouse acceleration sigmoid functions.

To run this, create a venv, activate it and install its dependncies:

```shell
python -m venv --prompt maccel .venv
# activate the venv, command printed when created
pip install pandas plotly ipywidgets
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


def sigmoid_x_from_y_ratio(steepness, midpoint, y_ratio) -> float:
    """Find the x-point where the curve reaches `y_ratio` of max-y."""
    x = midpoint - np.log((1.0 / y_ratio) - 1.0) / steepness

    return x


def sigmoid(
    steepness, velocity_midpoint, factor_min, factor_max
) -> tuple[np.array, np.array]:
    # Plot till the curve reaches 99.5% of max-y.
    x_end = sigmoid_x_from_y_ratio(steepness, velocity_midpoint, 0.995)
    x = np.arange(0, x_end, 0.1)

    e = np.exp(-steepness * (x - velocity_midpoint))
    y = factor_min + (factor_max - factor_min) / (1 + e)

    y = np.where(y < factor_floor, factor_floor, y)

    return x, y


def plot_accel_curve(
    steepness=1.0,
    velocity_midpoint=5,
    factor_min=0.9,
    factor_max=6.0,
):
    x, y = sigmoid(steepness, velocity_midpoint, factor_min, factor_max)

    df = pd.DataFrame({"x": x, "y": y})
    df = df.set_index("x")
    params_color = "green"
    params_attrs = {
        "line_dash": "dash",
        "line_color": params_color,
        "annotation_font_color": params_color,
    }
    fig = px.line(
        df.y,
        title="<b>Mouse-aceleration</b>"
        f"<br>steepness_low: {float(steepness_low):0.2}",
    )
    fig.update_layout(showlegend=False)
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
    valid_factors = (df.y > factor_floor)
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

interact(
    plot_accel_curve,
    steepness=(0.2, 2, 0.1),
    velocity_midpoint=(1, 10, 0.5),
    factor_min=(-2, 1, 0.05),
    factor_max=(2, 16, 1),
)

# %%
