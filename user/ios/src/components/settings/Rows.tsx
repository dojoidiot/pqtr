// components/settings/Rows.tsx
// Elegant, iOS-first settings rows (Expo Go friendly)

import React from "react";
import { View, Text, StyleSheet, Pressable, Switch, TextInput, ViewStyle } from "react-native";
import { Ionicons } from "@expo/vector-icons";

export function SectionCard({ title, children, style }: { title: string; children: React.ReactNode; style?: ViewStyle }) {
  return (
    <View style={[styles.card, style]}>
      <Text style={styles.cardTitle}>{title}</Text>
      <View style={{ gap: 8 }}>{children}</View>
    </View>
  );
}

export function SwitchRow({
  label, value, onValueChange, subtitle,
}: { label: string; value: boolean; onValueChange: (v: boolean) => void; subtitle?: string; }) {
  return (
    <View style={styles.row}>
      <View style={styles.rowText}>
        <Text style={styles.label}>{label}</Text>
        {!!subtitle && <Text style={styles.subtitle}>{subtitle}</Text>}
      </View>
      <Switch value={value} onValueChange={onValueChange} />
    </View>
  );
}

export function SelectRow({
  label, valueLabel, onPress, subtitle, danger,
}: { label: string; valueLabel?: string; onPress: () => void; subtitle?: string; danger?: boolean; }) {
  return (
    <Pressable style={styles.row} onPress={onPress}>
      <View style={styles.rowText}>
        <Text style={[styles.label, danger && { color: "#DC2626" }]}>{label}</Text>
        {!!subtitle && <Text style={styles.subtitle}>{subtitle}</Text>}
      </View>
      <View style={{ flexDirection: "row", alignItems: "center", gap: 8 }}>
        {!!valueLabel && <Text style={styles.value}>{valueLabel}</Text>}
        <Ionicons name="chevron-forward" size={18} color="#9CA3AF" />
      </View>
    </Pressable>
  );
}

export function TextRow({
  label, value, onChangeText, placeholder, mono, subtitle, keyboardType,
}: { label: string; value: string; onChangeText: (t: string) => void; placeholder?: string; mono?: boolean; subtitle?: string; keyboardType?: "default" | "numeric"; }) {
  return (
    <View style={styles.row}>
      <View style={[styles.rowText, { flex: 1 }]}>
        <Text style={styles.label}>{label}</Text>
        {!!subtitle && <Text style={styles.subtitle}>{subtitle}</Text>}
        <TextInput
          value={value}
          onChangeText={onChangeText}
          placeholder={placeholder}
          placeholderTextColor="#9CA3AF"
          style={[styles.input, mono && { fontFamily: "Courier", letterSpacing: 0.25 }]}
          keyboardType={keyboardType ?? "default"}
        />
      </View>
    </View>
  );
}

export function NumberRow({
  label, value, onChangeValue, min = 0, max = 999999, step = 1, subtitle,
}: { label: string; value: number; onChangeValue: (n: number) => void; min?: number; max?: number; step?: number; subtitle?: string; }) {
  const dec = () => onChangeValue(Math.max(min, value - step));
  const inc = () => onChangeValue(Math.min(max, value + step));
  return (
    <View style={styles.row}>
      <View style={styles.rowText}>
        <Text style={styles.label}>{label}</Text>
        {!!subtitle && <Text style={styles.subtitle}>{subtitle}</Text>}
      </View>
      <View style={styles.counter}>
        <Pressable onPress={dec} style={styles.counterBtn}><Text style={styles.counterTxt}>–</Text></Pressable>
        <Text style={styles.counterVal}>{value}</Text>
        <Pressable onPress={inc} style={styles.counterBtn}><Text style={styles.counterTxt}>+</Text></Pressable>
      </View>
    </View>
  );
}

const styles = StyleSheet.create({
  card: {
    backgroundColor: "#fff",
    borderRadius: 16,
    borderWidth: StyleSheet.hairlineWidth,
    borderColor: "#ECE7DF",
    padding: 12,
  },
  cardTitle: { fontWeight: "800", color: "#111827", marginBottom: 8 },

  row: {
    minHeight: 52,
    paddingVertical: 6,
    paddingHorizontal: 6,
    borderRadius: 12,
    backgroundColor: "#FAF8F4",
    borderWidth: StyleSheet.hairlineWidth,
    borderColor: "#EFECE6",
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
  },
  rowText: { gap: 2 },
  label: { fontWeight: "800", color: "#111827" },
  subtitle: { color: "#6B7280", fontSize: 12, fontWeight: "600" },
  value: { color: "#111827", fontWeight: "800" },

  input: {
    marginTop: 6,
    backgroundColor: "#fff",
    borderRadius: 10,
    paddingHorizontal: 10,
    paddingVertical: 10,
    borderWidth: StyleSheet.hairlineWidth,
    borderColor: "#ECE7DF",
    fontWeight: "700",
    color: "#111827",
  },

  counter: { flexDirection: "row", alignItems: "center", gap: 8 },
  counterBtn: {
    width: 32, height: 32, borderRadius: 8, backgroundColor: "#EDE8E1",
    alignItems: "center", justifyContent: "center",
  },
  counterTxt: { fontWeight: "800", color: "#111827", fontSize: 16, marginTop: -1 },
  counterVal: { width: 56, textAlign: "center", fontWeight: "800", color: "#111827" },
});
