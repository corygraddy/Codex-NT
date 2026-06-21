# VCA Plugin for Disting NT

A simple, low-CPU Voltage Controlled Amplifier.

## Features

*   **Level Control**: Manually set the gain from 0% to 100%.
*   **Curve Selection**: Choose between **Linear** and **Exponential** response curves for the level control.
*   **CV Modulation**: Modulate the level with an external CV source.
*   **CV Attenuverter**: Scale and invert the incoming CV modulation.

## Parameters

1.  **Level**: The base gain of the VCA.
2.  **Curve**: Toggles between Linear and Exponential gain response.
3.  **CV In**: Assigns a CV input bus to modulate the level.
4.  **CV Amt**: Attenuates or inverts the CV signal. At 100%, 1V of CV corresponds to a 10% change in level.
5.  **Audio In**: The audio signal to be processed.
6.  **Audio Out**: The processed audio output.

## Usage

This is a standard VCA. Patch an audio source to **Audio In** and connect **Audio Out** to your signal chain. Use the **Level** parameter to control the volume. For dynamic control, patch an envelope or LFO to a CV input and select that bus for the **CV In** parameter.
