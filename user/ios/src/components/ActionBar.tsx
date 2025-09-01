import React from "react";
import { View, Text, StyleSheet, Pressable, ViewStyle } from "react-native";
import { Ionicons } from "@expo/vector-icons";

export type ActionBarAction = {
  key: string;
  icon: keyof typeof Ionicons.glyphMap;
  label: string;
  onPress: () => void;
  danger?: boolean;
};

export default function ActionBar({
  actions,
  style,
}: {
  actions: ActionBarAction[];
  style?: ViewStyle;
}) {
  return (
    <View style={[styles.wrap, style]}>
      {actions.map((a) => (
        <Pressable
          key={a.key}
          style={[styles.btn, a.danger && styles.btnDanger]}
          onPress={a.onPress}
          android_ripple={{ color: "rgba(0,0,0,0.06)", borderless: false }}
          accessibilityRole="button"
          accessibilityLabel={a.label}
        >
          <Ionicons
            name={a.icon}
            size={18}
            color={a.danger ? "#DC2626" : "#111827"}
          />
          <Text style={[styles.label, a.danger && styles.dangerText]}>{a.label}</Text>
        </Pressable>
      ))}
    </View>
  );
}

const styles = StyleSheet.create({
  wrap: {
    backgroundColor: "#fff",
    borderRadius: 16,
    paddingVertical: 10,
    paddingHorizontal: 10,
    borderWidth: StyleSheet.hairlineWidth,
    borderColor: "#ECE7DF",
    flexDirection: "row",
    justifyContent: "space-between",
    gap: 8,
  },
  btn: {
    flex: 1,
    flexDirection: "column",
    alignItems: "center",
    gap: 6,
    paddingVertical: 10,
    borderRadius: 12,
  },
  btnDanger: {
    backgroundColor: "#FFF5F5",
  },
  label: { fontWeight: "800", color: "#111827", fontSize: 12 },
  dangerText: { color: "#DC2626" },
});
