// PresetLivePreviewScreen.tsx
// Expo Go compatible live preview for Lightroom-like presets.
// Path A: Parameter uniforms (WB, exposure, contrast, tone curve, saturation).
// Path B (optional): 3D LUT (near-parity), if you provide a .cube → turn into 2D LUT texture.
//
// Install (Expo):
//   expo install expo-gl expo-blur
//   npm i gl-react gl-react-expo
//
// Use: <PresetLivePreviewScreen source={{ uri }} preset={lightroomParams} />
// Where `lightroomParams` is produced from your .xmp (example object below).

import React, { useMemo, useState } from "react";
import { View, Text, StyleSheet, TouchableOpacity, SafeAreaView } from "react-native";
import { Surface } from "gl-react-expo";
import GL from "gl-react";

// ---------- 1) GLSL SHADER (parameter-based pipeline) ----------
const PresetNode = GL.createComponent(
  ({
    children, // image texture
    exposure = 0.0,          // EV in stops
    contrast = 0.0,          // -1..+1 (0 = none)
    saturation = 0.0,        // -1..+1 (0 = none)
    temperature = 0.0,       // -1 (cool) .. +1 (warm)
    tint = 0.0,              // -1 (green) .. +1 (magenta)
    highlights = 0.0,        // -1..+1 (compress/expand brights)
    shadows = 0.0,           // -1..+1 (compress/expand darks)
    whites = 0.0,            // -1..+1 (clip/expand near 1.0)
    blacks = 0.0,            // -1..+1 (clip/expand near 0.0)
    toneCurve = [            // piecewise linear curve 5 points (x,y) in 0..1
      0.0,0.0, 0.25,0.22, 0.5,0.5, 0.75,0.78, 1.0,1.0
    ],
  }) => (
    <GL.Node
      shader={shader}
      uniforms={{
        inputImageTexture: children,
        exposure, contrast, saturation, temperature, tint,
        highlights, shadows, whites, blacks,
        toneCurve,
      }}
    />
  ),
  { displayName: "PresetNode" }
);

// GLSL helper functions are embedded for readability.
const shader = GL.Shaders.create({
  preset: {
    frag: `
precision highp float;
varying vec2 uv;
uniform sampler2D inputImageTexture;

// Global uniforms
uniform float exposure;
uniform float contrast;
uniform float saturation;
uniform float temperature;
uniform float tint;
uniform float highlights;
uniform float shadows;
uniform float whites;
uniform float blacks;
uniform float toneCurve[10]; // 5 points (x,y)*5

// --- Helpers ---
vec3 applyExposure(vec3 c, float ev) {
  // EV in stops → multiplier 2^ev
  return c * pow(2.0, ev);
}
vec3 applyContrast(vec3 c, float k) {
  // pivot 0.5
  return (c - 0.5) * (1.0 + k) + 0.5;
}
vec3 applySaturation(vec3 c, float s) {
  float l = dot(c, vec3(0.2126, 0.7152, 0.0722));
  return mix(vec3(l), c, 1.0 + s);
}
vec3 applyWhiteBalance(vec3 c, float temp, float tn) {
  // Simple approximation: temp warms/cools via R/B, tint shifts G/M via G
  // Clamp for stability
  float rShift = clamp(temp * 0.10, -0.3, 0.3);
  float bShift = clamp(-temp * 0.10, -0.3, 0.3);
  float gShift = clamp(tn   * 0.10, -0.3, 0.3);
  c.r += rShift;
  c.b += bShift;
  c.g += gShift;
  return clamp(c, 0.0, 1.0);
}
float toneCurveEval(float x) {
  // piecewise linear across 5 points
  vec2 p0 = vec2(toneCurve[0], toneCurve[1]);
  vec2 p1 = vec2(toneCurve[2], toneCurve[3]);
  vec2 p2 = vec2(toneCurve[4], toneCurve[5]);
  vec2 p3 = vec2(toneCurve[6], toneCurve[7]);
  vec2 p4 = vec2(toneCurve[8], toneCurve[9]);
  if (x <= p1.x) {
    float t = clamp((x - p0.x) / (p1.x - p0.x + 1e-6), 0.0, 1.0);
    return mix(p0.y, p1.y, t);
  } else if (x <= p2.x) {
    float t = clamp((x - p1.x) / (p2.x - p1.x + 1e-6), 0.0, 1.0);
    return mix(p1.y, p2.y, t);
  } else if (x <= p3.x) {
    float t = clamp((x - p2.x) / (p3.x - p2.x + 1e-6), 0.0, 1.0);
    return mix(p2.y, p3.y, t);
  } else {
    float t = clamp((x - p3.x) / (p4.x - p3.x + 1e-6), 0.0, 1.0);
    return mix(p3.y, p4.y, t);
  }
}
vec3 applyHighlightsShadows(vec3 c, float hi, float sh) {
  float l = dot(c, vec3(0.2126,0.7152,0.0722));
  // compress highlights: push values above 0.6 toward 0.6
  float h = smoothstep(0.6, 1.0, l);
  // lift shadows: push values below 0.4 upward
  float s = 1.0 - smoothstep(0.0, 0.4, l);
  vec3 res = c;
  res = mix(res, res * (0.85), h * max(0.0, hi));        // reduce bright
  res = mix(res, res + vec3(0.10), s * max(0.0, sh));    // lift dark
  // allow negative (expand) too
  res = mix(res, res * (1.15), h * max(0.0, -sh));       // compress darks
  res = mix(res, res + vec3(-0.10), s * max(0.0, -hi));  // deepen brights
  return clamp(res, 0.0, 1.0);
}
vec3 applyWhitesBlacks(vec3 c, float w, float b) {
  // whites: expand/compress near 1.0 ; blacks: near 0.0
  c = clamp(c, 0.0, 1.0);
  c = mix(c, smoothstep(0.0, 1.0 + w*0.4, c), 0.5);
  c = mix(c, pow(c, vec3(1.0 + b*0.6)), 0.5);
  return clamp(c, 0.0, 1.0);
}

void main() {
  vec4 tex = texture2D(inputImageTexture, uv);
  vec3 c = tex.rgb;

  // 1) Exposure
  c = applyExposure(c, exposure);

  // 2) White balance (temperature/tint)
  c = applyWhiteBalance(c, temperature, tint);

  // 3) Tone curve (global)
  c.r = toneCurveEval(c.r);
  c.g = toneCurveEval(c.g);
  c.b = toneCurveEval(c.b);

  // 4) Highlights/Shadows + Whites/Blacks
  c = applyHighlightsShadows(c, highlights, shadows);
  c = applyWhitesBlacks(c, whites, blacks);

  // 5) Contrast & Saturation
  c = applyContrast(c, contrast);
  c = applySaturation(c, saturation);

  gl_FragColor = vec4(clamp(c,0.0,1.0), tex.a);
}
    `,
  },
});

