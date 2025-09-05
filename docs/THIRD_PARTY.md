# Third-Party Components and Licenses

This document provides complete attribution for all third-party libraries and components used in MixMind AI.

## 🔧 Core Audio Engine

### Tracktion Engine
- **License**: GPLv3 / Commercial Dual License
- **Copyright**: Tracktion Corporation
- **Source**: https://github.com/Tracktion/tracktion_engine
- **Use**: Digital Audio Workstation engine, audio processing, MIDI handling
- **Note**: License choice affects entire project licensing

### JUCE Framework  
- **License**: Included with Tracktion Engine
- **Copyright**: Raw Material Software Limited
- **Source**: https://github.com/juce-framework/JUCE (bundled with Tracktion)
- **Use**: Cross-platform audio application framework, GUI components

## 🎛️ Plugin Support

### VST3 SDK
- **License**: Proprietary (Royalty-free)
- **Copyright**: Steinberg Media Technologies GmbH
- **Source**: https://github.com/steinbergmedia/vst3sdk
- **Use**: VST3 plugin hosting and processing
- **Attribution**: "VST is a trademark of Steinberg Media Technologies GmbH"

## 📊 Audio Analysis

### libebur128
- **License**: MIT
- **Copyright**: Jan Kokemüller and contributors
- **Source**: https://github.com/jiixyj/libebur128
- **Use**: EBU R128 loudness measurement and LUFS normalization
- **MIT License Text**:
```
Copyright (c) 2011 Jan Kokemüller

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
```

## 🌐 Network and Data

### cpp-httplib
- **License**: MIT
- **Copyright**: Yuji Hirose
- **Source**: https://github.com/yhirose/cpp-httplib
- **Use**: HTTP client for AI API communication
- **MIT License Text**:
```
Copyright (c) 2019 Yuji Hirose

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
```

### nlohmann/json
- **License**: MIT  
- **Copyright**: Niels Lohmann
- **Source**: https://github.com/nlohmann/json
- **Use**: JSON parsing and serialization for configuration and data exchange
- **MIT License Text**:
```
Copyright (c) 2013-2024 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
```

## 🧪 Testing Framework

### GoogleTest (gtest)
- **License**: BSD-3-Clause
- **Copyright**: Google Inc.
- **Source**: https://github.com/google/googletest  
- **Use**: Unit testing framework
- **BSD-3-Clause License**:
```
Copyright 2008, Google Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.
    * Neither the name of Google Inc. nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.
```

## 📦 Build System

### CMake Modules
- **License**: Various (BSD, MIT)
- **Use**: Build system configuration
- **Sources**: CMake community modules, custom configurations

## 🎵 Audio Content (Optional)

### Built-in Audio Samples
- **License**: Public Domain / CC0
- **Use**: Default drum samples, bass tones for built-in generators
- **Source**: Generated synthetically or sourced from public domain

## 📄 Complete License Summary

| Component | License | Commercial Use | Attribution Required | Modifications Allowed |
|-----------|---------|----------------|---------------------|----------------------|
| **Tracktion Engine** | GPLv3/Commercial | ❌ GPLv3 / ✅ Commercial | ✅ | ✅ GPLv3 / ❌ Commercial |
| **JUCE** | With Tracktion | Same as Tracktion | ✅ | Same as Tracktion |
| **VST3 SDK** | Proprietary RF | ✅ | ✅ | ❌ |
| **libebur128** | MIT | ✅ | ✅ | ✅ |
| **cpp-httplib** | MIT | ✅ | ✅ | ✅ |
| **nlohmann/json** | MIT | ✅ | ✅ | ✅ |
| **GoogleTest** | BSD-3 | ✅ | ✅ | ✅ |

## 🚨 License Compatibility Notes

### If Using GPLv3 (Tracktion Engine Free License)
- ✅ All MIT and BSD components are compatible
- ✅ VST3 SDK is compatible (proprietary but allows GPL projects)
- ❌ Cannot combine with proprietary code
- ❌ Entire project must be GPLv3

### If Using Commercial License (Tracktion Engine Paid)
- ✅ All components are compatible
- ✅ Can combine with proprietary code  
- ✅ Can choose any license for final application
- ⚠️ Must obtain Tracktion commercial license

## 📋 Attribution Requirements

### In Application About Dialog:
```
MixMind AI uses the following open source components:

• Tracktion Engine - Copyright Tracktion Corporation
• VST3 SDK - Copyright Steinberg Media Technologies GmbH
  "VST is a trademark of Steinberg Media Technologies GmbH"
• libebur128 - Copyright Jan Kokemüller (MIT License)
• nlohmann/json - Copyright Niels Lohmann (MIT License)  
• cpp-httplib - Copyright Yuji Hirose (MIT License)
```

### In Documentation:
All components must be attributed with copyright notices and license texts as shown above.

### In Source Code:
Each source file should include appropriate copyright and license headers.

## 🔗 Additional Resources

- **Tracktion Licensing**: https://www.tracktion.com/develop/tracktion-engine
- **VST Developer Portal**: https://developer.steinberg.help/
- **libebur128 Documentation**: https://github.com/jiixyj/libebur128/blob/master/README.md
- **SPDX License List**: https://spdx.org/licenses/

---

**Last Updated**: 2024-01-20  
**Review Required**: When adding new dependencies