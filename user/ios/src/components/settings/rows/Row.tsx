import React, { useState } from "react";
import { View, Text, Pressable, StyleSheet } from "react-native";
import { colors, space, text as T } from "../../../theme/tokens";
import { Ionicons } from "@expo/vector-icons";

export function RowTitle({ title, help }: { title: string; help?: string }) {
  return (
    <View style={{ padding: space.lg, borderBottomWidth: 1, borderBottomColor: colors.stroke }}>
      <Text style={T.label}>{title}</Text>
      {!!help && <Text style={[T.help, { marginTop: 4 }]}>{help}</Text>}
    </View>
  );
}

export function RowToggle({
  title, help, value, onValueChange, right,
}: {
  title: string; help?: string; value: boolean; onValueChange: (v: boolean) => void; right?: React.ReactNode;
}) {
  return (
    <View style={styles.row}>
      <View style={{ flex: 1 }}>
        <Text style={T.label}>{title}</Text>
        {!!help && <Text style={[T.help, { marginTop: 2 }]}>{help}</Text>}
      </View>
      {right}
      <Pressable
        accessibilityRole="switch"
        accessibilityState={{ checked: value }}
        onPress={() => onValueChange(!value)}
        style={[
          styles.switch,
          { backgroundColor: value ? colors.brand : "#E9E7E1", justifyContent: value ? "flex-end" : "flex-start" },
        ]}
      >
        <View style={styles.knob} />
      </Pressable>
    </View>
  );
}

export function RowStepper({
  title, value, onChange, min = Number.MIN_SAFE_INTEGER, max = Number.MAX_SAFE_INTEGER,
}: {
  title: string; value: number; onChange: (v: number) => void; min?: number; max?: number;
}) {
  const dec = () => onChange(Math.max(min, value - 1));
  const inc = () => onChange(Math.min(max, value + 1));
  return (
    <View style={styles.row}>
      <Text style={T.label}>{title}</Text>
      <View style={{ flexDirection: "row", gap: 12, alignItems: "center" }}>
        <Pressable onPress={dec} style={styles.stepBtn}><Ionicons name="remove" size={18} color={colors.text} /></Pressable>
        <Text style={T.body}>{value}</Text>
        <Pressable onPress={inc} style={styles.stepBtn}><Ionicons name="add" size={18} color={colors.text} /></Pressable>
      </View>
    </View>
  );
}

export function RowTextField({
  title, value, onChangeText, placeholder,
}: {
  title: string; value: string; onChangeText: (v: string) => void; placeholder?: string;
}) {
  return (
    <View style={styles.rowCol}>
      <Text style={[T.label, { marginBottom: 6 }]}>{title}</Text>
      <View style={styles.input}>
        <Text
          style={[T.body, { color: value ? colors.text : colors.subtext }]}
          // SIMPLE editable using contentEditable-like approach:
          onPress={() => {}}
        >
          {value || placeholder || ""}
        </Text>
      </View>
    </View>
  );
}

export function RowChipGroup<T extends string>({
  title, options, value, onToggle, multi = true,
}: {
  title: string; options: T[]; value: T[]; onToggle: (opt: T) => void; multi?: boolean;
}) {
  return (
    <View style={styles.rowCol}>
      <Text style={[T.label, { marginBottom: 6 }]}>{title}</Text>
      <View style={{ flexDirection: "row", flexWrap: "wrap", gap: 8 }}>
        {options.map((opt) => {
          const active = value.includes(opt);
          return (
            <Pressable
              key={opt}
              onPress={() => onToggle(opt)}
              style={[
                styles.chip,
                active && { backgroundColor: colors.brandSoft, borderColor: colors.brand },
              ]}
            >
              <Text style={[T.body, { fontWeight: "700", color: active ? colors.brand : colors.text }]}>{opt}</Text>
            </Pressable>
          );
        })}
      </View>
    </View>
  );
}