// ---------- 2) Example preset params (from XMP) ----------
type PresetParams = {
  exposure?: number;       // EV
  contrast?: number;       // -1..+1
  saturation?: number;     // -1..+1
  temperature?: number;    // -1..+1
  tint?: number;           // -1..+1
  highlights?: number;     // -1..+1
  shadows?: number;        // -1..+1
  whites?: number;         // -1..+1
  blacks?: number;         // -1..+1
  toneCurve?: number[];    // 10-length array [x0,y0, ... x4,y4]
};

const EXAMPLE_PRESET: PresetParams = {
  exposure: 0.25,          // +0.25 EV
  contrast: 0.18,
  saturation: -0.05,
  temperature: 0.20,       // warm
  tint: 0.05,              // magenta
  highlights: -0.15,
  shadows: 0.18,
  whites: 0.08,
  blacks: -0.04,
  toneCurve: [0,0, 0.25,0.22, 0.5,0.52, 0.75,0.80, 1,1],
};

// ---------- 3) The live preview screen ----------
export default function PresetLivePreviewScreen({
  source = { uri: "https://images.unsplash.com/photo-1503023345310-bd7c1de61c7d?w=1200" },
  preset = EXAMPLE_PRESET,
}: {
  source?: { uri: string };
  preset?: PresetParams;
}) {
  const uniforms = useMemo(() => {
    // Fill defaults so shader always receives defined uniforms.
    const p = { ...EXAMPLE_PRESET, ...preset };
    if (!p.toneCurve || p.toneCurve.length !== 10) p.toneCurve = EXAMPLE_PRESET.toneCurve!;
    return p;
  }, [preset]);

  const [label, setLabel] = useState("Black and White 1"); // demo UI label

  return (
    <SafeAreaView style={styles.root}>
      {/* Title block over blurred header image (keep it minimal) */}
      <View style={styles.header}>
        <Text style={styles.title}>Australian Grand Prix</Text>
        <Text style={styles.subtitle}>Peter's Sony A7iv</Text>
      </View>

      {/* GPU surface */}
      <Surface style={styles.surface} preload>
        {/* children is the image; uniforms are passed into shader */}
        <PresetNode {...uniforms}>
          {{ uri: source.uri }}
        </PresetNode>
      </Surface>

      {/* Overlay selection row (your scroll wheel could live here) */}
      <View style={styles.overlayBar}>
        <Text style={styles.presetPill}>{label}</Text>
        <View style={{ flex: 1 }} />
        <TouchableOpacity
          onPress={() => setLabel(label === "Black and White 1" ? "Black and White 2" : "Black and White 1")}
          style={styles.switchBtn}
        >
          <Text style={styles.switchText}>Try Next</Text>
        </TouchableOpacity>
      </View>

      {/* Footer guidance */}
      <Text style={styles.help}>Live preview powered by GPU. Tap "Try Next" to simulate preset swap.</Text>
    </SafeAreaView>
  );
}

// ---------- 4) Styles ----------
const styles = StyleSheet.create({
  root: { flex: 1, backgroundColor: "#0e0f10" },
  header: { paddingHorizontal: 20, paddingTop: 8, paddingBottom: 6 },
  title: { color: "white", fontSize: 22, fontWeight: "600" },
  subtitle: { color: "rgba(255,255,255,0.75)", fontSize: 15, marginTop: 2 },
  surface: { flex: 1, width: "100%" },
  overlayBar: {
    position: "absolute",
    left: 16, right: 16, bottom: 28,
    backgroundColor: "rgba(0,0,0,0.32)",
    borderRadius: 14, paddingVertical: 10, paddingHorizontal: 14,
    flexDirection: "row", alignItems: "center",
    borderWidth: StyleSheet.hairlineWidth, borderColor: "rgba(255,255,255,0.08)",
  },
  presetPill: { color: "white", fontSize: 16, fontWeight: "600" },
  switchBtn: {
    paddingVertical: 8, paddingHorizontal: 12, borderRadius: 10,
    backgroundColor: "rgba(255,255,255,0.15)",
  },
  switchText: { color: "white", fontSize: 14, fontWeight: "500" },
  help: {
    position: "absolute", bottom: 6, alignSelf: "center",
    color: "rgba(255,255,255,0.6)", fontSize: 12,
  }
});
