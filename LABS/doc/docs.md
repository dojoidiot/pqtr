# Documentation Quality Reference

[back](../README.md)

This document serves as a reference guide for conducting documentation quality reviews of PQTR:LABS. It defines standards, provides review criteria, and offers examples for maintaining consistent, high-quality documentation.

---

## Documentation Standards

All LABS documentation maintains these standards:

- **Voice**: Declarative present tense (describes what the system does, not what it will do or should do)
- **Tense**: Present for current capabilities, conditional future ("may", "could") for out-of-scope features
- **Terminology**: Consistent use of "edit step", "module", "dial", "camera to web"
- **Structure**: Clear hierarchy, proper cross-references, logical flow from concept to implementation
- **Technical Accuracy**: All code examples, dial counts, and architectural descriptions must match implementation

---

## Review Checklist

### 1. Voice Consistency

#### ✅ CORRECT - Declarative Present Tense
Describes what the system **does**:
- ✅ "The pipe processes images through..."
- ✅ "Modules are applied in a specific order..."
- ✅ "This allows flexible workflows..."

#### ❌ AVOID - Future Tense or Imperative for Current Features
- ❌ "The pipe will process..." (future tense for existing capability)
- ❌ "You should configure..." (imperative for reference docs)

#### ⚠️ ACCEPTABLE - Passive Voice (Limited Use)
Passive voice is acceptable for:
- Process descriptions: "Once validated, the code is copied..."
- Capability statements: "New decoders can be developed..."
- Automatic operations: "Gamma encoding is applied automatically..."

### 2. Tense Consistency

#### ✅ CORRECT - Present for Current State
- ✅ "The HEAD decodes RAW files..."
- ✅ "All operations are performed in LCh color space..."

#### ✅ CORRECT - Conditional Future for Out-of-Scope
- ✅ "JPEG export could be added..."
- ✅ "These features may be reconsidered if..."

#### ❌ AVOID - Mixed Tenses
- ❌ "The pipe will process... and then saves..." (mixing future and present)

### 3. Terminology Consistency

#### Standard Terms (Use These)
- **edit step** (not "editing step", "step", or "stage")
- **module** (not "component", "processor", or "filter")
- **dial** (not "slider", "parameter", "control", or "knob")
- **Link** (named collection of modules in pipe)
- **camera to web** (project scope statement)
- **tune** (orchestrates geos + edge optimization)
- **geos** (spectral optimizer: SPSA/ACEO/HYBRID, 45 style dials, 12D features)
- **edge** (frequency loss: Laplacian variance for sharpness)
- **diff** (the comparison tool/metric)
- **HEAD → BODY → TAIL** (pipeline stages)
- **spectral loss** (geos: geodesic distance for color/tone)
- **frequency loss** (edge: Laplacian variance for sharpness)
- **tune.json** (output from tune: dials + 3D LUT, consumed by labs)
- **three roles** (color/tone, sharpness, geometry)
- **two-link architecture** (scene-linear + display links)
- **analysis** (empirical findings doc)

#### ❌ AVOID - Inconsistent Terms
- ❌ "adjustment step" instead of "edit step"
- ❌ "settings" or "sliders" instead of "dials"

### 4. Structure and Flow

#### Required Document Structure

**README.md**:
1. Project description
2. Project structure
3. Philosophy/scope
4. Components (parts)
5. Tools (programs)
6. Development tools
7. Out of scope
8. Success criteria
9. Documentation standards

**Specification docs** (pipe.md, diff.md, tune.md, test.md):
1. [back] link to parent
2. Purpose
3. Architecture/concept (for diff/tune: document both modes/algorithms)
4. Details (modules, dials, etc.)
5. Examples (configuration, usage)
6. API interface (if applicable)
7. Mode/algorithm selection guidelines (diff, tune)

**Theory docs** (geos.md):
1. [back] link to parent
2. Purpose (what theory this explains)
3. Mathematical foundation
4. Comparison with alternatives
5. Implementation considerations
6. References to implementation docs

**Test documentation** (test.md):
1. [back] link to parent
2. Test philosophy (synthetic images, mathematical verification)
3. Test structure (directory layout, Makefile targets)
4. Module test coverage (test cases per module)
5. Tool test sections:
   - Diff: spectral loss tests, frequency loss tests
   - GeoS: optimizer tests (SPSA/ACEO/HYBRID, 45 dials, 12D features)
   - Tune: integration tests, style transfer tests
6. Running tests (commands, visual inspection)

**Module docs** (geometric_adjustments.md, etc.):
1. [back] link to pipe.md
2. Purpose (one-line description)
3. Dials (table with range, default, mapping, transfer function)
4. Total dial count
5. Notes (implementation details, color space, defaults)

**Library docs** (libs.md):
1. [back] link to parent
2. Overview (what the library exposes)
3. Library details (location, size, dependencies)
4. Public headers (current and planned)
5. API reference (key types and functions)
6. Consumer example (linking, usage)
7. Build instructions

**Optimizer docs** (geos.md, edge.md):
1. [back] link to parent
2. Purpose (what this optimizer handles)
3. Core insight (key principle)
4. Mathematical foundation
5. Algorithm details
6. Implementation considerations
7. API interface
8. See Also links

**Ideas doc** (idea.md):
1. [back] link to parent
2. Brief intro (out of scope ideas)
3. Ideas with: current state, problem, approach, benefits, considerations
4. See Also links to related docs

