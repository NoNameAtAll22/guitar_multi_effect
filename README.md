<h1>FPGA Guitar Multi-Effect Processor</h1>

A real-time digital guitar multi-effects processor implemented on FPGA, designed to process audio signals with multiple configurable effects such as distortion, tremolo, delay, and octave modulation.

<br>

<h2>Overview</h2>

This project implements a hardware-based audio effects processor for electric guitar signals using FPGA technology. The system captures an input audio signal, processes it through a configurable chain of digital effects, and outputs the modified signal in real time.

The design focuses on:

- Low-latency signal processing

- Modular DSP architecture

- Hardware-level control of audio effects

<br>

<h2>Features</h2>

- Real-time audio signal processing

- Configurable multi-effect chain

- Low-latency FPGA-based DSP

- Modular and extensible design  

<br>

<br>

<h2>Board schematic</h2>
<img width="1588" height="2086" alt="Schematic" src="https://github.com/user-attachments/assets/9a12207a-9713-43ab-8179-21b97fcd526a" />

<sub>_Note_1: U can use any button as long as it has data output_</sub>

<sub>_Note_2: A monostable button is provided for this project. If you want to use bistable button (latching) change button conditions in main.c_</sub>

<br>

_**Note for project**_

Project requires use of ESP-IDF. 

You can use any touch display as long as it has ILI9341 controller for display and XPT2046 controller for touch.

Make sure you use li-po battery with JST-PH connector. 



