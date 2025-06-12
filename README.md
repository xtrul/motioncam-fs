# MotionCam Virtual File System

Work in progress

## Calibration profiles and camera settings

Two helper loaders are available for JSON configuration files:

```cpp
#include "CalibrationProfile.h"
#include "CameraSettings.h"

auto profiles = motioncam::loadCalibrationProfilesFromFile("camera-matrix.json");
auto settings = motioncam::loadCameraSettingsFromFile("camera-name.json");
```

These functions parse the provided JSON files into in-memory maps. The example
`camera-matrix.json` and `camera-name.json` files in the repository illustrate
the expected structure.

The main window now lets you choose these JSON files using two "Browse" buttons
for camera settings and calibration profiles.
