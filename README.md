# 💍 Ring Sizer for Flipper Zero

![Flipper Zero](https://img.shields.io/badge/Device-Flipper%20Zero-orange?style=for-the-badge&logo=flipperzero) ![Build Status](https://img.shields.io/badge/Build-Passing-brightgreen?style=for-the-badge) ![License](https://img.shields.io/badge/License-MIT-blue?style=for-the-badge)

**Ring Sizer** is a precision utility application designed for the Flipper Zero. It transforms your device's display into a calibrated measuring tool, allowing you to determine ring sizes physically or convert measurements between US and EU standards instantly.

> [!WARNING] 
> **Calibration & Hardware Notice**
> This application is strictly calibrated for the **default, built-in Flipper Zero display**.
> * **Do not use via Screen Casting:** Using qFlipper or casting the screen to a phone/PC will result in inaccurate physical measurements due to screen scaling differences.
> * **Hardware Modifications:** Any modifications that alter the screen's physical dimensions or resolution will render measurements inaccurate.

---

## 📸 Screenshots

| Measure Mode | Display Mode |
|:---:|:---:|
| | |
| *Real-time sizing* | *Unit conversion* |

---

## ✨ Features

* **Physical Measurement:** Place a ring directly on the screen to measure its diameter and corresponding size.
* **Dual Standards:** Automatically calculates and displays sizes in both **US** and **EU** standards.
* **Digital Reference:** Input a known size to visualize the diameter on screen.
* **Precise Controls:** Fine-tune measurements in millimeter increments.

---

## 📖 Usage Guide

### 1. Measure Ring (Physical Sizing)
Use this mode to find the size of a physical ring you possess.

1.  Select **Measure Ring** from the main menu.
2.  Place your ring flat against the Flipper Zero screen.
3.  Use the **UP** and **DOWN** buttons to adjust the size of the on-screen circle until it perfectly matches the inner diameter of your ring.
4.  Read the results displayed on the screen:
    * **Diameter (mm)**
    * **US Size**
    * **EU Size**

> **Screenshot Tip:** *Add a photo here showing a ring sitting on the Flipper screen.*
> `![Ring on Screen](url_to_your_image)`

### 2. Display Ring (Reference & Conversion)
Use this mode if you know a size and want to see the measurements or convert between units.

1.  Select **Display Ring** from the main menu.
2.  Use the **LEFT** and **RIGHT** buttons to toggle the input unit between **US** and **EU**.
3.  Use the **UP** and **DOWN** buttons to adjust the size value.
4.  Press **OK** to render the circle and view the full measurement breakdown.

---

## 🎮 Controls

| Key | Context | Action |
| :--- | :--- | :--- |
| **⬆️ UP** | All Modes | Increase Circle Size / Value |
| **⬇️ DOWN** | All Modes | Decrease Circle Size / Value |
| **⬅️ LEFT** | Display Mode | Switch Unit Standard (US/EU) |
| **➡️ RIGHT** | Display Mode | Switch Unit Standard (US/EU) |
| **🆗 OK** | Display Mode | Confirm & Render Circle |
| **↩️ BACK** | All Modes | Return to Menu / Exit |

---

## 📦 Installation

You can install this application by compiling the firmware or dropping the `.fap` file into your applications directory.

1.  Clone this repository.
2.  Connect your Flipper Zero via USB.
3.  Copy the compiled application to `SD Card/apps/Tools`.
