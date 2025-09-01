import React, { useEffect, useRef, useState } from "react";
import { View, TextInput, Text, StyleSheet } from "react-native";

export default function NotesEditor({
  value,
  onChangeDebounced,
  maxChars = 2000,
  placeholder = "Add a note…",
  debounceMs = 600,
}: {
  value: string;
  onChangeDebounced: (v: string) => void;
  maxChars?: number;
  placeholder?: string;
  debounceMs?: number;
}) {
  const [text, setText] = useState(value ?? "");
  const tRef = useRef<NodeJS.Timeout | null>(null);

  useEffect(() => setText(value ?? ""), [value]);

  useEffect(() => {
    if (tRef.current) clearTimeout(tRef.current);
    tRef.current = setTimeout(() => onChangeDebounced(text), debounceMs);
    return () => { if (tRef.current) clearTimeout(tRef.current); };
  }, [text, onChangeDebounced, debounceMs]);

  const count = text.length;
  const over = count > maxChars;

  return (
    <View style={styles.wrap}>
      <TextInput
        value={text}
        onChangeText={setText}
        placeholder={placeholder}
        placeholderTextColor="#9CA3AF"
        multiline
        textAlignVertical="top"
        style={styles.input}
        maxLength={maxChars}
      />
      <Text style={[styles.counter, over && { color: "#DC2626" }]}>{count}/{maxChars}</Text>
    </View>
  );
}

const styles = StyleSheet.create({
  wrap: {
    backgroundColor: "#FFFFFF",
    borderWidth: StyleSheet.hairlineWidth,
    borderColor: "#ECE7DF",
    borderRadius: 12,
    padding: 10,
  },
  input: {
    minHeight: 100,
    color: "#111827",
    fontWeight: "700",
  },
  counter: { alignSelf: "flex-end", color: "#9CA3AF", fontSize: 11, fontWeight: "700", marginTop: 6 },
});
