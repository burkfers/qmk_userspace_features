## Python hydrogen notebook to desing mouse acceleration sigmoid functions.
#
# Run this to install dependncies (uncomment package to run it in VSCode):
#     pip install pandas plotly # notebook
# or explore it in Wolfram:
#     https://www.wolframalpha.com/input?i=plot+1%2B%28c-1%29%2F%281+%2B+e%5E%28-%28x-b%29*a%29%29+with+a%3D0.9+with+b%3D5+with+c%3D4+from+x%3D-0.1+to+10+from+y%3D-0.5+to+4.5
#
# %%*
from typing import NamedTuple

import numpy as np
import plotly.express as px


# %%
def sigmoid(x, steepness, velocity_midpoint, factor_max):
    factor_min = 1
    e = np.exp(-steepness * (x - velocity_midpoint))
    y = factor_min + (factor_max - factor_min) / (1 + e)

    return y


def plot_accel_curve(x, y, steepness, velocity_midpoint, factor_max):
    labels = {
        "x": "mouse input velocity",
        "y": "velocity factor",
    }
    params_color = "green"
    params_attrs = {
        "line_dash": "dash",
        "line_color": params_color,
        "annotation_font_color": params_color,
    }
    fig = px.line(x=x, y=y, labels=labels)
    fig.add_hline(factor_max, annotation_text="factor_max", **params_attrs)
    fig.add_vline(
        velocity_midpoint,
        annotation_text="velocity_midpoint",
        annotation_position="right",
        **params_attrs
    )

    return fig


x = np.arange(0, 10, 0.1)

steepness = 1.0
velocity_midpoint = 5
factor_max = 6.0

y = sigmoid(x, steepness, velocity_midpoint, factor_max)
plot_accel_curve(x, y, steepness, velocity_midpoint, factor_max)

# %%
