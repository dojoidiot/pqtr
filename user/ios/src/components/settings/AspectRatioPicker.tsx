// components/settings/AspectRatioPicker.tsx
// Toggle chips with tiny thumbnail frames to preview crop geometry.

import React from "react";
import { View, Text, StyleSheet, Pressable } from "react-native";

export default function AspectRatioPicker({
  selected, onToggle,
}: { selected: string[]; onToggle: (key: string) => void; }) {
  const ALL = ["1:1","4:5","3:2","16:9","9:16"];
  return (
    <View style={{ flexDirection: "row", flexWrap: "wrap", gap: 8 }}>
      {ALL.map(r => {
        const on = selected.includes(r);
        return (
          <Pressable key={r} onPress={() => onToggle(r)} style={[styles.chip, on && styles.on]}>
            <View style={styles.frame}>
              <View style={[styles.crop, frameStyle(r)]} />
            </View>
            <Text style={[styles.txt, on && styles.txtOn]}>{r}</Text>
          </Pressable>
        );
      })}
    </View>
  );
}

function frameStyle(r: string) {
  // frame is fixed 26x18; scale crop box to aspect
  const W = 26, H = 18;
  const [a, b] = r.split(":").map(Number);
  const target = a / b;
  const frame = W / H;
  if (target > frame) {
    // wider: full width, reduced height
    const h = H * (frame / target);
    return { width: W - 2, height: Math.max(8, h) };
  } else {
    // taller: full height, reduced width
    const w = W * (target / frame);
    return { width: Math.max(8, w), height: H - 2 };
  }
}

const styles = StyleSheet.create({
  chip: {
    flexDirection: "row", alignItems: "center", gap: 8,
    backgroundColor: "#FAF8F4", borderRadius: 999, paddingHorizontal: 10, paddingVertical: 8,
    borderWidth: StyleSheet.hairlineWidth, borderColor: "#EFECE6",
  },
  on: { backgroundColor: "#EAF4EE", borderColor: "#CFE7D8" },
  txt: { fontWeight: "800", color: "#111827", fontSize: 12 },
  txtOn: { color: "#175E4C" },

  frame: { width: 26, height: 18, borderRadius: 4, backgroundColor: "#EEE8DF", alignItems: "center", justifyContent: "center" },
  crop: { backgroundColor: "#D7D0C6", borderRadius: 2 },
});
