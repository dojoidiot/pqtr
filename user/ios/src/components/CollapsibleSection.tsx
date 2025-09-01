import React, { useState } from "react";
import { View, Text, StyleSheet, Pressable, ViewStyle } from "react-native";
import { Ionicons } from "@expo/vector-icons";

export default function CollapsibleSection({
  title,
  initiallyOpen = false,
  children,
  style,
}: {
  title: string;
  initiallyOpen?: boolean;
  children: React.ReactNode;
  style?: ViewStyle;
}) {
  const [open, setOpen] = useState(initiallyOpen);

  return (
    <View style={[styles.wrap, style]}>
      <Pressable style={styles.header} onPress={() => setOpen(o => !o)} accessibilityRole="button" accessibilityLabel={`${title} section`}>
        <Text style={styles.title}>{title}</Text>
        <Ionicons name={open ? "chevron-up" : "chevron-down"} size={18} color="#6B7280" />
      </Pressable>
      {open && <View style={styles.content}>{children}</View>}
    </View>
  );
}

const styles = StyleSheet.create({
  wrap: { backgroundColor: "#fff", borderRadius: 16, borderWidth: StyleSheet.hairlineWidth, borderColor: "#ECE7DF" },
  header: { flexDirection: "row", alignItems: "center", justifyContent: "space-between", paddingHorizontal: 12, paddingVertical: 12 },
  title: { fontWeight: "800", color: "#111827" },
  content: { paddingHorizontal: 12, paddingBottom: 12 },
});
