import React, { useState } from "react";
import { View, Text, StyleSheet, Pressable, TextInput, ViewStyle } from "react-native";
import { Ionicons } from "@expo/vector-icons";

export default function TagChips({
  title = "Tags",
  tags,
  onAdd,
  onRemove,
  style,
}: {
  title?: string;
  tags: string[];
  onAdd: (t: string) => void;
  onRemove: (t: string) => void;
  style?: ViewStyle;
}) {
  const [adding, setAdding] = useState(false);
  const [text, setText] = useState("");

  const commit = () => {
    const v = text.trim();
    if (v) onAdd(v);
    setText("");
    setAdding(false);
  };

  return (
    <View style={[styles.wrap, style]}>
      <Text style={styles.title}>{title}</Text>
      <View style={styles.chips}>
        {tags.map(t => (
          <Pressable
            key={t}
            style={styles.chip}
            onLongPress={() => onRemove(t)}
            accessibilityLabel={`Tag ${t}. Long press to remove.`}
          >
            <Text style={styles.chipTxt}>{t}</Text>
          </Pressable>
        ))}
        {!adding ? (
          <Pressable style={[styles.chip, styles.addChip]} onPress={() => setAdding(true)}>
            <Ionicons name="add" size={16} color="#175E4C" />
            <Text style={[styles.chipTxt, { color: "#175E4C" }]}>Add tag</Text>
          </Pressable>
        ) : (
          <View style={[styles.inputChip]}>
            <TextInput
              value={text}
              onChangeText={setText}
              autoFocus
              placeholder="Type and return"
              placeholderTextColor="#9CA3AF"
              style={{ minWidth: 80, fontWeight: "700", color: "#111827", paddingVertical: 4 }}
              onSubmitEditing={commit}
              onBlur={() => setAdding(false)}
            />
          </View>
        )}
      </View>
      <Text style={styles.hint}>Long-press a tag to remove</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  wrap: { backgroundColor: "#fff", borderRadius: 16, padding: 12, borderWidth: StyleSheet.hairlineWidth, borderColor: "#ECE7DF" },
  title: { fontWeight: "800", color: "#111827", marginBottom: 8 },
  chips: { flexDirection: "row", flexWrap: "wrap", gap: 8 },
  chip: { backgroundColor: "#EEE8DF", paddingHorizontal: 12, paddingVertical: 8, borderRadius: 999 },
  addChip: { backgroundColor: "#EAF4EE", flexDirection: "row", alignItems: "center", gap: 6, borderWidth: 1, borderColor: "#CFE7D8" },
  inputChip: { backgroundColor: "#fff", paddingHorizontal: 10, paddingVertical: 6, borderRadius: 999, borderWidth: 1, borderColor: "#E1DED7" },
  chipTxt: { fontWeight: "800", color: "#333" },
  hint: { marginTop: 6, color: "#9CA3AF", fontSize: 11, fontWeight: "700" },
});
