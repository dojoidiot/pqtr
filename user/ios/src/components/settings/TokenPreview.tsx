// components/settings/TokenPreview.tsx
// Live preview for naming rule with tokens.

import React, { useMemo } from "react";
import { View, Text, StyleSheet } from "react-native";

export default function TokenPreview({
  rule, context,
}: { rule: string; context: { project: string; date: string; tag?: string; counter: number; original?: string } }) {
  const sample = useMemo(() => {
    const map: Record<string, string | number> = {
      "{project}": context.project,
      "{date}": context.date,
      "{tag}": context.tag ?? "general",
      "{counter}": context.counter.toString().padStart(4, "0"),
      "{original}": context.original ?? "DSC_0123",
    };
    let out = rule;
    Object.entries(map).forEach(([k, v]) => { out = out.split(k).join(String(v)); });
    return out;
  }, [rule, context]);

  return (
    <View style={styles.wrap}>
      <Text style={styles.label}>Preview</Text>
      <Text style={styles.value}>{sample}.jpg</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  wrap: { marginTop: 8, backgroundColor: "#fff", borderRadius: 12, padding: 10, borderWidth: StyleSheet.hairlineWidth, borderColor: "#ECE7DF" },
  label: { color: "#6B7280", fontWeight: "700", marginBottom: 4 },
  value: { color: "#111827", fontWeight: "800" },
});