**Analysis doc** (analysis.md):
1. [back] link to parent
2. Current architecture overview
3. Batch results (table format)
4. Pattern observations (grouped by type)
5. Optimizer tendencies
6. Research findings (regional loss, LUT covariance)
7. Cross-references to implementation docs

### 5. Technical Accuracy

#### Items to Verify

**Dial Counts** (45 style dials):

| Block | Dials | Count |
|-------|-------|-------|
| ColorCorrection | exposure, temperature, tint | 3 |
| ToneMapping | contrast, highlights, shadows, toe, shoulder, black, white | 7 |
| GlobalColor | vibrance, saturation, density | 3 |
| SplitTone | shadow_temp, shadow_tint, highlight_temp, highlight_tint | 4 |
| SelectiveColor | 8 hues × (hue, sat, lum) | 24 |
| Detail | sharpen_amount, sharpen_radius, denoise_luma, denoise_chroma | 4 |
| **Total** | | **45** |

Other (not optimized):
- Geometric: 6 dials (user-controlled composition)

**Feature Vector** (12D):
- σ₁, σ₂, σ₃ (SVD singular values)
- μ_L, μ_C (LCH means)
- std_L, std_C (LCH stds)
- skew_L (luminance skewness)
- cov_LC, cov_HC (covariances)
- μ_a, μ_b (Lab a/b means for color cast)

**Optimizers:**
- **SPSA**: Phased optimization, builds covariance
- **ACEO**: CMA-ES eigenspace using prior covariance
- **HYBRID**: ACEO for direction/pop, then SPSA for polish

**Color Spaces**:
- `SCENE_LINEAR_RGB` (camera native)
- `LINEAR_RGB` (working space, D65)
- `LCH` (perceptual adjustments)
- `SRGB` (standard output)

**Architecture Terms**:
- HEAD: RAW decoding
- BODY: Edit steps with modules
- TAIL: PNG output

**Output Format**: PNG (lossless), not JPEG

---

## Common Issues and Fixes

### Issue: Outdated References

**Problem**: References to removed/out-of-scope features
**Example**: ❌ "This is the first step after demosaicing..."
**Fix**: Update to current architecture: ✅ "This module is always placed in its own dedicated edit step..."

### Issue: Inconsistent Capability Descriptions

**Problem**: Mixing what exists with what might exist
**Example**: ❌ "The pipe supports PNG and will support JPEG..."
**Fix**: Separate clearly: ✅ "The pipe outputs PNG. JPEG support is out of scope (see out_of_scope.md)."

### Issue: Dial Count Mismatches

**Problem**: Documentation shows different total than implementation
**Example**: ❌ "17 dials total"
**Fix**: ✅ "45 style dials (3 color + 7 tone + 3 global + 4 split + 24 selective + 4 detail)" (verify against actual implementation)

### Issue: Missing Cross-References

**Problem**: Documents reference features without linking
**Example**: ❌ "See the out of scope document"
**Fix**: ✅ "See [out_of_scope.md](./mods/out_of_scope.md)"

---

## Review Process

### Step 1: Voice and Tense Review
- Read through each document checking for present tense
- Flag any future tense for current features
- Verify conditional future for out-of-scope items

### Step 2: Terminology Audit
- Search for variant terms (e.g., "parameter" vs "dial", "slider" vs "dial")
- Verify consistent usage of standard terms
- Update any inconsistencies

### Step 3: Structure Verification
- Check document hierarchy (headings, sections)
- Verify all [back] links work
- Confirm logical flow matches standards

### Step 4: Technical Accuracy Check
- Count dials in each module
- Verify code examples compile/run
- Check architectural descriptions match implementation
- Validate color space references

### Step 5: Cross-Reference Validation
- Test all internal links
- Verify all referenced documents exist
- Check that examples match actual file formats

---

## Quality Assessment Criteria

### EXCELLENT
- ✅ Perfect voice and tense consistency
- ✅ No terminology variations
- ✅ All technical details accurate
- ✅ Clear structure, all links work
- ✅ Comprehensive examples

### GOOD
- ✅ Consistent voice and tense (minor passive voice acceptable)
- ✅ Terminology mostly consistent
- ✅ Technical details accurate
- ⚠️ Minor structural improvements possible
- ⚠️ Some examples could be enhanced

### NEEDS WORK
- ❌ Mixed tenses or incorrect voice
- ❌ Inconsistent terminology
- ❌ Technical inaccuracies (wrong counts, outdated refs)
- ❌ Structural issues, broken links
- ❌ Missing or incorrect examples

---

## Reference Examples

### Good Voice Examples

**Declarative Present** (describing capability):
> The pipe processes images through a sequence of edit steps. Each step contains all 6 available modules but only enables those relevant to its purpose.

**Conditional Future** (describing out-of-scope):
> Display P3 support could be added in future expansions. This would require additional color space conversion routines.

### Good Terminology Examples

**Consistent module description**:
> The Color Correction module transforms camera-native RGB to a device-independent color space. It has 3 dials: temperature, tint, and exposure.

**Consistent architecture reference**:
> The HEAD decodes the RAW file. The BODY processes it through edit steps. The TAIL renders the final PNG.

---

## Maintenance

This reference document is updated when:
- Documentation standards change
- New terminology is introduced
- Review process is refined
- Common issues are identified

When conducting a review, reference this document as the authoritative source for standards and criteria.
