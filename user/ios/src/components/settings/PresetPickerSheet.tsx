// components/settings/PresetPickerSheet.tsx
// Minimal in-page sheet to choose default preset and build a chain.

import React, { useState } from "react";
import { View, Text, StyleSheet, Modal, Pressable, FlatList } from "react-native";

type Preset = { id: string; name: string };
const MOCK_PRESETS: Preset[] = [
  { id: "p1", name: "PQTR – Clean Film" },
  { id: "p2", name: "PQTR – Contrast Punch" },
  { id: "p3", name: "PQTR – Warm Skin" },
  { id: "p4", name: "PQTR – Night Boost" },
];

export default function PresetPickerSheet({
  visible, onClose, onPick, initialChain = [],
}: {
  visible: boolean;
  onClose: () => void;
  onPick: (primaryId: string | null, chain: string[]) => void;
  initialChain?: string[];
}) {
  const [sel, setSel] = useState<string | null>(initialChain[0] ?? null);
  const [chain, setChain] = useState<string[]>(initialChain);

  const toggleChain = (id: string) => {
    setChain(prev => (prev.includes(id) ? prev.filter(x => x !== id) : [...prev, id]));
  };

  return (
    <Modal transparent animationType="fade" visible={visible} onRequestClose={onClose}>
      <Pressable style={styles.backdrop} onPress={onClose} />
      <View style={styles.sheet}>
        <Text style={styles.title}>Select default preset</Text>
        <FlatList
          data={MOCK_PRESETS}
          keyExtractor={i => i.id}
          ItemSeparatorComponent={() => <View style={{ height: 8 }} />}
          renderItem={({ item }) => (
            <Pressable
              style={[styles.row, sel === item.id && styles.active]}
              onPress={() => setSel(item.id)}
              onLongPress={() => toggleChain(item.id)}
            >
              <Text style={styles.presetName}>{item.name}</Text>
              {chain.includes(item.id) && <Text style={styles.chain}>in chain</Text>}
            </Pressable>
          )}
          style={{ maxHeight: 280 }}
        />
        <View style={styles.actions}>
          <Pressable style={[styles.btn, styles.ghost]} onPress={onClose}><Text style={styles.btnTxtGhost}>Cancel</Text></Pressable>
          <Pressable
            style={[styles.btn, styles.primary]}
            onPress={() => { onPick(sel, chain); onClose(); }}
          >
            <Text style={styles.btnTxt}>Set</Text>
          </Pressable>
        </View>
        <Text style={styles.hint}>Tip: long-press a preset to add/remove it from the chain order.</Text>
      </View>
    </Modal>
  );
}

const styles = StyleSheet.create({
  backdrop: { ...StyleSheet.absoluteFillObject, backgroundColor: "rgba(0,0,0,0.35)" },
  sheet: {
    position: "absolute", left: 12, right: 12, bottom: 18,
    backgroundColor: "#fff", borderRadius: 16, padding: 12,
    borderWidth: StyleSheet.hairlineWidth, borderColor: "#ECE7DF",
  },
  title: { fontWeight: "800", color: "#111827", marginBottom: 10 },
  row: { backgroundColor: "#FAF8F4", borderRadius: 10, padding: 12, borderWidth: StyleSheet.hairlineWidth, borderColor: "#EFECE6" },
  active: { borderColor: "#175E4C" },
  presetName: { fontWeight: "800", color: "#111827" },
  chain: { marginTop: 4, color: "#175E4C", fontWeight: "800", fontSize: 12 },
  actions: { flexDirection: "row", gap: 8, marginTop: 10 },
  btn: { flex: 1, paddingVertical: 12, borderRadius: 12, alignItems: "center" },
  ghost: { backgroundColor: "#F3F1EC" },
  primary: { backgroundColor: "#175E4C" },
  btnTxt: { color: "#fff", fontWeight: "800" },
  btnTxtGhost: { color: "#175E4C", fontWeight: "800" },
  hint: { marginTop: 8, color: "#6B7280", fontSize: 12, fontWeight: "600" },
});
