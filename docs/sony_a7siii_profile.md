# Camera Profile: Sony Alpha 7S III (ILCE-7SM3)

Technical reference for the cinematic POC and future integration of the Sony Alpha 7S III sensor profile.

## 📊 Technical Specifications

| Parameter | Value | Definition & Engine Role |
| :--- | :--- | :--- |
| **Sensor Type** | 35mm Full-Frame CMOS | Impacts focal length multipliers and Depth of Field (DOF) depth. |
| **Sensor Size** | 35.6 x 23.8 mm | Physical surface used for circle of confusion and bokeh scale. |
| **DOF Simulation** | 35mm f/1.8 | Shallow depth of field; focal distance 16.0m, range 4.0m. |
| **Output Resolution** | 12.1 MP (4240 x 2832) | Pixel pitch reference for grain/noise frequency. |
| **Dynamic Range** | 15+ stops (S-Log3) | High Dynamic Range (HDR) ceiling; maps to ACES highlight roll-off. |
| **Dual Base ISO** | 640 / 12,800 | Optimal signal-to-noise ratio points; triggers gain simulation shifts. |
| **Color Space** | S-Gamut3.Cine | Wide-gamut primaries; target for color grading transforms. |
| **Gamma Curves** | S-Log3, S-Cinetone | Logarithmic vs Cine-style transfer functions (EOTF). |

## 🔍 Parameter Definitions

### 1. Sensor Size & Form Factor

In our engine, the **Full-Frame** format serves as the "1.0x" reference. It dictates the relationship between focal length (mm) and Field of View (FOV). A 35mm lens on this sensor provides a "standard" cinematic perspective without crop factor.

### 2. S-Log3 (Gamma Curve)

A quasi-logarithmic curve designed to preserve the maximum dynamic range of the sensor. It compresses the highlights and lifts shadows to fit more information into the 10-bit/12-bit container. In rendering, this is our "Linear to Log" intermediate step before grading.

### 3. Dual Base ISO (Dual Gain)

The Alpha 7S III uses two distinct analog circuits. Low Gain (ISO 640) for bright scenes and High Gain (ISO 12,800) for low light. In the shader, this should affect the **Noise Floor** and dark-region grain intensity.

### 4. S-Cinetone

A color science derived from the Sony VENICE cinema camera. It focuses on pleasing skin tones (natural mid-tones) and a "softer" highlight roll-off compared to standard Rec.709.

## 📚 Sources & Technical Resources

- **Official Specifications**: [Sony Alpha 7S III Product Page](https://www.sony.com/electronics/interchangeable-lens-cameras/ilce-7sm3)
- **Sensor Lab Tests**: [PhotonsToPhotos (Read Noise/DR)](https://www.photonstophotos.net/Charts/PDR.htm#Sony%20ILCE-7SM3)
- **Dynamic Range Analysis**: [DXOMark Sensor Review](https://www.dxomark.com/Cameras/Sony/A7S-III)
- **Color Science & Log**: [Sony Help Guide - S-Log3/S-Gamut3](https://helpguide.sony.net/di/pp/v1/en/contents/TP0000909108.html)
- **Visual Reference**: `docs/camera_references/preview_sony_a7siii_reference.jpg`

## ⌨️ Runtime Controls

- **Apply Sony A7S III Profile**: Press `F8` during runtime.
