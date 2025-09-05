# MixMind AI Licensing Documentation

## Overview

This document outlines the licensing decisions for MixMind AI and its dependencies, with particular focus on the dual-license components that require careful consideration.

## 🚨 Critical Licensing Decisions Required

### 1. Tracktion Engine Licensing

**Current Status: ⚠️ DECISION REQUIRED**

Tracktion Engine is available under dual licensing:

#### Option A: GPLv3 (Free, Open Source)
- ✅ **Cost**: Free
- ✅ **Use Case**: Open source projects
- ❌ **Restriction**: Entire MixMind AI must be GPLv3
- ❌ **Commercial**: Cannot sell proprietary versions

#### Option B: Commercial License
- ✅ **Commercial**: Can create proprietary products
- ✅ **Flexibility**: Can choose any license for MixMind AI
- ❌ **Cost**: Paid licensing from Tracktion Corporation
- ❌ **Compliance**: Commercial licensing terms apply

**📋 Required Actions:**
1. Choose between GPLv3 or commercial licensing
2. Update all source file headers with chosen license
3. Add appropriate copyright notices
4. If commercial: Obtain license from [Tracktion Corporation](https://www.tracktion.com/)

**🔗 Links:**
- [Tracktion Engine Licensing](https://github.com/Tracktion/tracktion_engine/blob/develop/LICENSE.md)
- [Tracktion Corporation Contact](https://www.tracktion.com/contact)

### 2. VST3 SDK Licensing  

**Current Status: ⚠️ DECISION REQUIRED**

VST3 SDK has specific proprietary licensing requirements:

#### License Terms
- ✅ **Cost**: Royalty-free
- ✅ **Use**: Can distribute VST3-enabled applications
- ⚠️ **Attribution**: Must include Steinberg copyright notices
- ⚠️ **Trademark**: Cannot use "VST" trademark without permission
- ❌ **Modification**: SDK source cannot be modified/redistributed

**📋 Required Actions:**
1. Include Steinberg copyright notices in About dialogs
2. Add VST3 SDK attribution to documentation  
3. Follow trademark guidelines for "VST" usage
4. Consider trademark license for "VST" branding

**🔗 Links:**
- [VST3 SDK License](https://github.com/steinbergmedia/vst3sdk/blob/master/LICENSE.txt)
- [Steinberg Media](https://www.steinberg.net/)
- [VST Trademark Guidelines](https://www.steinberg.net/en/company/technologies/vst3.html)

## ✅ Resolved Dependencies  

### MIT Licensed Components

The following dependencies use MIT license (compatible with all options):

#### libebur128 (Audio Loudness Measurement)
- **License**: MIT
- **Use**: LUFS/EBU R128 loudness measurement
- **Attribution Required**: Include MIT license text
- **Source**: [libebur128 GitHub](https://github.com/jiixyj/libebur128)

#### nlohmann/json (JSON Processing)
- **License**: MIT  
- **Use**: Configuration and data serialization
- **Attribution Required**: Include MIT license text
- **Source**: [nlohmann/json GitHub](https://github.com/nlohmann/json)

#### cpp-httplib (HTTP Client)
- **License**: MIT
- **Use**: AI API communication
- **Attribution Required**: Include MIT license text
- **Source**: [cpp-httplib GitHub](https://github.com/yhirose/cpp-httplib)

## 📄 License Decision Matrix

| Component | License | Commercial OK | Attribution | Cost |
|-----------|---------|---------------|-------------|------|
| **Tracktion Engine** | GPLv3 **OR** Commercial | ❌ GPLv3 / ✅ Commercial | Copyright notices | Free / Paid |
| **VST3 SDK** | Proprietary (Royalty-Free) | ✅ | Steinberg notices | Free |
| **libebur128** | MIT | ✅ | MIT license text | Free |
| **nlohmann/json** | MIT | ✅ | MIT license text | Free |
| **cpp-httplib** | MIT | ✅ | MIT license text | Free |
| **JUCE** | Included with Tracktion | Same as Tracktion | Same as Tracktion | Same as Tracktion |

## 🎯 Recommended Licensing Approach

### For Open Source Release (GPLv3)
```
MixMind AI - Intelligent DAW Assistant
Copyright (C) 2024 [Your Name/Organization]

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program includes:
- Tracktion Engine (GPLv3) - Copyright Tracktion Corporation
- VST3 SDK (Proprietary) - Copyright Steinberg Media Technologies
- libebur128 (MIT) - Copyright Jan Kokemüller
```

### For Commercial Release
```
MixMind AI - Intelligent DAW Assistant  
Copyright (C) 2024 [Your Name/Organization]
All rights reserved.

This software includes components licensed under various terms:
- Tracktion Engine - Licensed under commercial agreement with Tracktion Corporation
- VST3 SDK - Copyright Steinberg Media Technologies (Royalty-free)
- libebur128 - MIT License
```

## 📝 Required Attribution Files

### THIRD_PARTY.md
Create `docs/THIRD_PARTY.md` with complete attribution:

```markdown
# Third-Party Components

## libebur128
- License: MIT
- Copyright: Jan Kokemüller and contributors
- Source: https://github.com/jiixyj/libebur128
- Use: EBU R128 loudness measurement

## Tracktion Engine  
- License: [GPLv3/Commercial - specify choice]
- Copyright: Tracktion Corporation
- Source: https://github.com/Tracktion/tracktion_engine
- Use: Digital Audio Workstation engine

## VST3 SDK
- License: Proprietary (Royalty-free)
- Copyright: Steinberg Media Technologies GmbH
- Source: https://github.com/steinbergmedia/vst3sdk  
- Use: VST plugin hosting
```

### Source File Headers
Add to all `.cpp/.h` files:
```cpp
/*
 * This file is part of MixMind AI
 * Copyright (C) 2024 [Your Name]
 * 
 * [License text based on decision]
 */
```

## 🔧 Implementation Checklist

### Before First Release:
- [ ] **CRITICAL**: Decide on Tracktion Engine licensing (GPLv3 vs Commercial)
- [ ] Add license headers to all source files
- [ ] Create `LICENSE` file in repository root
- [ ] Create `docs/THIRD_PARTY.md` with attributions
- [ ] Add copyright notices to About dialog/documentation
- [ ] If commercial: Obtain Tracktion commercial license
- [ ] Review VST3 trademark usage

### In Application UI:
- [ ] About dialog shows all copyright notices
- [ ] Help menu links to license information  
- [ ] VST3 branding follows Steinberg guidelines
- [ ] Third-party attributions accessible to users

### In Documentation:
- [ ] README.md mentions key licensing decisions
- [ ] Build instructions include license implications
- [ ] Distribution guidelines for different licenses

## ⚖️ Legal Considerations

### GPLv3 Implications
If choosing GPLv3 route:
- ✅ Can distribute for free
- ❌ Cannot create proprietary versions  
- ❌ Derivatives must also be GPLv3
- ⚠️ Users can modify and redistribute

### Commercial Implications  
If choosing commercial route:
- ✅ Full control over licensing
- ✅ Can create proprietary versions
- ✅ Can sell commercial licenses
- ❌ Must pay for Tracktion license
- ⚠️ More complex compliance requirements

### VST3 Compliance
Regardless of main license choice:
- ✅ Can use VST3 SDK royalty-free
- ⚠️ Must include Steinberg attributions
- ⚠️ Cannot modify VST3 SDK source
- ⚠️ Trademark restrictions on "VST" branding

## 📞 Contact Information

For licensing questions:
- **Tracktion Engine**: [support@tracktion.com](mailto:support@tracktion.com)
- **VST3 SDK**: [Steinberg Developer Support](https://developer.steinberg.help/)
- **Legal Advice**: Consult with IP attorney for commercial deployments

---

**⚠️ Important**: This document provides guidance but is not legal advice. Consult qualified legal counsel for commercial licensing decisions.