export function RowPicker({
  title, value, onPress,
}: {
  title: string; value: string; onPress: () => void;
}) {
  return (
    <Pressable onPress={onPress} style={[styles.row, { paddingVertical: space.lg }]}>
      <Text style={T.label}>{title}</Text>
      <View style={{ flexDirection: "row", alignItems: "center", gap: 8 }}>
        <Text style={[T.body, { color: colors.subtext }]}>{value}</Text>
        <Ionicons name="chevron-forward" size={18} color={colors.subtext} />
      </View>
    </Pressable>
  );
}

export function RowDropdown<T extends string>({
  title, value, options, onValueChange, help,
}: {
  title: string; value: T; options: T[]; onValueChange: (v: T) => void; help?: string;
}) {
  const [isOpen, setIsOpen] = useState(false);
  
  return (
    <View style={styles.rowCol}>
      <Text style={T.label}>{title}</Text>
      {!!help && <Text style={[T.help, { marginTop: 2, marginBottom: 8 }]}>{help}</Text>}
      <Pressable 
        onPress={() => setIsOpen(!isOpen)} 
        style={[styles.dropdown, { borderColor: isOpen ? colors.brand : colors.stroke }]}
      >
        <Text style={[T.body, { color: value ? colors.text : colors.subtext }]}>{value}</Text>
        <Ionicons 
          name={isOpen ? "chevron-up" : "chevron-down"} 
          size={18} 
          color={colors.subtext} 
        />
      </Pressable>
      
      {isOpen && (
        <View style={styles.dropdownOptions}>
          {options.map((option) => (
            <Pressable
              key={option}
              onPress={() => {
                onValueChange(option);
                setIsOpen(false);
              }}
              style={[
                styles.dropdownOption,
                option === value && { backgroundColor: colors.brandSoft, borderColor: colors.brand }
              ]}
            >
              <Text style={[
                T.body, 
                { 
                  fontWeight: option === value ? "700" : "400",
                  color: option === value ? colors.brand : colors.text 
                }
              ]}>
                {option}
              </Text>
              {option === value && (
                <Ionicons name="checkmark" size={18} color={colors.brand} />
              )}
            </Pressable>
          ))}
        </View>
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  row: {
    paddingHorizontal: space.lg, paddingVertical: space.md,
    borderBottomWidth: 1, borderBottomColor: colors.stroke,
    flexDirection: "row", alignItems: "center", gap: 12,
  },
  rowCol: {
    paddingHorizontal: space.lg, paddingVertical: space.md,
    borderBottomWidth: 1, borderBottomColor: colors.stroke,
  },
  switch: { width: 54, height: 32, borderRadius: 16, padding: 3 },
  knob: { width: 26, height: 26, borderRadius: 13, backgroundColor: colors.surface },
  stepBtn: {
    width: 32, height: 32, borderRadius: 8, alignItems: "center", justifyContent: "center",
    borderWidth: 1, borderColor: colors.stroke, backgroundColor: colors.surface,
  },
  input: {
    height: 44, borderRadius: 12, borderWidth: 1, borderColor: colors.stroke,
    backgroundColor: colors.surface, paddingHorizontal: 12, justifyContent: "center",
  },
  chip: {
    paddingHorizontal: 12, height: 36, borderRadius: 18,
    borderWidth: 1, borderColor: colors.stroke, alignItems: "center", justifyContent: "center",
    backgroundColor: colors.surface,
  },
  dropdown: {
    height: 44, borderRadius: 12, borderWidth: 1, borderColor: colors.stroke,
    backgroundColor: colors.surface, paddingHorizontal: 12, justifyContent: "space-between",
    alignItems: "center", flexDirection: "row",
  },
  dropdownOptions: {
    position: "absolute",
    top: "100%",
    left: 0,
    right: 0,
    backgroundColor: colors.surface,
    borderRadius: 12,
    borderWidth: 1,
    borderColor: colors.stroke,
    marginTop: 8,
    zIndex: 1,
  },
  dropdownOption: {
    paddingHorizontal: 12,
    paddingVertical: 12,
    flexDirection: "row",
    alignItems: "center",
    justifyContent: "space-between",
  },
});
