# GitHub Issue: Licensing alignment (TE/JUCE vs MIT)

**Copy this content to create GitHub Issue:**

---

**Title**: Licensing alignment (TE/JUCE vs MIT)  
**Labels**: legal, licensing, blocker  
**Priority**: High  
**Assignees**: @Tonytony5278

## Overview

MixMind AI currently uses MIT License for the core application, but depends on third-party libraries (Tracktion Engine, JUCE) that may have conflicting licensing requirements. This needs resolution before broad binary distribution.

## Current Situation

### Core MixMind AI
- **License**: MIT License
- **Status**: ✅ Permits commercial and open source use
- **Distribution**: ✅ No restrictions on binary distribution

### Dependencies with Licensing Concerns

**Tracktion Engine**
- **License**: Typically GPLv3 or commercial
- **Impact**: If GPLv3, forces entire application to GPLv3
- **Usage**: Audio engine, plugin hosting, file I/O

**JUCE Framework** 
- **License**: GPLv3 or commercial  
- **Impact**: If GPLv3, forces entire application to GPLv3
- **Usage**: Audio processing, UI framework (if used)

## Legal Issue

**License Incompatibility**: MIT (permissive) vs GPLv3 (copyleft)
- MIT allows proprietary usage
- GPLv3 requires entire derivative work to be GPLv3
- Cannot distribute MIT-licensed binaries linked against GPLv3 libraries

## Resolution Options

### Option A: Full Open Source (GPLv3)
**Approach**: Switch MixMind AI to GPLv3
- ✅ **Pros**: Legal compliance, can use all dependencies freely
- ❌ **Cons**: Prevents proprietary/commercial distribution
- **Implementation**: Update LICENSE file, all headers, documentation

### Option B: Dual Licensing 
**Approach**: Keep core MIT, commercial licensing for dependencies
- ✅ **Pros**: Maintains MIT flexibility
- ❌ **Cons**: Requires purchasing commercial licenses for TE/JUCE
- **Cost**: ~$1000+ per developer for commercial licenses

### Option C: Dependency Replacement
**Approach**: Replace GPLv3 dependencies with MIT/permissive alternatives
- ✅ **Pros**: Maintains pure MIT licensing
- ❌ **Cons**: Significant engineering effort, potential feature loss
- **Examples**: Replace TE with custom audio engine, JUCE with other frameworks

### Option D: Alpha Compromise (Current)
**Approach**: Distribute core-only MIT binaries, users build full version locally
- ✅ **Pros**: Legal compliance, maintains MIT for core
- ❌ **Cons**: Poor user experience, limits adoption
- **Status**: Currently implemented for Alpha

## Recommendation

**For Alpha → Beta transition**, suggest **Option A: Full GPLv3**:

**Rationale**:
1. **Simplicity**: Clear legal compliance
2. **Community**: GPLv3 encourages open source contributions
3. **Precedent**: Many professional audio tools are GPLv3 (Ardour, etc.)
4. **Future**: Can always dual-license later if commercial needs arise

**Implementation Plan**:
1. Update LICENSE file to GPLv3
2. Add GPLv3 headers to all source files
3. Update README licensing section
4. Notify contributors about license change
5. Update CI/CD to distribute full binaries

## Alternative: Investigate Actual Licenses

**Action Items**:
- [ ] Verify actual Tracktion Engine license terms
- [ ] Verify actual JUCE license terms  
- [ ] Check if we're actually linking against GPL versions
- [ ] Investigate if commercial licenses are already available
- [ ] Contact Tracktion/JUCE for clarification

## Impact Assessment

**If we choose GPLv3**:
- ✅ Can distribute full-featured binaries
- ✅ Legal compliance ensured
- ❌ Commercial distribution requires contributor agreement
- ❌ Proprietary plugins/extensions not possible

**If we keep MIT (Option D)**:
- ✅ Maximum licensing flexibility
- ❌ Cannot distribute full-featured binaries
- ❌ Poor user experience for Alpha testing
- ❌ Limits adoption and feedback

## Timeline

**Target Resolution**: Before Beta release
**Immediate Action**: Legal research and license verification
**Decision Deadline**: Within 2 weeks of this issue creation

## Questions for Resolution

1. What are the exact license terms for our TE/JUCE usage?
2. Are we linking statically or dynamically?
3. Do we have any commercial license options?
4. What's the cost/benefit of dependency replacement?
5. Are there GPL-compatible alternatives?

## References

- [GPL vs MIT Licensing Guide](https://choosealicense.com/licenses/)
- [JUCE Licensing Information](https://juce.com/get-juce/licence)
- [Tracktion Engine Licensing](https://www.tracktion.com/develop/tracktion-engine)
- [Software License Compatibility](https://en.wikipedia.org/wiki/License_compatibility)

---

**This issue blocks broad binary distribution and must be resolved before Beta release.**